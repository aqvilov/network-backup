#include "../include/GoogleDriveUploadQueue.h"
#include "../include/GoogleDriveUploader.h"  // для вызова UploadFileSync

void GoogleDriveUploadQueue::Enqueue(const std::wstring& localPath,
                                     const std::wstring& parentFolderId,
                                     UploadCallback callback)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace(localPath, parentFolderId, callback);
    }
    m_cv.notify_one();
}

void GoogleDriveUploadQueue::WorkerLoop() {
    while (m_running) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
            if (!m_running && m_queue.empty()) break;
            if (m_queue.empty()) continue;
            task = std::move(m_queue.front());
            m_queue.pop();
        }
        // Выполняем синхронную загрузку (будет реализована позже)
        GoogleDriveUploader::UploadFileSync(task.localPath, task.parentFolderId, task.callback);
    }
}