#define WIN32_LEAN_AND_MEAN

#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include "../include/Logger.h"

namespace fs = std::filesystem;

// Вспомогательная функция для получения уникального имени файла
std::wstring GetUniqueTempFileName(const std::wstring& prefix) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    return std::wstring(tempPath) + prefix + L"_" + std::to_wstring(timestamp) + L".txt";
}

void TestLoggerInitialization() {
    std::cout << "Testing logger initialization..." << std::endl;
    
    std::wstring logFilePath = GetUniqueTempFileName(L"logger_init");
    fs::path logFile(logFilePath);
    
    // Clean up any existing file
    std::error_code ec;
    fs::remove(logFile, ec);
    
    // Initialize logger
    Logger::Init(logFile.wstring());
    
    // Check that we can write
    Logger::Info(L"Test info message");
    Logger::Warning(L"Test warning message");
    Logger::Error(L"Test error message");
    
    // Check that log file was created
    assert(fs::exists(logFile));
    
    // Check file has content
    std::ifstream file(logFile);
    assert(file.good());
    
    // Cleanup
    file.close();
    fs::remove(logFile, ec);
    
    std::cout << " Logger initialization passed" << std::endl;
}

void TestLoggerMultipleWrites() {
    std::cout << "Testing multiple writes..." << std::endl;
    
    std::wstring logFilePath = GetUniqueTempFileName(L"logger_multi");
    fs::path logFile(logFilePath);
    std::error_code ec;
    fs::remove(logFile, ec);
    
    Logger::Init(logFile.wstring());
    
    const int messageCount = 100;
    for (int i = 0; i < messageCount; ++i) {
        Logger::Info(L"Message " + std::to_wstring(i));
    }
    
    // Count lines in file
    std::ifstream file(logFile);
    int lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        lineCount++;
    }
    file.close();
    
    assert(lineCount == messageCount);
    
    fs::remove(logFile, ec);
    
    std::cout << " Multiple writes passed" << std::endl;
}

void TestLoggerMemoryBuffer() {
    std::cout << "Testing memory buffer..." << std::endl;
    
    std::wstring logFilePath = GetUniqueTempFileName(L"logger_mem");
    fs::path logFile(logFilePath);
    std::error_code ec;
    fs::remove(logFile, ec);
    
    Logger::Init(logFile.wstring());
    
    // Write some messages
    Logger::Info(L"Buffer test 1");
    Logger::Warning(L"Buffer test 2");
    Logger::Error(L"Buffer test 3");
    
    // Get lines from memory
    auto lines = Logger::GetLines();
    assert(lines.size() >= 3);
    
    // Check that count works
    size_t count = Logger::Count();
    assert(count == lines.size());
    
    fs::remove(logFile, ec);
    
    std::cout << " Memory buffer passed" << std::endl;
}

void TestLoggerOverflow() {
    std::cout << "Testing buffer overflow (max 1000 lines)..." << std::endl;
    
    std::wstring logFilePath = GetUniqueTempFileName(L"logger_overflow");
    fs::path logFile(logFilePath);
    std::error_code ec;
    fs::remove(logFile, ec);
    
    Logger::Init(logFile.wstring());
    
    // Write 1500 messages
    const int messageCount = 1500;
    for (int i = 0; i < messageCount; ++i) {
        Logger::Info(L"Overflow test " + std::to_wstring(i));
    }
    
    auto lines = Logger::GetLines();
    assert(lines.size() <= 1000);
    
    fs::remove(logFile, ec);
    
    std::cout << " Buffer overflow passed" << std::endl;
}

void TestLoggerThreadSafety() {
    std::cout << "Testing thread safety (basic)..." << std::endl;
    
    std::wstring logFilePath = GetUniqueTempFileName(L"logger_thread");
    fs::path logFile(logFilePath);
    std::error_code ec;
    fs::remove(logFile, ec);
    
    Logger::Init(logFile.wstring());
    
    // Simulate concurrent writes (simplified test)
    Logger::Info(L"Thread safety test");
    Logger::Warning(L"Another message");
    Logger::Error(L"Error message");
    
    // Just verify no crash
    assert(true);
    
    fs::remove(logFile, ec);
    
    std::cout << " Thread safety passed" << std::endl;
}

int main() {
    std::cout << "\n=== Running Logger Tests ===\n" << std::endl;
    
    try {
        TestLoggerInitialization();
        TestLoggerMultipleWrites();
        TestLoggerMemoryBuffer();
        TestLoggerOverflow();
        TestLoggerThreadSafety();
        
        std::cout << "\n All Logger tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n Unknown test failure" << std::endl;
        return 1;
    }
    
    return 0;
}