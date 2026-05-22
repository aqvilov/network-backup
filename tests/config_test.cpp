#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>
#include "../include/Config.h"

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

std::wstring GetTestConfigPath()
{
    return L"test_config_temp.txt";
}

void ResetConfig()
{
    std::wofstream file(GetTestConfigPath(), std::ios::trunc);
    file.close();

    Config::Load(GetTestConfigPath());
    Config::ClearWatchPaths();
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

void TestSetAndGet()
{
    ResetConfig();

    Config::Set(L"username", L"admin");
    Config::Set(L"port", L"8080");

    ASSERT_EQ(L"admin", Config::Get(L"username"));
    ASSERT_EQ(L"8080", Config::Get(L"port"));
    ASSERT_EQ(L"default", Config::Get(L"missing", L"default"));
}

void TestOverwriteValue()
{
    ResetConfig();

    Config::Set(L"user", L"admin");
    Config::Set(L"user", L"root");

    ASSERT_EQ(L"root", Config::Get(L"user"));
}

void TestHas()
{
    ResetConfig();

    Config::Set(L"existing", L"value");

    ASSERT_TRUE(Config::Has(L"existing"));
    ASSERT_FALSE(Config::Has(L"missing"));
}

void TestSaveAndLoad()
{
    ResetConfig();

    Config::Set(L"server", L"localhost");
    Config::Set(L"timeout", L"30");

    Config::Save();
    Config::Load(GetTestConfigPath());

    ASSERT_EQ(L"localhost", Config::Get(L"server"));
    ASSERT_EQ(L"30", Config::Get(L"timeout"));
}

void TestSetWatchPaths()
{
    ResetConfig();

    std::vector<std::wstring> paths =
    {
        L"C:\\source1",
        L"D:\\source2"
    };

    Config::SetWatchPaths(paths);

    auto loaded = Config::GetWatchPaths();

    ASSERT_EQ(size_t(2), loaded.size());
    ASSERT_EQ(L"C:\\source1", loaded[0]);
    ASSERT_EQ(L"D:\\source2", loaded[1]);
}

void TestAddWatchPath()
{
    ResetConfig();

    Config::AddWatchPath(L"C:\\first");
    Config::AddWatchPath(L"D:\\second");

    auto paths = Config::GetWatchPaths();

    ASSERT_EQ(size_t(2), paths.size());
    ASSERT_EQ(L"C:\\first", paths[0]);
    ASSERT_EQ(L"D:\\second", paths[1]);
}

void TestRemoveWatchPath()
{
    ResetConfig();

    Config::AddWatchPath(L"C:\\keep");
    Config::AddWatchPath(L"D:\\remove");
    Config::AddWatchPath(L"E:\\keep2");

    Config::RemoveWatchPath(1);

    auto paths = Config::GetWatchPaths();

    ASSERT_EQ(size_t(2), paths.size());
    ASSERT_EQ(L"C:\\keep", paths[0]);
    ASSERT_EQ(L"E:\\keep2", paths[1]);
}

void TestRemoveInvalidIndex()
{
    ResetConfig();

    Config::AddWatchPath(L"C:\\test");

    Config::RemoveWatchPath(99);

    ASSERT_EQ(size_t(1), Config::GetWatchPaths().size());
}

void TestClearWatchPaths()
{
    ResetConfig();

    Config::AddWatchPath(L"C:\\path1");
    Config::AddWatchPath(L"C:\\path2");

    Config::ClearWatchPaths();

    ASSERT_EQ(size_t(0), Config::GetWatchPaths().size());
}

void TestMaxVersions()
{
    ResetConfig();

    Config::SetMaxVersions(10);
    ASSERT_EQ(10, Config::GetMaxVersions());

    Config::SetMaxVersions(3);
    ASSERT_EQ(3, Config::GetMaxVersions());
}

int main()
{
    std::cout << "========== Running Config Tests ==========\n";

    RunTest(TestSetAndGet, "SetAndGet");
    RunTest(TestOverwriteValue, "OverwriteValue");
    RunTest(TestHas, "Has");
    RunTest(TestSaveAndLoad, "SaveAndLoad");
    RunTest(TestSetWatchPaths, "SetWatchPaths");
    RunTest(TestAddWatchPath, "AddWatchPath");
    RunTest(TestRemoveWatchPath, "RemoveWatchPath");
    RunTest(TestRemoveInvalidIndex, "RemoveInvalidIndex");
    RunTest(TestClearWatchPaths, "ClearWatchPaths");
    RunTest(TestMaxVersions, "MaxVersions");

    std::remove("test_config_temp.txt");

    std::cout << "\n========== Results: "
        << testsPassed
        << " passed, "
        << testsFailed
        << " failed ==========\n";

    return testsFailed ? 1 : 0;
}