#pragma once
//очередь задач на копирование

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include "FileUtils.h"
#include "Logger.h"

using ResultCallback = std::function<void(const std::wstring& path,
                                          bool success,
                                          uint64_t bytes,
                                          const std::wstring& errorMsg)>;

class BackupQueue {
public:
    BackupQueue() = default;
    ~BackupQueue() { Stop(); }

    // Запустить рабочий поток
    bool Start(const std::wstring& watchRoot,
               const std::wstring& destDir,
               ResultCallback onResult)
    {
        if (m_worker.joinable()) return false;  // уже запущен
        m_watchRoot = watchRoot;
        m_destDir   = destDir;
        m_onResult  = onResult;
        m_running   = true;
        m_worker    = std::thread(&BackupQueue::WorkerLoop, this);
        return true;
    }

    void Stop() {
        m_running = false;
        m_cv.notify_all();
        if (m_worker.joinable())
            m_worker.join();
    }

    void Enqueue(const std::wstring& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pending.count(filePath)) return; // КОНТРИМ УЖЕ ДОБАВЛЕННЫЕ ФАЙЛЫ
        m_pending.insert(filePath);
        m_queue.push(filePath);
        m_cv.notify_one();
    }

    struct Stats {
        uint64_t copied  = 0;  
        uint64_t skipped = 0;  
        uint64_t errors  = 0;  
        uint64_t bytes   = 0;  
        size_t   queued  = 0;  
    };

    Stats GetStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        Stats s   = m_stats;
        s.queued  = m_queue.size();
        return s;
    }

private:
    void WorkerLoop() {
        while (m_running.load()) {
            std::wstring filePath;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] {
                    return !m_queue.empty() || !m_running.load();
                });
                if (!m_running.load() && m_queue.empty()) break;
                if (m_queue.empty()) continue;

                filePath = m_queue.front();
                m_queue.pop();
                m_pending.erase(filePath);
            }

            auto result = FileUtils::CopyToBackup(filePath, m_watchRoot, m_destDir);
            std::wstring errorMessage;  

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (result.success) {
                    m_stats.copied++;
                    m_stats.bytes += result.bytesCopied;
                    Logger::Info(L"Скопирован: " + filePath + L" (" + FileUtils::FormatSize(result.bytesCopied) + L")");
                    errorMessage = L"";  // НОВОЕ: при успехе - пустая строка
                } 
                else if (result.error.find(L"Пропущен") != std::wstring::npos) {
                    m_stats.skipped++;
                    errorMessage = result.error;  
                } 
                else {
                    m_stats.errors++;
                    Logger::Error(L"Ошибка копирования: " + filePath + L" — " + result.error);
                    errorMessage = result.error;  
                }
            }

            if (m_onResult)
                m_onResult(filePath, result.success, result.bytesCopied, errorMessage);
        }
    }

    std::wstring    m_watchRoot;
    std::wstring    m_destDir;
    ResultCallback  m_onResult;

    mutable std::mutex       m_mutex;
    std::condition_variable  m_cv;
    std::queue<std::wstring> m_queue;
    std::unordered_set<std::wstring> m_pending; 

    std::thread       m_worker;
    std::atomic<bool> m_running{false};
    Stats             m_stats;
};