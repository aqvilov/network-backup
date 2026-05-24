#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../include/FileUtils.h"
#include "../include/Logger.h"
#include "../include/Watcher.h"

namespace fs = std::filesystem;

static std::wstring MakeTempDirectory() {
    static std::atomic<uint32_t> counter{0};

    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath))
        throw std::runtime_error("Не удалось получить TEMP путь");

    std::wstring path = tempPath;
    std::wstring name = L"netbackup_test_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(counter.fetch_add(1));
    fs::path dir = fs::path(path) / name;
    fs::create_directories(dir);
    return dir.wstring();
}

static void RemoveTempDirectory(const std::wstring& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
}

static void WriteTextFile(const std::wstring& filePath, const std::string& content) {
    std::ofstream f(fs::path(filePath), std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Не удалось создать файл");
    f << content;
}

static std::wstring ReadTextFile(const std::wstring& filePath) {
    std::ifstream f(fs::path(filePath), std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Не удалось открыть файл");
    std::ostringstream ss;
    ss << f.rdbuf();
    return std::wstring(ss.str().begin(), ss.str().end());
}

static bool StringEndsWith(const std::wstring& s, const std::wstring& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void Assert(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::abort();
    }
}

static void TestGetRelativePath() {
    std::wstring root = L"C:\\temp\\root";
    std::wstring full = L"C:\\temp\\root\\sub\\file.txt";
    std::wstring rel = FileUtils::GetRelativePath(full, root);
    Assert(rel == L"sub\\file.txt", "GetRelativePath вернул неверный относительный путь");
}

static void TestCopyToBackup() {
    std::wstring srcDir = MakeTempDirectory();
    std::wstring destDir = MakeTempDirectory();
    std::wstring subDir = srcDir + L"\\data";
    fs::create_directories(subDir);
    std::wstring srcFile = subDir + L"\\test.txt";
    WriteTextFile(srcFile, "hello world");

    auto result = FileUtils::CopyToBackup(srcFile, srcDir, destDir);
    if (!result.success) {
        std::wcerr << L"CopyToBackup failed:\n";
        std::wcerr << L"  srcFile = " << srcFile << L"\n";
        std::wcerr << L"  srcDir = " << srcDir << L"\n";
        std::wcerr << L"  destDir = " << destDir << L"\n";
        std::wcerr << L"  error = " << result.error << L"\n";
    }
    Assert(result.success, "CopyToBackup должен успешно копировать файл");
    Assert(result.bytesCopied == 11, "CopyToBackup вернул неверный размер копии");

    std::wstring destFile = destDir + L"\\data\\test.txt";
    Assert(fs::exists(destFile), "Файл не был скопирован в папку бэкапа");

    RemoveTempDirectory(srcDir);
    RemoveTempDirectory(destDir);
}

static void TestCopyToBackupSkipTempFile() {
    std::wstring srcDir = MakeTempDirectory();
    std::wstring destDir = MakeTempDirectory();
    std::wstring srcFile = srcDir + L"\\~temp.txt";
    WriteTextFile(srcFile, "dummy");

    auto result = FileUtils::CopyToBackup(srcFile, srcDir, destDir);
    Assert(!result.success, "CopyToBackup должен пропустить временный файл");
    Assert(result.error.find(L"Пропущен временный файл") != std::wstring::npos,
           "CopyToBackup вернул неверное сообщение об ошибке для временного файла");

    RemoveTempDirectory(srcDir);
    RemoveTempDirectory(destDir);
}

static void TestComputeCRC32() {
    std::wstring tempDir = MakeTempDirectory();
    std::wstring filePath = tempDir + L"\\crc_test.bin";
    WriteTextFile(filePath, "abc");

    uint32_t crc = FileUtils::ComputeCRC32(filePath);
    Assert(crc == 0x352441C2u, "ComputeCRC32 returned incorrect value for abc");

    RemoveTempDirectory(tempDir);
}

static void TestFormatSize() {
    Assert(FileUtils::FormatSize(1) == L"1 B", "FormatSize для 1 байта некорректен");
    Assert(FileUtils::FormatSize(1024) == L"1.0 KB", "FormatSize для 1024 байт некорректен");
    Assert(FileUtils::FormatSize(1048576) == L"1.0 MB", "FormatSize для 1 МБ некорректен");
    Assert(FileUtils::FormatSize(1073741824) == L"1.00 GB", "FormatSize для 1 ГБ некорректен");
}

static void TestUtf8ToWide() {
    std::wstring wide = FileUtils::Utf8ToWide("Test \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
    Assert(wide == L"Test Привет", "Utf8ToWide неверно конвертирует UTF-8 строку");
}

static void TestGetAppDataDir() {
    std::wstring path = FileUtils::GetAppDataDir();
    Assert(!path.empty(), "GetAppDataDir вернул пустую строку");
    Assert(StringEndsWith(path, L"\\NetBackup"), "GetAppDataDir должен возвращать путь, заканчивающийся на \"\\NetBackup\"");
}

static void TestLogger() {
    std::wstring logDir = MakeTempDirectory();
    std::wstring logFile = logDir + L"\\logger_test.log";
    size_t before = Logger::Count();

    Logger::Init(logFile);
    Logger::Info(L"Test info");
    Logger::Warning(L"Test warning");
    Logger::Error(L"Test error");

    Assert(Logger::Count() == before + 3, "Logger должен накапливать 3 новых строки");

    auto lines = Logger::GetLines();
    Assert(lines.size() >= 3, "Logger::GetLines вернул недостаточно строк");
    Assert(lines.back().find(L"[ERR]") != std::wstring::npos, "Последняя строка логгера должна содержать уровень [ERR]");

    Assert(fs::exists(logFile), "Лог-файл должен быть создан");

    RemoveTempDirectory(logDir);
}

static void TestWatcher() {
    std::wstring watchDir = MakeTempDirectory();
    std::promise<FileAction> promise;
    bool callbackCalled = false;

    Watcher watcher;
    bool started = watcher.Start(watchDir, false, [&](FileAction action, const std::wstring& path) {
        if (action == FileAction::Added && path.find(L"watch_test.txt") != std::wstring::npos) {
            if (!callbackCalled) {
                callbackCalled = true;
                promise.set_value(action);
            }
        }
    });
    Assert(started, "Watcher не запустился для существующей папки");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::wstring filePath = watchDir + L"\\watch_test.txt";
    WriteTextFile(filePath, "watcher");

    auto future = promise.get_future();
    auto status = future.wait_for(std::chrono::seconds(5));
    watcher.Stop();
    Assert(status == std::future_status::ready, "Watcher не зафиксировал создание файла в течение 5 секунд");
    Assert(!watcher.IsRunning(), "Watcher должен остановиться после Stop()");

    RemoveTempDirectory(watchDir);
}

static void TestWatcherInvalidFolder() {
    Watcher watcher;
    bool started = watcher.Start(L"C:\\this_folder_does_not_exist_12345", false, [](FileAction, const std::wstring&) {});
    Assert(!started, "Watcher должен вернуть false для несуществующей папки");
}

int main() {
    std::vector<std::function<void()>> tests = {
        TestGetRelativePath,
        TestCopyToBackup,
        TestCopyToBackupSkipTempFile,
        TestComputeCRC32,
        TestFormatSize,
        TestUtf8ToWide,
        TestGetAppDataDir,
        TestLogger,
        TestWatcher,
        TestWatcherInvalidFolder,
    };

    std::cout << "Running " << tests.size() << " tests...\n";
    for (size_t i = 0; i < tests.size(); ++i) {
        tests[i]();
        std::cout << "Test " << (i + 1) << " passed\n";
    }

    std::cout << "All tests passed!\n";
    return 0;
}
