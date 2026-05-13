// тут юзаем WinAPI
// содержит весь UI

#pragma execution_character_set("utf-8")
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <atomic>
#include <stack>

#include "../include/Logger.h"
#include "../include/Config.h"
#include "../include/FileUtils.h"
#include "../include/Watcher.h"
#include "../include/BackupQueue.h"

#include <filesystem>
#include <functional>
#include <memory>
namespace fs = std::filesystem; 

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define ID_BTN_WATCH_ADD  101
#define ID_BTN_WATCH_REMOVE 102
#define ID_BTN_DEST       103
#define ID_BTN_START      104
#define ID_BTN_STOP       105
#define ID_BTN_OPEN_DEST  106
#define ID_LIST_WATCH     107
#define ID_LIST           108
#define ID_TIMER_UI       200

#define WM_LOG_UPDATE  (WM_USER + 1) 
#define WM_STATS_UPDATE (WM_USER + 2)
#define WM_FULLSYNC_UPDATE   (WM_USER + 3) 
#define WM_START_WATCHER     (WM_USER + 4)

static std::vector<std::unique_ptr<Watcher>> g_watchers;
static BackupQueue  g_queue;
static std::thread g_syncThread;
static std::atomic<bool> g_isRunning{false};
static std::atomic<bool> g_isFullSyncRunning{false};

static HWND g_hWnd = nullptr;
static HWND g_hListWatch = nullptr;
static HWND g_hList = nullptr;
static HWND g_hLblDest = nullptr;
static HWND g_hLblStatus = nullptr;
static HWND g_hLblStats = nullptr;
static HWND g_hBtnStart = nullptr;
static HWND g_hBtnStop = nullptr;
static HWND g_hBtnOpenDest = nullptr;
static HWND g_hBtnWatchAdd = nullptr;
static HWND g_hBtnWatchRemove = nullptr;
static HFONT g_hFont = nullptr;



// выбор папки
static std::wstring PickFolder(HWND owner, const wchar_t* title) {
    wchar_t buf[MAX_PATH] = {};
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        SHGetPathFromIDListW(pidl, buf);
        CoTaskMemFree(pidl);
    }
    return buf;
}

static void ListAddItem(const std::wstring& text) {
    if (!g_hList) return;
    int count = ListView_GetItemCount(g_hList);
    LVITEMW item = {};
    item.mask = LVIF_TEXT;
    item.iItem = count;
    item.pszText = (LPWSTR)text.c_str();
    ListView_InsertItem(g_hList, &item);
    ListView_EnsureVisible(g_hList, count, FALSE);

    if (count > 500)
        ListView_DeleteItem(g_hList, 0);
}

static void RefreshStats() {
    auto stats = g_queue.GetStats();
    std::wostringstream ss;
    ss << L"Скопировано: " << stats.copied
        << L"  |  Ошибок: " << stats.errors
        << L"  |  Трафик: " << FileUtils::FormatSize(stats.bytes)
        << L"  |  В очереди: " << stats.queued;
    SetWindowTextW(g_hLblStats, ss.str().c_str());
}

