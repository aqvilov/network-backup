#include <iostream>
#include <thread>
#include <chrono>
#include "../include/BackupQueue.h"

int testsPassed = 0;
int testsFailed = 0;

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cout << "FAILED: " << #expected << " != " << #actual \
                  << " (line " << __LINE__ << ")" << std::endl; \
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

void TestStartStop()
{
    BackupQueue queue;
    bool started = queue.Start(L"C:\\backup", nullptr);
    ASSERT_TRUE(started);
    queue.Stop();
}

void TestStartTwice()
{
    BackupQueue queue;
    bool first = queue.Start(L"C:\\backup", nullptr);
    bool second = queue.Start(L"C:\\backup2", nullptr);
    ASSERT_TRUE(first);
    ASSERT_FALSE(second);
    queue.Stop();
}

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

void TestEnqueueAndStats()
{
    BackupQueue queue;
    queue.Start(L"C:\\backup", nullptr);
    queue.SetWatchRoots({ L"C:\\data" });
    queue.Enqueue(L"C:\\data\\file1.txt");
    queue.Enqueue(L"C:\\data\\file2.txt");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto stats = queue.GetStats();
    ASSERT_EQ(2, stats.queued);
    queue.Stop();
}

void TestDuplicateIgnored()
{
    BackupQueue queue;
    queue.Start(L"C:\\backup", nullptr);
    queue.SetWatchRoots({ L"C:\\data" });
    queue.Enqueue(L"C:\\data\\file.txt");
    queue.Enqueue(L"C:\\data\\file.txt");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto stats = queue.GetStats();
    ASSERT_EQ(1, stats.queued);
    queue.Stop();
}

void TestMultipleUniqueFiles()
{
    BackupQueue queue;
    queue.Start(L"C:\\backup", nullptr);
    queue.SetWatchRoots({ L"C:\\data" });
    queue.Enqueue(L"C:\\data\\f1.txt");
    queue.Enqueue(L"C:\\data\\f2.txt");
    queue.Enqueue(L"C:\\data\\f3.txt");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto stats = queue.GetStats();
    ASSERT_EQ(3, stats.queued);
    queue.Stop();
}

void TestSetWatchRoots()
{
    BackupQueue queue;
    queue.Start(L"C:\\backup", nullptr);
    std::vector<std::wstring> roots = { L"C:\\root1", L"D:\\root2" };
    queue.SetWatchRoots(roots);
    queue.Enqueue(L"C:\\root1\\file.txt");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto stats = queue.GetStats();
    ASSERT_EQ(1, stats.queued);
    queue.Stop();
}

void TestInvalidPathIgnored()
{
    BackupQueue queue;
    queue.Start(L"C:\\backup", nullptr);
    queue.Enqueue(L"");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto stats = queue.GetStats();
    ASSERT_EQ(0, stats.queued);
    queue.Stop();
}

int main()
{
    std::cout << "========== Running BackupQueue Tests ==========\n";

    RunTest(TestStartStop, "StartStop");
    RunTest(TestStartTwice, "StartTwice");
    RunTest(TestStatsInitialization, "StatsInitialization");
    RunTest(TestEnqueueAndStats, "EnqueueAndStats");
    RunTest(TestDuplicateIgnored, "DuplicateIgnored");
    RunTest(TestMultipleUniqueFiles, "MultipleUniqueFiles");
    RunTest(TestSetWatchRoots, "SetWatchRoots");
    RunTest(TestInvalidPathIgnored, "InvalidPathIgnored");

    std::cout << "\n========== Results: "
        << testsPassed
        << " passed, "
        << testsFailed
        << " failed ==========\n";

    return testsFailed ? 1 : 0;
}