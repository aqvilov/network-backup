#pragma once
// Logger.h — запись событий в файл
// Потокобезопасен: можно вызывать из любого потока

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <mutex>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

class Logger {
private:
    static inline std::wofstream logFile;
    static inline std::mutex logMutex;

    // Внутренняя запись без захвата мьютекса — вызывать только под локом!
    static void WriteRaw(const std::wstring& message) {
        if (!logFile.is_open()) return;
        auto now  = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        logFile << std::put_time(std::localtime(&time), L"%Y-%m-%d %H:%M:%S")
                << L" - " << message << std::endl;
        logFile.flush();
    }

public:
    static void Init(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open())
            logFile.close();
        logFile.open(path, std::ios::app);
        WriteRaw(L"Logger initialized");  // уже под локом — без рекурсии
    }

    static void Log(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        WriteRaw(message);
    }

    static void Info(const std::wstring& message) {
        Log(L"[INFO] " + message);
    }

    static void Warning(const std::wstring& message) {
        Log(L"[WARNING] " + message);
    }

    static void Error(const std::wstring& message) {
        Log(L"[ERROR] " + message);
    }

    static void Close() {
        std::lock_guard<std::mutex> lock(logMutex);
        WriteRaw(L"Logger closed");       // уже под локом - значит без рекурсии
        logFile.close();
    }
};