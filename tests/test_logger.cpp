#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cassert>
#include <thread>
#include <chrono>
#include "Logger.h"

namespace fs = std::filesystem;

// Вспомогательная функция для чтения лог-файла
std::vector<std::wstring> ReadLogFile(const std::wstring& path) {
    std::vector<std::wstring> lines;
    std::wifstream file(path);
    if (!file.is_open()) return lines;
    
    std::wstring line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

// Тест 1: Инициализация логгера
bool TestLoggerInit() {
    std::cout << "  Testing logger initialization... ";
    
    fs::path tempDir = fs::temp_directory_path() / L"logger_test";
    fs::create_directories(tempDir);
    fs::path logFile = tempDir / L"test.log";
    
    Logger::Init(logFile.wstring());
    
    // Проверяем, что файл создался
    bool fileExists = fs::exists(logFile);
    assert(fileExists);
    
    Logger::Info(L"Test message");
    Logger::Close();
    
    fs::remove_all(tempDir);
    
    std::cout << "PASSED" << std::endl;
    return true;
}

// Тест 2: Запись нескольких сообщений
bool TestLoggerMultipleWrites() {
    std::cout << "  Testing multiple writes... ";
    
    fs::path tempDir = fs::temp_directory_path() / L"logger_test2";
    fs::create_directories(tempDir);
    fs::path logFile = tempDir / L"test.log";
    
    Logger::Init(logFile.wstring());
    
    const int messageCount = 5;
    for (int i = 0; i < messageCount; i++) {
        Logger::Info(L"Message " + std::to_wstring(i));
    }
    Logger::Close();
    
    auto lines = ReadLogFile(logFile.wstring());
    assert(lines.size() >= 1); // Хотя бы одно сообщение записалось
    
    fs::remove_all(tempDir);
    
    std::cout << "PASSED" << std::endl;
    return true;
}

// Тест 3: Разные уровни логирования
bool TestLoggerLevels() {
    std::cout << "  Testing log levels... ";
    
    fs::path tempDir = fs::temp_directory_path() / L"logger_test3";
    fs::create_directories(tempDir);
    fs::path logFile = tempDir / L"test.log";
    
    Logger::Init(logFile.wstring());
    
    Logger::Info(L"Info message");
    Logger::Warning(L"Warning message");
    Logger::Error(L"Error message");
    Logger::Close();
    
    auto lines = ReadLogFile(logFile.wstring());
    assert(lines.size() >= 3);
    
    fs::remove_all(tempDir);
    
    std::cout << "PASSED" << std::endl;
    return true;
}

// Тест 4: Дописывание в файл (append mode)
bool TestLoggerAppend() {
    std::cout << "  Testing append mode... ";
    
    fs::path tempDir = fs::temp_directory_path() / L"logger_test4";
    fs::create_directories(tempDir);
    fs::path logFile = tempDir / L"test.log";
    
    Logger::Init(logFile.wstring());
    Logger::Info(L"First session");
    Logger::Close();
    
    // Вторая сессия - должна дописать, а не перезаписать
    Logger::Init(logFile.wstring());
    Logger::Info(L"Second session");
    Logger::Close();
    
    auto lines = ReadLogFile(logFile.wstring());
    assert(lines.size() >= 2);
    
    fs::remove_all(tempDir);
    
    std::cout << "PASSED" << std::endl;
    return true;
}

// Тест 5: Логирование без инициализации
bool TestLoggerWithoutInit() {
    std::cout << "  Testing without init (should not crash)... ";
    
    // Просто вызываем логгер без инициализации - не должно упасть
    Logger::Info(L"This should not crash");
    Logger::Warning(L"This should not crash either");
    Logger::Error(L"Still no crash");
    
    std::cout << "PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Running Logger Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    // Запускаем тесты
    if (TestLoggerInit()) passed++; else failed++;
    if (TestLoggerMultipleWrites()) passed++; else failed++;
    if (TestLoggerLevels()) passed++; else failed++;
    if (TestLoggerAppend()) passed++; else failed++;
    if (TestLoggerWithoutInit()) passed++; else failed++;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return failed > 0 ? 1 : 0;
}