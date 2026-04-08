#pragma once
// Watcher.h — слежка за папкой через ReadDirectoryChangesW
// Запускается в отдельном потоке, вызывает колбэк при изменении файлов

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

enum class FileAction {
    Added,    // файл создан
    Modified, // файл изменён
    Deleted,  // файл удалён
    Renamed   // файл переименован
};

// Колбэк: вызывается при каждом событии
using WatchCallback = std::function<void(FileAction action, const std::wstring& path)>;

class Watcher {
public:
    Watcher() = default;
    ~Watcher() { Stop(); }

    // Запустить слежку за папкой
    // watchPath  — папка за которой следим
    // recursive  — следить и в подпапках тоже
    // callback   — функция которую вызываем при изменении
    bool Start(const std::wstring& watchPath,
               bool recursive,
               WatchCallback callback)
    {
        if (m_running.load()) return false;

        m_path     = watchPath;
        m_recursive = recursive;
        m_callback  = callback;

        // Проверяем что папка существует
        if (GetFileAttributesW(watchPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            return false;

        m_running = true;
        m_thread  = std::thread(&Watcher::Loop, this);
        return true;
    }

    void Stop() {
        m_running = false;
        // Пробуждаем поток если он заблокирован в ReadDirectoryChangesW
        if (m_hDir != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_hDir, nullptr);
        }
        if (m_thread.joinable())
            m_thread.join();
    }

    bool IsRunning() const { return m_running.load(); }

private:
    void Loop() {
        m_hDir = CreateFileW(
            m_path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (m_hDir == INVALID_HANDLE_VALUE) {
            m_running = false;
            return;
        }

        alignas(DWORD) char buffer[65536];
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        while (m_running.load()) {
            DWORD bytesReturned = 0;

            BOOL ok = ReadDirectoryChangesW(
                m_hDir,
                buffer,
                sizeof(buffer),
                m_recursive ? TRUE : FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME  |  // создание/удаление/переименование
                FILE_NOTIFY_CHANGE_LAST_WRITE |  // изменение содержимого
                FILE_NOTIFY_CHANGE_SIZE,         // изменение размера
                &bytesReturned,
                &overlapped,
                nullptr
            );

            if (!ok) break;
            DWORD wait = WaitForSingleObject(overlapped.hEvent, 500);

            if (!m_running.load()) break;
            if (wait == WAIT_TIMEOUT) continue;
            if (wait != WAIT_OBJECT_0) break;

            // Получаем результат
            if (!GetOverlappedResult(m_hDir, &overlapped, &bytesReturned, FALSE))
                break;
            if (bytesReturned == 0) continue;

            ResetEvent(overlapped.hEvent);

            FILE_NOTIFY_INFORMATION* info =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

            while (true) {
                std::wstring filename(
                    info->FileName,
                    info->FileNameLength / sizeof(wchar_t)
                );
                std::wstring fullPath = m_path + L"\\" + filename;

                FileAction action;
                bool shouldNotify = true;

                switch (info->Action) {
                    case FILE_ACTION_ADDED:            action = FileAction::Added;    break;
                    case FILE_ACTION_MODIFIED:         action = FileAction::Modified; break;
                    case FILE_ACTION_REMOVED:          action = FileAction::Deleted;  break;
                    case FILE_ACTION_RENAMED_NEW_NAME: action = FileAction::Renamed;  break;
                    default: shouldNotify = false; break;
                }

                
                
                //if (shouldNotify && m_callback) {
                //   Sleep(150);
                //    m_callback(action, fullPath);
                //}
                //
                // Убираем задержку.
                if (shouldNotify && m_callback) 
                {
                    m_callback(action, fullPath);
                }


                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(info) + info->NextEntryOffset
                );
            }
        }

        CloseHandle(overlapped.hEvent);
        CloseHandle(m_hDir);
        m_hDir    = INVALID_HANDLE_VALUE;
        m_running = false;
    }

    std::wstring   m_path;
    bool           m_recursive = true;
    WatchCallback  m_callback;
    std::thread    m_thread;
    std::atomic<bool> m_running{false};
    HANDLE         m_hDir = INVALID_HANDLE_VALUE;
};
