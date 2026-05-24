#include <windows.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Простая тестовая обёртка
#define EXPECT_TRUE(expr) do { if (!(expr)) { std::cerr << "Expectation failed at line " << __LINE__ << ": " #expr << std::endl; return false; } } while (0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))

// ГЛОБАЛЬНАЯ ПЕРЕМЕННАЯ ДЛЯ ТЕСТОВОГО РЕЖИМА
static bool g_testMode = false;

// Функция-обёртка для MessageBox в тестовом режиме
static inline int TestMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    if (g_testMode) {
        // В тестовом режиме не показываем MessageBox, просто логируем
        std::wcerr << L"\n  [TEST MODE] MessageBox suppressed: " << (lpCaption ? lpCaption : L"") 
                   << L"\n  Message: " << (lpText ? lpText : L"") << std::endl;
        return IDOK;
    }
    return MessageBoxW(hWnd, lpText, lpCaption, uType);
}

// Заменяем все вызовы MessageBoxW в MainWindow.cpp на нашу обёртку
#define MessageBoxW TestMessageBoxW

// Подключаем реализацию MainWindow.cpp прямо в тестовый модуль, чтобы получить доступ к статическим helper-элементам
#include "../src/MainWindow.cpp"

// Восстанавливаем оригинальный MessageBoxW
#undef MessageBoxW

static fs::path MakeTempTestRoot() {
    auto base = fs::temp_directory_path() / L"NetBackupMainWindowTest";
    auto id = std::to_wstring(::GetTickCount64());
    return base / id;
}

static void CleanupTemp(const fs::path& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
}

static bool WaitForCopiedCount(uint64_t expected, int timeoutMs = 5000) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto stats = g_queue.GetStats();
        if (stats.copied >= expected)
            return true;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() >= timeoutMs) {
            std::cerr << "Timeout: copied=" << stats.copied << ", expected=" << expected << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

static void ResetMainWindowState() {
    g_hWnd = nullptr;
    g_isRunning.store(false);
    g_isFullSyncRunning.store(false);
    if (g_syncThread.joinable())
        g_syncThread.join();
    StopBackup();
}

static void WriteTestFile(const fs::path& path, const std::string& content) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, content.c_str(), (DWORD)content.size(), &written, nullptr);
    CloseHandle(file);
}

// ============ ТЕСТЫ ============

// Тест 1: StartBackup выполняет начальную синхронизацию
static bool Test_StartBackupPerformsInitialSync() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";
    auto sourceFile = watchRoot / L"sample.txt";
    auto targetFile = destRoot / L"sample.txt";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot));
    EXPECT_TRUE(fs::create_directories(destRoot));

    WriteTestFile(sourceFile, "Hello NetBackup");

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(1, 8000));
    EXPECT_TRUE(fs::exists(targetFile));
    EXPECT_TRUE(fs::file_size(targetFile) > 0);

    StopBackup();
    CleanupTemp(root);
    return true;
}

// Тест 2: StopBackup безопасен когда не запущен
static bool Test_StopBackupIsSafeWhenNotRunning() {
    ResetMainWindowState();
    StopBackup();
    EXPECT_FALSE(g_isRunning.load());
    EXPECT_FALSE(g_isFullSyncRunning.load());
    return true;
}

// Тест 3: Синхронизация нескольких файлов
static bool Test_FullSyncMultipleFiles() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot));
    EXPECT_TRUE(fs::create_directories(destRoot));

    // Создаём 3 тестовых файла
    WriteTestFile(watchRoot / L"file1.txt", "Content 1");
    WriteTestFile(watchRoot / L"file2.txt", "Content 2");
    WriteTestFile(watchRoot / L"file3.txt", "Content 3");

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(3, 10000));

    EXPECT_TRUE(fs::exists(destRoot / L"file1.txt"));
    EXPECT_TRUE(fs::exists(destRoot / L"file2.txt"));
    EXPECT_TRUE(fs::exists(destRoot / L"file3.txt"));

    StopBackup();
    CleanupTemp(root);
    return true;
}

