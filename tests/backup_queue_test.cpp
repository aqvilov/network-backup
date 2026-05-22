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

    if (testsFailed == oldFailed) {
        std::cout << "PASSED\n";
        testsPassed++;
    }
}

void TestStartStop()
{
    BackupQueue queue;

    ASSERT_TRUE(queue.Start(L"C:\\backup", nullptr));

    queue.Stop();
}

void TestStartTwice()
{
    BackupQueue queue;

    ASSERT_TRUE(queue.Start(L"C:\\backup", nullptr));
    ASSERT_FALSE(queue.Start(L"C:\\backup2", nullptr));

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
    ASSERT_EQ(size_t(0), stats.queued);
}

void TestDuplicateQueueWithoutWorker()
{
    BackupQueue queue;

    queue.Enqueue(L"C:\\data\\file.txt");
    queue.Enqueue(L"C:\\data\\file.txt");

    auto stats = queue.GetStats();

    ASSERT_EQ(size_t(1), stats.queued);
}

void TestMultipleUniqueQueueWithoutWorker()
{
    BackupQueue queue;

    queue.Enqueue(L"C:\\data\\a.txt");
    queue.Enqueue(L"C:\\data\\b.txt");
    queue.Enqueue(L"C:\\data\\c.txt");

    auto stats = queue.GetStats();

    ASSERT_EQ(size_t(3), stats.queued);
}

void TestInvalidPathAcceptedIntoQueue()
{
    BackupQueue queue;

    queue.Enqueue(L"");

    auto stats = queue.GetStats();

    ASSERT_EQ(size_t(1), stats.queued);
}

void TestStopWithoutStart()
{
    BackupQueue queue;

    queue.Stop();

    ASSERT_TRUE(true);
}

int main()
{
    std::cout << "========== Running BackupQueue Tests ==========\n";

    RunTest(TestStartStop, "StartStop");
    RunTest(TestStartTwice, "StartTwice");
    RunTest(TestStatsInitialization, "StatsInitialization");
    RunTest(TestDuplicateQueueWithoutWorker, "DuplicateQueueWithoutWorker");
    RunTest(TestMultipleUniqueQueueWithoutWorker, "MultipleUniqueQueueWithoutWorker");
    RunTest(TestInvalidPathAcceptedIntoQueue, "InvalidPathAcceptedIntoQueue");
    RunTest(TestStopWithoutStart, "StopWithoutStart");

    std::cout << "\n========== Results: "
        << testsPassed
        << " passed, "
        << testsFailed
        << " failed ==========\n";

    return testsFailed ? 1 : 0;
}