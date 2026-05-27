#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include "GoogleDriveUploader.h"




class GoogleDriveUploadQueue {
public:
    GoogleDriveUploadQueue() = default;
    ~GoogleDriveUploadQueue() { Stop(); }

    bool Start() {
        if (m_worker.joinable()) return false;
        m_running = true;
        m_worker = std::thread(&GoogleDriveUploadQueue::WorkerLoop, this);
        return true;
    }

    void Stop() {
        m_running = false;
        m_cv.notify_all();
        if (m_worker.joinable())
            m_worker.join();
    }

    void Enqueue(const std::wstring& localPath,
                 const std::wstring& parentFolderId,
                 UploadCallback callback);

private:
    struct Task {
        std::wstring localPath;
        std::wstring parentFolderId;
        UploadCallback callback;
        
        Task(const std::wstring& path, const std::wstring& folder, UploadCallback cb)
            : localPath(path), parentFolderId(folder), callback(cb) {}
    };

    void WorkerLoop();

    std::queue<Task> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_running{false};
};