// Тест 4: Синхронизация вложенных папок
static bool Test_FullSyncNestedDirectories() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot / L"subdir1" / L"subdir2"));
    EXPECT_TRUE(fs::create_directories(destRoot));

    WriteTestFile(watchRoot / L"root.txt", "Root file");
    WriteTestFile(watchRoot / L"subdir1" / L"nested.txt", "Nested file");
    WriteTestFile(watchRoot / L"subdir1" / L"subdir2" / L"deep.txt", "Deep file");

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(3, 15000));

    // Добавим небольшую задержку для завершения всех операций
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!fs::exists(destRoot / L"root.txt"))
        std::cerr << "Missing: root.txt" << std::endl;
    if (!fs::exists(destRoot / L"subdir1" / L"nested.txt"))
        std::cerr << "Missing: subdir1/nested.txt" << std::endl;
    if (!fs::exists(destRoot / L"subdir1" / L"subdir2" / L"deep.txt"))
        std::cerr << "Missing: subdir1/subdir2/deep.txt" << std::endl;

    EXPECT_TRUE(fs::exists(destRoot / L"root.txt"));
    EXPECT_TRUE(fs::exists(destRoot / L"subdir1" / L"nested.txt"));
    EXPECT_TRUE(fs::exists(destRoot / L"subdir1" / L"subdir2" / L"deep.txt"));

    StopBackup();
    CleanupTemp(root);
    return true;
}

// Тест 5: Игнорирование системных файлов
static bool Test_FullSyncIgnoresSystemFiles() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot));
    EXPECT_TRUE(fs::create_directories(destRoot));

    WriteTestFile(watchRoot / L"normal.txt", "Normal file");
    WriteTestFile(watchRoot / L"~temp.txt", "Temp file");
    WriteTestFile(watchRoot / L"desktop.ini", "System file");
    WriteTestFile(watchRoot / L"thumbs.db", "Thumbs file");

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(1, 10000));

    // Дополнительная задержка
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!fs::exists(destRoot / L"normal.txt"))
        std::cerr << "Missing: normal.txt (should exist)" << std::endl;

    // Должен скопироваться только normal.txt
    EXPECT_TRUE(fs::exists(destRoot / L"normal.txt"));
    EXPECT_FALSE(fs::exists(destRoot / L"~temp.txt"));
    EXPECT_FALSE(fs::exists(destRoot / L"desktop.ini"));
    EXPECT_FALSE(fs::exists(destRoot / L"thumbs.db"));

    StopBackup();
    CleanupTemp(root);
    return true;
}

// Тест 6: Проверка валидации - пустые папки
static bool Test_StartBackupValidatesEmptyPaths() {
    Config::ClearWatchPaths();
    Config::Set(L"destPath", L"");
    ResetMainWindowState();

    // StartBackup должен показать MessageBox и не запуститься
    // Поскольку MessageBox блокирующий, мы не можем полноценно протестировать
    // Но можем проверить что состояние не изменилось
    auto initialState = g_isRunning.load();
    StartBackup(nullptr);
    
    // Ждём немного чтобы убедиться что ничего не запустилось
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(initialState, g_isRunning.load());

    return true;
}

// Тест 7: Проверка валидации - несуществующие папки
static bool Test_StartBackupValidatesNonExistentPaths() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"nonexistent_watch";
    auto destRoot = root / L"nonexistent_dest";

    CleanupTemp(root);
    
    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    auto initialState = g_isRunning.load();
    StartBackup(nullptr);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(initialState, g_isRunning.load());

    CleanupTemp(root);
    return true;
}

// Тест 8: Множественные папки слежки
static bool Test_MultipleWatchPaths() {
    auto root = MakeTempTestRoot();
    auto watchRoot1 = root / L"watch1";
    auto watchRoot2 = root / L"watch2";
    auto destRoot = root / L"dest";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot1));
    EXPECT_TRUE(fs::create_directories(watchRoot2));
    EXPECT_TRUE(fs::create_directories(destRoot));

    WriteTestFile(watchRoot1 / L"file1.txt", "From watch1");
    WriteTestFile(watchRoot2 / L"file2.txt", "From watch2");

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot1.wstring(), watchRoot2.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(2, 15000));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!fs::exists(destRoot / L"file1.txt"))
        std::cerr << "Missing: file1.txt from watch1" << std::endl;
    if (!fs::exists(destRoot / L"file2.txt"))
        std::cerr << "Missing: file2.txt from watch2" << std::endl;

    EXPECT_TRUE(fs::exists(destRoot / L"file1.txt"));
    EXPECT_TRUE(fs::exists(destRoot / L"file2.txt"));

    StopBackup();
    CleanupTemp(root);
    return true;
}

