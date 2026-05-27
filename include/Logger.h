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
private:
    // C++17 inline static - определения прямо в заголовке!
    static inline std::wofstream logFile;
    static inline std::mutex logMutex;
    
public:
    static void Init(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) logFile.close();
    logFile.open(path, std::ios::app);
    if (logFile.is_open()) {
        // Пишем напрямую, без вызова Log()
        logFile << L"Logger initialized" << std::endl;
        logFile.flush();
    }
}
    
    static void Log(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            logFile << std::put_time(std::localtime(&time), L"%Y-%m-%d %H:%M:%S") 
                    << L" - " << message << std::endl;
            logFile.flush();  // Важно! Немедленно записываем
        }
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
    if (logFile.is_open()) {
        logFile << L"Logger closed" << std::endl;
        logFile.flush();
        logFile.close();
    }
}
};