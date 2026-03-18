#pragma once
#include "interfaces/IWatcher.h"
#include "interfaces/IUploader.h"
#include "interfaces/IDelta.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace backup {

enum class BackupStatus {
    Idle,
    Watching,
    Uploading,
    Error,
    Paused
};

struct BackupTask {
    std::string filePath;
    uint64_t    enqueuedAt;
};

struct BackupStats {
    uint64_t filesUploaded   = 0;
    uint64_t filesSkipped    = 0;
    uint64_t bytesUploaded   = 0;
    uint64_t errors          = 0;
    BackupStatus status      = BackupStatus::Idle;
    std::string lastError;
    std::string currentFile;
    float  currentFilePercent = 0.f;
};

class BackupEngine {
public:
    using StatsCallback = std::function<void(const BackupStats&)>;

    BackupEngine(std::shared_ptr<IWatcher>  watcher,
                 std::shared_ptr<IUploader> uploader,
                 std::shared_ptr<IDelta>    delta);
    ~BackupEngine();

    // Управление отслеживаемыми путями
    bool addWatchPath(const std::string& path);
    bool removeWatchPath(const std::string& path);
    std::vector<std::string> watchedPaths() const;

    // Запуск / остановка
    bool start();
    void stop();
    void pause();
    void resume();

    // Ручной запуск полного бэкапа
    void triggerFullBackup();

    // Колбэк для UI — вызывается из рабочего потока
    void setStatsCallback(StatsCallback cb);

    BackupStats currentStats() const;

private:
    void workerLoop();
    void processTask(const BackupTask& task);
    void enqueue(const std::string& path);
    void onFileEvent(const FileEvent& event);

    std::shared_ptr<IWatcher>  m_watcher;
    std::shared_ptr<IUploader> m_uploader;
    std::shared_ptr<IDelta>    m_delta;

    std::thread             m_workerThread;
    std::atomic<bool>       m_running{false};
    std::atomic<bool>       m_paused{false};

    mutable std::mutex      m_queueMutex;
    std::condition_variable m_cv;
    std::queue<BackupTask>  m_taskQueue;

    mutable std::mutex      m_statsMutex;
    BackupStats             m_stats;
    StatsCallback           m_statsCallback;

    // дедупликация: не ставить в очередь один файл дважды
    std::mutex              m_pendingMutex;
    std::vector<std::string> m_pendingPaths;
};

} // namespace backup
