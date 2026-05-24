#pragma once
// Logger.h — запись событий в файл и в память (для UI)
// Потокобезопасен: можно вызывать из любого потока

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

class Logger {
public:
    // Уровни важности сообщения
    enum class Level { Info, Warning, Error };

    static void Init(const std::wstring& logFilePath) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_file.open(std::filesystem::path(logFilePath), std::ios::app);
    }

    // зааписать сообщение
    static void Write(Level level, const std::wstring& message) {
        std::wstring line = Timestamp() + L"  " + LevelStr(level) + L"  " + message;

        std::lock_guard<std::mutex> lock(s_mutex);

        // В файл
        if (s_file.is_open())
            s_file << line << L"\n", s_file.flush();

        // В память (UI читает отсюда)
        s_lines.push_back(line);
        if (s_lines.size() > 1000)// не храним больше 1000 строк
            s_lines.erase(s_lines.begin());
    }

    // Удобные обертки
    static void Info   (const std::wstring& m) { Write(Level::Info,    m); }
    static void Warning(const std::wstring& m) { Write(Level::Warning, m); }
    static void Error  (const std::wstring& m) { Write(Level::Error,   m); }

    static std::vector<std::wstring> GetLines() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_lines;
    }

    static size_t Count() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_lines.size();
    }

private:
    static std::wstring Timestamp() {
        auto now  = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &time);
        std::wostringstream ss;
        ss << std::setfill(L'0')
           << std::setw(2) << tm.tm_hour << L":"
           << std::setw(2) << tm.tm_min  << L":"
           << std::setw(2) << tm.tm_sec;
        return ss.str();
    }

    static std::wstring LevelStr(Level l) {
        switch (l) {
            case Level::Info:    return L"[INFO]";
            case Level::Warning: return L"[WARN]";
            case Level::Error:   return L"[ERR] ";
        }
        return L"";
    }

    static inline std::mutex              s_mutex;
    static inline std::wofstream          s_file;
    static inline std::vector<std::wstring> s_lines;
};
