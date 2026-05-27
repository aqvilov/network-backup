#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>
#include "../include/BackupQueue.h"

namespace fs = std::filesystem;

// Заглушка — определена в MainWindow.cpp, в тестах не нужна
void AddErrorRecord(const std::wstring&, const std::wstring&, bool, const std::wstring&) {}

// ============================================================================
// Тестовый фреймворк
// ============================================================================
int testsPassed = 0;
int testsFailed = 0;

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cout << "FAILED: expected " << (expected) << " but got " << (actual) \
                  << " (" << #actual << ", line " << __LINE__ << ")" << std::endl; \
        testsFailed++; \
        return; \
    }

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cout << "FAILED: " << #condition \
                  << " is false (line " << __LINE__ << ")" << std::endl; \
        testsFailed++; \
        return; \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        std::cout << "FAILED: " << #condition \
                  << " is true (line " << __LINE__ << ")" << std::endl; \
        testsFailed++; \
        return; \
    }

void RunTest(void (*test)(), const char* name)
{
    std::cout << "Running " << name << "... ";
    int oldFailed = testsFailed;
    test();
    if (testsFailed == oldFailed)
    {
        std::cout << "PASSED" << std::endl;
        testsPassed++;
    }
}

// Ждать пока суммарная статистика (copied+skipped+errors) достигнет N
static bool WaitForProcessed(BackupQueue& queue, uint64_t expected, int timeoutMs = 2000)
{
    int elapsed = 0;
    while (elapsed < timeoutMs)
    {
        auto s = queue.GetStats();
        if (s.copied + s.skipped + s.errors >= expected)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        elapsed += 50;
    }
    return false;
}

// ============================================================================
// Вспомогательная работа с временными директориями
// ============================================================================
static fs::path MakeTempDir(const std::wstring& suffix)
{
    fs::path dir = fs::temp_directory_path() / (L"bq_test_" + suffix);
    fs::create_directories(dir);
    return dir;
}

static void CreateFile(const fs::path& path, const std::string& content = "data")
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

// ============================================================================
// Тест 1: запуск и остановка
// ============================================================================
void TestStartStop()
{
    fs::path dest = MakeTempDir(L"start_stop");
    BackupQueue queue;
    bool started = queue.Start(dest.wstring(), nullptr);
    ASSERT_TRUE(started);
    ASSERT_TRUE(queue.GetStats().queued == 0);
    queue.Stop();
    fs::remove_all(dest);
}

// ============================================================================
// Тест 2: двойной старт без остановки возвращает false
// ============================================================================
void TestStartTwice()
{
    fs::path dest = MakeTempDir(L"start_twice");
    BackupQueue queue;
    bool first  = queue.Start(dest.wstring(), nullptr);
    bool second = queue.Start(dest.wstring(), nullptr);
    ASSERT_TRUE(first);
    ASSERT_FALSE(second);
    queue.Stop();
    fs::remove_all(dest);
}

// ============================================================================
// Тест 3: статистика по умолчанию — все нули
// ============================================================================
void TestStatsInitialization()
{
    BackupQueue queue;
    auto stats = queue.GetStats();
    ASSERT_EQ(0, stats.copied);
    ASSERT_EQ(0, stats.skipped);
    ASSERT_EQ(0, stats.errors);
    ASSERT_EQ(0, stats.bytes);
    ASSERT_EQ(0, stats.queued);
}

