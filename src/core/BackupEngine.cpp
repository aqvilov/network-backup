#include "BackupEngine.h"
#include <chrono>
#include <algorithm>
#include <iostream>

namespace backup {

BackupEngine::BackupEngine(std::shared_ptr<IWatcher>  watcher,
                           std::shared_ptr<IUploader> uploader,
                           std::shared_ptr<IDelta>    delta)
    : m_watcher(std::move(watcher))
    , m_uploader(std::move(uploader))
    , m_delta(std::move(delta))
{
    m_watcher->setCallback([this](const FileEvent& ev) {
        onFileEvent(ev);
    });
}

BackupEngine::~BackupEngine() {
    stop();
}

bool BackupEngine::addWatchPath(const std::string& path) {
    return m_watcher->addPath(path);
}

bool BackupEngine::removeWatchPath(const std::string& path) {
    return m_watcher->removePath(path);
}

std::vector<std::string> BackupEngine::watchedPaths() const {
    return m_watcher->watchedPaths();
}

bool BackupEngine::start() {
    if (m_running.load()) return false;

    if (!m_uploader->isAuthenticated()) {
        if (!m_uploader->authenticate()) {
            std::lock_guard<std::mutex> lk(m_statsMutex);
            m_stats.status    = BackupStatus::Error;
            m_stats.lastError = "Google Drive authentication failed";
            return false;
        }
    }

    m_running = true;
    m_paused  = false;
    m_workerThread = std::thread(&BackupEngine::workerLoop, this);
    m_watcher->start();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.status = BackupStatus::Watching;
    }
    return true;
}

void BackupEngine::stop() {
    if (!m_running.load()) return;
    m_running = false;
    m_paused  = false;
    m_cv.notify_all();
    m_watcher->stop();
    if (m_workerThread.joinable())
        m_workerThread.join();
}

void BackupEngine::pause() {
    m_paused = true;
    std::lock_guard<std::mutex> lk(m_statsMutex);
    m_stats.status = BackupStatus::Paused;
}

void BackupEngine::resume() {
    m_paused = false;
    m_cv.notify_all();
    std::lock_guard<std::mutex> lk(m_statsMutex);
    m_stats.status = BackupStatus::Watching;
}

void BackupEngine::triggerFullBackup() {
    for (auto& path : m_watcher->watchedPaths())
        enqueue(path);
}

void BackupEngine::setStatsCallback(StatsCallback cb) {
    std::lock_guard<std::mutex> lk(m_statsMutex);
    m_statsCallback = std::move(cb);
}

BackupStats BackupEngine::currentStats() const {
    std::lock_guard<std::mutex> lk(m_statsMutex);
    return m_stats;
}

// ─── private ──────────────────────────────────────────────────────────────

void BackupEngine::onFileEvent(const FileEvent& event) {
    if (event.type == FileEventType::Deleted) return; // удаление — отдельная логика
    enqueue(event.path);
}

void BackupEngine::enqueue(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_pendingMutex);
    // дедупликация: не добавляем тот же путь дважды
    if (std::find(m_pendingPaths.begin(), m_pendingPaths.end(), path)
            != m_pendingPaths.end()) return;

    m_pendingPaths.push_back(path);
    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> qlk(m_queueMutex);
        m_taskQueue.push({ path, ts });
    }
    m_cv.notify_one();
}

void BackupEngine::workerLoop() {
    while (m_running.load()) {
        BackupTask task;
        {
            std::unique_lock<std::mutex> lk(m_queueMutex);
            m_cv.wait(lk, [this] {
                return !m_taskQueue.empty() || !m_running.load();
            });
            if (!m_running.load()) break;
            if (m_paused.load())   continue;
            task = m_taskQueue.front();
            m_taskQueue.pop();
        }

        // убираем из pending
        {
            std::lock_guard<std::mutex> lk(m_pendingMutex);
            auto it = std::find(m_pendingPaths.begin(), m_pendingPaths.end(), task.filePath);
            if (it != m_pendingPaths.end()) m_pendingPaths.erase(it);
        }

        processTask(task);
    }
}

void BackupEngine::processTask(const BackupTask& task) {
    auto notifyStats = [this]() {
        StatsCallback cb;
        BackupStats   snap;
        {
            std::lock_guard<std::mutex> lk(m_statsMutex);
            cb   = m_statsCallback;
            snap = m_stats;
        }
        if (cb) cb(snap);
    };

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.status      = BackupStatus::Uploading;
        m_stats.currentFile = task.filePath;
        m_stats.currentFilePercent = 0.f;
    }
    notifyStats();

    // Шаг 1: проверить дельту
    DeltaResult delta;
    try {
        delta = m_delta->prepare(task.filePath);
    } catch (const std::exception& ex) {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.errors++;
        m_stats.lastError = ex.what();
        m_stats.status    = BackupStatus::Watching;
        notifyStats();
        return;
    }

    if (!delta.hasChanges) {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.filesSkipped++;
        m_stats.status = BackupStatus::Watching;
        notifyStats();
        return;
    }

    // Шаг 2: загрузить
    bool ok = m_uploader->uploadFile(
        task.filePath,
        "backup/" + task.filePath,
        [this, &notifyStats](const UploadProgress& p) {
            {
                std::lock_guard<std::mutex> lk(m_statsMutex);
                m_stats.currentFilePercent = p.percent();
            }
            notifyStats();
        }
    );

    std::lock_guard<std::mutex> lk(m_statsMutex);
    if (ok) {
        m_stats.filesUploaded++;
        m_stats.bytesUploaded += delta.deltaSize;
    } else {
        m_stats.errors++;
        m_stats.lastError = "Upload failed: " + task.filePath;
    }
    m_stats.status = BackupStatus::Watching;
    m_stats.currentFile.clear();
}

} // namespace backup