// Тест 9: Остановка во время работы
static bool Test_StopBackupWhileRunning() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot));
    EXPECT_TRUE(fs::create_directories(destRoot));

    // Создаём много файлов чтобы синхронизация заняла время
    for (int i = 0; i < 50; i++) {
        WriteTestFile(watchRoot / (L"file" + std::to_wstring(i) + L".txt"), 
                     "Content " + std::to_string(i));
    }

    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    
    // Ждём пока что-то начнёт работать
    bool started = false;
    for (int i = 0; i < 20; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (g_isFullSyncRunning.load() || g_isRunning.load()) {
            started = true;
            break;
        }
    }
    
    if (!started) {
        std::cerr << "Warning: backup didn't start in time" << std::endl;
        // Всё равно проверим что StopBackup безопасен
    }
    
    StopBackup();
    
    // Проверяем что остановился
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_FALSE(g_isRunning.load());
    EXPECT_FALSE(g_isFullSyncRunning.load());

    CleanupTemp(root);
    return true;
}

// Тест 10: Обновление существующего файла
static bool Test_FullSyncUpdatesExistingFile() {
    auto root = MakeTempTestRoot();
    auto watchRoot = root / L"watch";
    auto destRoot = root / L"dest";
    auto sourceFile = watchRoot / L"update.txt";
    auto targetFile = destRoot / L"update.txt";

    CleanupTemp(root);
    EXPECT_TRUE(fs::create_directories(watchRoot));
    EXPECT_TRUE(fs::create_directories(destRoot));

    // Первая версия файла
    WriteTestFile(sourceFile, "Version 1");
    Config::ClearWatchPaths();
    Config::SetWatchPaths({ watchRoot.wstring() });
    Config::Set(L"destPath", destRoot.wstring());
    ResetMainWindowState();

    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(1, 10000));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    StopBackup();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!fs::exists(targetFile))
        std::cerr << "First version doesn't exist" << std::endl;

    // Проверяем первую версию
    EXPECT_TRUE(fs::exists(targetFile));
    
    // Обновляем файл с задержкой
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    WriteTestFile(sourceFile, "Version 2 - Updated content");
    
    // Запускаем снова
    ResetMainWindowState();
    StartBackup(nullptr);
    EXPECT_TRUE(WaitForCopiedCount(1, 10000));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Проверяем что файл обновился
    EXPECT_TRUE(fs::exists(targetFile));
    EXPECT_TRUE(fs::file_size(targetFile) > 10);
    
    StopBackup();
    CleanupTemp(root);
    return true;
}

// ============ MAIN ============

int main() {
    // ВАЖНО: Включаем тестовый режим чтобы отключить MessageBox
    g_testMode = true;
    
    std::cout << "===========================================\n";
    std::cout << "Running MainWindow Comprehensive Tests\n";
    std::cout << "===========================================\n\n";

    struct Test {
        const char* name;
        bool (*func)();
    };

    Test tests[] = {
        {"StopBackup is safe when not running", Test_StopBackupIsSafeWhenNotRunning},
        {"StartBackup performs initial sync", Test_StartBackupPerformsInitialSync},
        {"FullSync handles multiple files", Test_FullSyncMultipleFiles},
        {"FullSync handles nested directories", Test_FullSyncNestedDirectories},
        {"FullSync ignores system files", Test_FullSyncIgnoresSystemFiles},
        {"StartBackup validates empty paths", Test_StartBackupValidatesEmptyPaths},
        {"StartBackup validates non-existent paths", Test_StartBackupValidatesNonExistentPaths},
        {"Multiple watch paths work correctly", Test_MultipleWatchPaths},
        {"StopBackup works while running", Test_StopBackupWhileRunning},
        {"FullSync updates existing files", Test_FullSyncUpdatesExistingFile},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        std::cout << "Running: " << test.name << "... ";
        if (test.func()) {
            std::cout << "PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "FAILED" << std::endl;
            failed++;
        }
    }

    std::cout << "\n===========================================\n";
    std::cout << "Test Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "===========================================\n";

    return failed > 0 ? 1 : 0;
}