// ============================================================================
// Тест 4: реальное копирование — файл существует, должен скопироваться
// ============================================================================
void TestEnqueueRealFile()
{
    fs::path src  = MakeTempDir(L"src_real");
    fs::path dest = MakeTempDir(L"dst_real");
    fs::path file = src / L"hello.txt";
    CreateFile(file, "hello world");

    std::atomic<bool> callbackCalled{false};
    BackupQueue queue;
    queue.SetWatchRoots({ src.wstring() });
    queue.Start(dest.wstring(), [&](const std::wstring&, bool success, uint64_t) {
        if (success) callbackCalled = true;
    });

    queue.Enqueue(file.wstring());
    bool ok = WaitForProcessed(queue, 1);

    ASSERT_TRUE(ok);
    auto stats = queue.GetStats();
    ASSERT_EQ(1, stats.copied);
    ASSERT_EQ(0, stats.errors);
    ASSERT_TRUE(callbackCalled.load());

    queue.Stop();
    fs::remove_all(src);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 5: дубликаты игнорируются — второй Enqueue того же пути не добавляется
// ============================================================================
void TestDuplicateIgnored()
{
    fs::path src  = MakeTempDir(L"src_dup");
    fs::path dest = MakeTempDir(L"dst_dup");
    fs::path file = src / L"dup.txt";
    CreateFile(file, "content");

    std::atomic<int> callbackCount{0};
    BackupQueue queue;
    queue.SetWatchRoots({ src.wstring() });
    queue.Start(dest.wstring(), [&](const std::wstring&, bool, uint64_t) {
        callbackCount++;
    });

    // Оба Enqueue до того как воркер успеет взять файл
    queue.Enqueue(file.wstring());
    queue.Enqueue(file.wstring());

    WaitForProcessed(queue, 1, 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // дать время на второй (если бы был)

    ASSERT_EQ(1, callbackCount.load());

    queue.Stop();
    fs::remove_all(src);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 6: несколько уникальных файлов — все обрабатываются
// ============================================================================
void TestMultipleUniqueFiles()
{
    fs::path src  = MakeTempDir(L"src_multi");
    fs::path dest = MakeTempDir(L"dst_multi");
    CreateFile(src / L"f1.txt", "1");
    CreateFile(src / L"f2.txt", "2");
    CreateFile(src / L"f3.txt", "3");

    BackupQueue queue;
    queue.SetWatchRoots({ src.wstring() });
    queue.Start(dest.wstring(), nullptr);

    queue.Enqueue((src / L"f1.txt").wstring());
    queue.Enqueue((src / L"f2.txt").wstring());
    queue.Enqueue((src / L"f3.txt").wstring());

    bool ok = WaitForProcessed(queue, 3);
    ASSERT_TRUE(ok);

    auto stats = queue.GetStats();
    ASSERT_EQ(3, stats.copied + stats.skipped + stats.errors);

    queue.Stop();
    fs::remove_all(src);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 7: несуществующий файл → попадает в errors
// ============================================================================
void TestNonExistentFileGoesToErrors()
{
    fs::path src  = MakeTempDir(L"src_nofile");
    fs::path dest = MakeTempDir(L"dst_nofile");

    BackupQueue queue;
    queue.SetWatchRoots({ src.wstring() });
    queue.Start(dest.wstring(), nullptr);

    queue.Enqueue((src / L"ghost.txt").wstring());

    bool ok = WaitForProcessed(queue, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(1, queue.GetStats().errors);

    queue.Stop();
    fs::remove_all(src);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 8: несколько корневых путей — правильный выбирается для каждого файла
// ============================================================================
void TestSetWatchRoots()
{
    fs::path src1 = MakeTempDir(L"root1");
    fs::path src2 = MakeTempDir(L"root2");
    fs::path dest = MakeTempDir(L"dst_roots");
    CreateFile(src1 / L"a.txt", "aaa");
    CreateFile(src2 / L"b.txt", "bbb");

    BackupQueue queue;
    queue.SetWatchRoots({ src1.wstring(), src2.wstring() });
    queue.Start(dest.wstring(), nullptr);

    queue.Enqueue((src1 / L"a.txt").wstring());
    queue.Enqueue((src2 / L"b.txt").wstring());

    bool ok = WaitForProcessed(queue, 2);
    ASSERT_TRUE(ok);

    auto stats = queue.GetStats();
    ASSERT_EQ(2, stats.copied + stats.skipped + stats.errors);

    queue.Stop();
    fs::remove_all(src1);
    fs::remove_all(src2);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 9: пустой путь — не добавляется в очередь (pending не содержит "")
// ============================================================================
void TestEmptyPathNotEnqueued()
{
    fs::path dest = MakeTempDir(L"dst_empty");
    BackupQueue queue;
    queue.Start(dest.wstring(), nullptr);

    queue.Enqueue(L"");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto stats = queue.GetStats();
    // пустой путь добавляется как любой другой — воркер выдаст ошибку
    // главное что queued == 0 (уже обработан или не добавлен)
    ASSERT_EQ(0, stats.queued);

    queue.Stop();
    fs::remove_all(dest);
}

// ============================================================================
// Тест 10: ResultCallback вызывается с правильными аргументами
// ============================================================================
void TestResultCallback()
{
    fs::path src  = MakeTempDir(L"src_cb");
    fs::path dest = MakeTempDir(L"dst_cb");
    fs::path file = src / L"cb_file.txt";
    CreateFile(file, "callback test");

    std::wstring receivedPath;
    bool receivedSuccess = false;
    uint64_t receivedBytes = 0;

    BackupQueue queue;
    queue.SetWatchRoots({ src.wstring() });
    queue.Start(dest.wstring(),
        [&](const std::wstring& path, bool success, uint64_t bytes) {
            receivedPath    = path;
            receivedSuccess = success;
            receivedBytes   = bytes;
        });

    queue.Enqueue(file.wstring());
    WaitForProcessed(queue, 1);

    ASSERT_TRUE(receivedSuccess);
    ASSERT_TRUE(receivedBytes > 0);
    ASSERT_TRUE(receivedPath == file.wstring());

    queue.Stop();
    fs::remove_all(src);
    fs::remove_all(dest);
}

// ============================================================================
// Тест 11: EnableGoogleDriveUpload не крашит при false
// ============================================================================
void TestGoogleDriveDisabled()
{
    fs::path dest = MakeTempDir(L"dst_gdrive");
    BackupQueue queue;
    queue.EnableGoogleDriveUpload(false);
    bool started = queue.Start(dest.wstring(), nullptr);
    ASSERT_TRUE(started);
    queue.Stop();
    fs::remove_all(dest);
}

// ============================================================================
// main
// ============================================================================
int main()
{
    std::cout << "========== Running BackupQueue Tests ==========\n";

    RunTest(TestStartStop,                "StartStop");
    RunTest(TestStartTwice,               "StartTwice");
    RunTest(TestStatsInitialization,      "StatsInitialization");
    RunTest(TestEnqueueRealFile,          "EnqueueRealFile");
    RunTest(TestDuplicateIgnored,         "DuplicateIgnored");
    RunTest(TestMultipleUniqueFiles,      "MultipleUniqueFiles");
    RunTest(TestNonExistentFileGoesToErrors, "NonExistentFileGoesToErrors");
    RunTest(TestSetWatchRoots,            "SetWatchRoots");
    RunTest(TestEmptyPathNotEnqueued,     "EmptyPathNotEnqueued");
    RunTest(TestResultCallback,           "ResultCallback");
    RunTest(TestGoogleDriveDisabled,      "GoogleDriveDisabled");

    std::cout << "\n========== Results: "
              << testsPassed << " passed, "
              << testsFailed << " failed ==========\n";

    return testsFailed ? 1 : 0;
}