static void CreateControls(HWND hWnd) {
    g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto MakeLabel = [&](const wchar_t* text, int x, int y, int cx, int cy, HMENU id = nullptr) {
        HWND hw = CreateWindowW(L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            x, y, cx, cy, hWnd, id, nullptr, nullptr);
        SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        return hw;
        };
    auto MakeButton = [&](const wchar_t* text, int x, int y, int cx, int cy, HMENU id) {
        HWND hw = CreateWindowW(L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, cx, cy, hWnd, id, nullptr, nullptr);
        SendMessageW(hw, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        return hw;
        };

    // папки слежки (список)
    MakeLabel(L"Папки для слежки:", 10, 12, 200, 20);
    g_hListWatch = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        10, 35, 465, 80, hWnd, (HMENU)ID_LIST_WATCH, nullptr, nullptr);
    SendMessageW(g_hListWatch, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 460;
    col.pszText = (LPWSTR)L"Путь";
    ListView_InsertColumn(g_hListWatch, 0, &col);
    
    // Запрещаем изменение размера столбцов
    HWND hHeaderWatch = ListView_GetHeader(g_hListWatch);
    if (hHeaderWatch) {
        LONG styleWatch = GetWindowLongW(hHeaderWatch, GWL_STYLE);
        SetWindowLongW(hHeaderWatch, GWL_STYLE, styleWatch | HDS_NOSIZING);
    }
    
    g_hBtnWatchAdd = MakeButton(L"+ Добавить папку", 480, 35, 90, 26, (HMENU)ID_BTN_WATCH_ADD);
    g_hBtnWatchRemove = MakeButton(L"- Удалить", 480, 65, 90, 26, (HMENU)ID_BTN_WATCH_REMOVE);

    // Загружаем сохраненные папки
    auto watchPaths = Config::GetWatchPaths();
    for (const auto& path : watchPaths) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(g_hListWatch);
        item.pszText = (LPWSTR)path.c_str();
        ListView_InsertItem(g_hListWatch, &item);
    }

    // папка бэкапа
    MakeLabel(L"Бэкап:", 10, 125, 70, 20);
    g_hLblDest = MakeLabel(Config::Get(L"destPath", L"не выбрана").c_str(),
        85, 125, 380, 20, (HMENU)ID_BTN_DEST);
    MakeButton(L"Выбрать...", 472, 121, 100, 26, (HMENU)ID_BTN_DEST);

    // кнопки управления
    g_hBtnStart = MakeButton(L"▶  Старт", 10, 158, 120, 30, (HMENU)ID_BTN_START);
    g_hBtnStop = MakeButton(L"■  Стоп", 140, 158, 120, 30, (HMENU)ID_BTN_STOP);
    g_hBtnOpenDest = MakeButton(L"📁 Открыть бэкап", 280, 158, 150, 30, (HMENU)ID_BTN_OPEN_DEST);
    EnableWindow(g_hBtnStop, FALSE);
    EnableWindow(g_hBtnOpenDest, FALSE);

    //статус
    g_hLblStatus = MakeLabel(L"Ожидание...", 10, 198, 565, 20);
    g_hLblStats = MakeLabel(L"", 10, 218, 565, 18);

    // список событий
    MakeLabel(L"Журнал событий:", 10, 243, 200, 18);
    g_hList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS,
        10, 263, 565, 182, hWnd, (HMENU)ID_LIST, nullptr, nullptr);
    SendMessageW(g_hList, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // крлонки ListView
    auto AddCol = [](HWND hList, const wchar_t* text, int width, int idx) {
        LVCOLUMNW c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.cx = width;
        c.pszText = (LPWSTR)text;
        ListView_InsertColumn(hList, idx, &c);
        };
    AddCol(g_hList, L"Время", 70, 0);
    AddCol(g_hList, L"Статус", 60, 1);
    AddCol(g_hList, L"Файл", 432, 2);

    // Запрещаем изменение размера столбцов
    HWND hHeader = ListView_GetHeader(g_hList);
    if (hHeader) {
        LONG style = GetWindowLongW(hHeader, GWL_STYLE);
        SetWindowLongW(hHeader, GWL_STYLE, style | HDS_NOSIZING);
    }
}

//Функиця Полной Синхронизации.
static void FullSync(const std::vector<std::wstring>& watchRoots, const std::wstring& destDir, HWND hWnd) {
    uint64_t fileCount = 0;

    auto finishWithError = [&](const std::wstring& errorMsg) {
        g_isFullSyncRunning.store(false);
        PostMessageW(hWnd, WM_FULLSYNC_UPDATE, (WPARAM)(new std::wstring(L"Ошибка: " + errorMsg)), 0);
        EnableWindow(g_hBtnStart, TRUE);
    };

    try {
        for (const auto& watchRoot : watchRoots) {
            if (!g_isFullSyncRunning.load()) return;
            
            fs::path root(watchRoot);
            if (!fs::exists(root)) {
                Logger::Warning(L"Папка слежки не существует: " + watchRoot);
                continue;
            }

            std::stack<fs::path> directories;
            directories.push(root);

            while (!directories.empty() && g_isFullSyncRunning.load()) {
                fs::path current = directories.top();
                directories.pop();

                for (const auto& entry : fs::directory_iterator(current)) {
                    if (!g_isFullSyncRunning.load()) return;

                    if (entry.is_directory()) {
                        directories.push(entry.path());
                    }
                    else if (entry.is_regular_file()) {
                        std::wstring src = entry.path().wstring();
                        std::wstring fname = entry.path().filename().wstring();
                        if (!fname.empty() && fname[0] == L'~') continue;
                        if (fname == L"desktop.ini" || fname == L"thumbs.db") continue;

                        try {
                            fs::path srcPath(src);
                            fs::path rootPath(watchRoot);
                            fs::path destDirPath(destDir);
                            
                            std::wstring relative = FileUtils::GetRelativePath(src, watchRoot);
                            fs::path targetPath = destDirPath / relative;
                            
                            if (fs::exists(targetPath)) {
                                auto srcTime = fs::last_write_time(srcPath);
                                auto targetTime = fs::last_write_time(targetPath);
                                auto srcSize = fs::file_size(srcPath);
                                auto targetSize = fs::file_size(targetPath);
                                
                                if (srcSize == targetSize && srcTime == targetTime) {
                                    continue;
                                }
                            }
                        } catch (...) {
                        }

                        g_queue.Enqueue(src);
                        fileCount++;
                        wchar_t buf[256];
                        swprintf_s(buf, L"Синхронизация: %llu файлов добавлено в очередь", fileCount);
                        if (g_isFullSyncRunning.load()) {
                            PostMessageW(hWnd, WM_FULLSYNC_UPDATE, (WPARAM)(new std::wstring(buf)), 0);
                        }
                    }
                }
            }
        }

        if (!g_isFullSyncRunning.load()) {
            finishWithError(L"Синхронизация прервана пользователем");
            return;
        }

        wchar_t finalMsg[256];
        swprintf_s(finalMsg, L"Сканирование завершено. Файлов добавлено в очередь: %llu",
                   fileCount);
        Logger::Info(finalMsg);
        g_isFullSyncRunning.store(false);
        PostMessageW(hWnd, WM_FULLSYNC_UPDATE, (WPARAM)(new std::wstring(finalMsg)), 0);
        Sleep(100);
        PostMessageW(hWnd, WM_START_WATCHER, 0, 0);
    }
    catch (const std::exception& e) {
        std::wstring err = L"Исключение: " + FileUtils::Utf8ToWide(e.what());
        Logger::Error(err);
        finishWithError(err);
    }
    catch (...) {
        finishWithError(L"Неизвестное исключение");
    }
}


static void StartBackup(HWND hWnd) {
    auto watchPaths = Config::GetWatchPaths();
    std::wstring dest = Config::Get(L"destPath");

    if (watchPaths.empty() || dest.empty()) 
    {
        MessageBoxW(hWnd, L"Выберите хотя бы одну папку для слежки и папку для бэкапа.", L"Ошибка", MB_ICONWARNING);
        return;
    }
    
    // Проверка что папка бэкапа не совпадает с папками слежки
    for (const auto& watch : watchPaths) {
        if (watch == dest) 
        {
            MessageBoxW(hWnd, L"Папки слежки и бэкапа должны быть разными.", L"Ошибка", MB_ICONWARNING);
            return;
        }
    }
    
    // Проверка что папки не являются подпапками друг друга
    for (const auto& watch : watchPaths) {
        if (FileUtils::IsSubdirectory(watch, dest)) {
            MessageBoxW(hWnd, 
                L"Папка бэкапа не может быть внутри папки слежки!\n"
                L"Это приведёт к бесконечному копированию.",
                L"Ошибка",
                MB_ICONERROR);
            return;
        }
        if (FileUtils::IsSubdirectory(dest, watch)) {
            MessageBoxW(hWnd, 
                L"Папка слежки не может быть внутри папки бэкапа!",
                L"Ошибка",
                MB_ICONERROR);
            return;
        }
    }
    
    // Проверка существования папок
    for (const auto& watch : watchPaths) {
        if (!fs::exists(watch)) {
            MessageBoxW(hWnd, (L"Папка слежки не существует: " + watch).c_str(), L"Ошибка", MB_ICONERROR);
            return;
        }
    }
    
    if (!fs::exists(dest)) {
        MessageBoxW(hWnd, L"Папка назначения не существует", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    if (g_isFullSyncRunning.load() || g_isRunning.load()) return;
    
    if (g_syncThread.joinable())
        g_syncThread.join();

    g_isFullSyncRunning.store(true);

    if (!g_queue.Start(dest, [](const std::wstring& path, bool ok, uint64_t bytes) {
        struct Payload { std::wstring path; bool ok; uint64_t bytes; };
        auto* p = new Payload{ path, ok, bytes };
        PostMessageW(g_hWnd, WM_LOG_UPDATE, (WPARAM)p, 0);
    })) {
        MessageBoxW(hWnd, L"Не удалось запустить очередь копирования", L"Ошибка", MB_ICONERROR);
        g_isFullSyncRunning.store(false);
        return;
    }

    // Устанавливаем корневые папки для очереди
    g_queue.SetWatchRoots(watchPaths);

    EnableWindow(g_hBtnStart, FALSE);
    EnableWindow(g_hBtnStop, FALSE);
    SetWindowTextW(g_hLblStatus, L"Выполняется первоначальная синхронизация...");

    g_syncThread = std::thread([hWnd, watchPaths, dest]() {
        FullSync(watchPaths, dest, hWnd);
    });
}

static void ActuallyStartWatcher()
{
    Logger::Info(L"[DEBUG] ActuallyStartWatcher вызвана");
    
    g_isFullSyncRunning.store(false);
    
    if (g_isRunning.load()) {
        Logger::Info(L"[DEBUG] Watcher уже запущен, выходим");
        return;
    }

    auto watchPaths = Config::GetWatchPaths();
    if (watchPaths.empty()) {
        MessageBoxW(g_hWnd, L"Нет папок для слежки.", L"Ошибка", MB_ICONERROR);
        EnableWindow(g_hBtnStart, TRUE);
        return;
    }

    std::wstring dest = Config::Get(L"destPath");
    Logger::Info(L"[DEBUG] Попытка запустить watchers для " + std::to_wstring(watchPaths.size()) + L" папок");

    // Очищаем старые watcher'ы
    g_watchers.clear();

    bool allStarted = true;
    for (size_t i = 0; i < watchPaths.size(); ++i) {
        const auto& watch = watchPaths[i];
        
        // Создаем новый Watcher через unique_ptr
        auto watcher = std::make_unique<Watcher>();
        bool started = watcher->Start(watch, true,
            [](FileAction action, const std::wstring& path) 
            {
                if (action == FileAction::Added || action == FileAction::Modified || action == FileAction::Renamed) 
                {
                    g_queue.Enqueue(path);
                } else if (action == FileAction::Deleted) 
                {
                    Logger::Info(L"Удалён: " + path);
                }
            }
        );

        if (!started) 
        {
            Logger::Error(L"Не удалось запустить слежку для: " + watch);
            allStarted = false;
        } else {
            // Добавляем watcher в вектор только если успешно запущен
            g_watchers.push_back(std::move(watcher));
        }
    }

    if (!allStarted) 
    {
        // Останавливаем все запущенные watcher'ы
        g_watchers.clear();
        g_queue.Stop();
        MessageBoxW(g_hWnd, L"Не удалось открыть одну или несколько папок слежки.", L"Ошибка", MB_ICONERROR);
        EnableWindow(g_hBtnStart, TRUE);
        g_isRunning.store(false);
        return;
    }

    g_isRunning.store(true);
    g_isFullSyncRunning.store(false);
    
    // Формируем строку статуса
    std::wstring statusText = L"Слежу: ";
    for (size_t i = 0; i < watchPaths.size(); ++i) {
        if (i > 0) statusText += L", ";
        statusText += watchPaths[i];
    }
    SetWindowTextW(g_hLblStatus, statusText.c_str());
    
    EnableWindow(g_hBtnStop, TRUE);
    EnableWindow(g_hBtnOpenDest, TRUE);
    Logger::Info(L"Запущен. Слежка за " + std::to_wstring(watchPaths.size()) + L" папками → " + dest);
}

static void StopBackup() {
    if (g_isFullSyncRunning.load()) 
    {
        g_isFullSyncRunning.store(false);
        if (g_syncThread.joinable())
            g_syncThread.join();
    }
    
    if (g_syncThread.joinable())
        g_syncThread.join();
    
    // Останавливаем все watcher'ы (Stop() вызывается автоматически в деструкторе)
    g_watchers.clear();
    
    g_queue.Stop();
    g_isRunning.store(false);
    g_isFullSyncRunning.store(false);
    
    SetWindowTextW(g_hLblStatus, L"Остановлено.");
    EnableWindow(g_hBtnStart, TRUE);
    EnableWindow(g_hBtnStop, FALSE);
    
    Logger::Info(L"Остановлено.");
}

//КАЛЛ БЕК
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        g_hWnd = hWnd;
        CreateControls(hWnd);
        // Таймер обновления статистики каждые 500ms
        SetTimer(hWnd, ID_TIMER_UI, 500, nullptr);
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_UI) RefreshStats();
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case ID_BTN_WATCH_ADD: {
            std::wstring p = PickFolder(hWnd, L"Папка для слежки");
            if (!p.empty()) {
                // Проверка на корень диска и защищенные пути
                if (FileUtils::IsRootOrProtectedPath(p)) {
                    MessageBoxW(hWnd, 
                        L"Нельзя выбрать корень диска (C:\\, D:\\), рабочий стол или системные папки!\n"
                        L"Выберите конкретную подпапку.",
                        L"Недопустимая папка",
                        MB_ICONWARNING);
                    break;
                }
                
                // Проверка что папка не добавлена уже
                auto watchPaths = Config::GetWatchPaths();
                bool alreadyExists = false;
                for (const auto& existing : watchPaths) {
                    if (existing == p) {
                        alreadyExists = true;
                        break;
                    }
                }
                
                if (alreadyExists) {
                    MessageBoxW(hWnd, L"Эта папка уже добавлена в список слежки.", L"Предупреждение", MB_ICONWARNING);
                    break;
                }
                
                Config::AddWatchPath(p);
                Config::Save();
                
                LVITEMW item = {};
                item.mask = LVIF_TEXT;
                item.iItem = ListView_GetItemCount(g_hListWatch);
                item.pszText = (LPWSTR)p.c_str();
                ListView_InsertItem(g_hListWatch, &item);
            }
            break;
        }
        
        case ID_BTN_WATCH_REMOVE: {
            int selected = ListView_GetNextItem(g_hListWatch, -1, LVNI_SELECTED);
            if (selected >= 0) {
                Config::RemoveWatchPath(selected);
                Config::Save();
                ListView_DeleteItem(g_hListWatch, selected);
            } else {
                MessageBoxW(hWnd, L"Выберите папку из списка для удаления.", L"Предупреждение", MB_ICONWARNING);
            }
            break;
        }
        
        case ID_BTN_DEST: {
            std::wstring p = PickFolder(hWnd, L"Папка для бэкапа");
            if (!p.empty()) {
                // Проверка на корень диска и защищенные пути
                if (FileUtils::IsRootOrProtectedPath(p)) {
                    MessageBoxW(hWnd, 
                        L"Нельзя выбрать корень диска (C:\\, D:\\), рабочий стол или системные папки!\n"
                        L"Выберите конкретную подпапку.",
                        L"Недопустимая папка",
                        MB_ICONWARNING);
                    break;
                }
                
                Config::Set(L"destPath", p);
                Config::Save();
                SetWindowTextW(g_hLblDest, p.c_str());
            }
            break;
        }
        case ID_BTN_START:    StartBackup(hWnd); break;
        case ID_BTN_STOP:     StopBackup();      break;
        case ID_BTN_OPEN_DEST: {
            std::wstring dest = Config::Get(L"destPath");
            if (!dest.empty())
                ShellExecuteW(nullptr, L"open", dest.c_str(), nullptr, nullptr, SW_SHOW);
            break;
        }
        }
        break;


    case WM_LOG_UPDATE: {
        struct Payload { std::wstring path; bool ok; uint64_t bytes; };
        auto* p = reinterpret_cast<Payload*>(wParam);


        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t timeBuf[16];
        swprintf_s(timeBuf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

        std::wstring fname = p->path;
        size_t slash = fname.find_last_of(L"\\/");
        if (slash != std::wstring::npos) fname = fname.substr(slash + 1);

        int row = ListView_GetItemCount(g_hList);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = timeBuf;
        ListView_InsertItem(g_hList, &item);

        std::wstring status = p->ok ? L"✓ OK" : L"✗ ERR";
        ListView_SetItemText(g_hList, row, 1, (LPWSTR)status.c_str());
        ListView_SetItemText(g_hList, row, 2, (LPWSTR)p->path.c_str());
        ListView_EnsureVisible(g_hList, row, FALSE);

        if (row > 500) ListView_DeleteItem(g_hList, 0);

        delete p;
        break;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        if (g_hList)
            SetWindowPos(g_hList, nullptr, 10, 263,
                rc.right - 20, rc.bottom - 273, SWP_NOZORDER);
        break;
    }

    //Обнолвение Полной синхронизации
    case WM_FULLSYNC_UPDATE:   
    {
        std::wstring* msg = (std::wstring*)wParam;
        SetWindowTextW(g_hLblStatus, msg->c_str());
        delete msg;
        break;
    }
    //Иницализация Watchera крч
    case WM_START_WATCHER: 
    {
        ActuallyStartWatcher();
        break;
    }
    
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_UI);
        if (g_isRunning) StopBackup();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitialize(nullptr);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    // Папка настроек %APPDATA%\NetBackup
    std::wstring appDir = FileUtils::GetAppDataDir();
    CreateDirectoryW(appDir.c_str(), nullptr);

    if (GetLastError() != ERROR_ALREADY_EXISTS && GetLastError() != 0) {
        MessageBoxW(nullptr, L"Не удалось создать папку для настроек", L"Ошибка", MB_ICONERROR);
    }

    // Инициализация логгера и конфига
    Logger::Init(appDir + L"\\backup.log");
    Config::Load(appDir + L"\\config.ini");

    Logger::Info(L"=== NetBackup запущен ===");


    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"NetBackupMVP";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);

    // создаём окно
    HWND hWnd = CreateWindowW(
        L"NetBackupMVP",
        L"NetBackup — Локальное хранение",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // мейн цикл
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Logger::Info(L"=== NetBackup завершён ===");
    CoUninitialize();
    return (int)msg.wParam;
}