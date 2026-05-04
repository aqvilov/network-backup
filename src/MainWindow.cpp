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
#include <vector>           
#include <mutex>           

#include "../include/Logger.h"
#include "../include/Config.h"
#include "../include/FileUtils.h"
#include "../include/Watcher.h"
#include "../include/BackupQueue.h"

#include <filesystem> 
#include <functional> 
namespace fs = std::filesystem; 

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define ID_BTN_WATCH      101
#define ID_BTN_DEST       102
#define ID_BTN_START      103
#define ID_BTN_STOP       104
#define ID_BTN_OPEN_DEST  105
#define ID_LIST           106
#define ID_TIMER_UI       200
#define ID_BTN_ERRORS     108   

#define WM_LOG_UPDATE     (WM_USER + 1) 
#define WM_STATS_UPDATE   (WM_USER + 2)
#define WM_FULLSYNC_UPDATE (WM_USER + 3) 
#define WM_START_WATCHER  (WM_USER + 4)
#define WM_TRAYICON       (WM_USER + 5)
#define WM_UPDATE_ERRORS  (WM_USER + 6)

static Watcher      g_watcher;
static BackupQueue  g_queue;
static std::thread g_syncThread;
static std::atomic<bool> g_isRunning{false};
static std::atomic<bool> g_isFullSyncRunning{false};

static HWND g_hWnd = nullptr;
static HWND g_hList = nullptr;
static HWND g_hLblWatch = nullptr;
static HWND g_hLblDest = nullptr;
static HWND g_hLblStatus = nullptr;
static HWND g_hLblStats = nullptr;
static HWND g_hBtnStart = nullptr;
static HWND g_hBtnStop = nullptr;
static HWND g_hBtnOpenDest = nullptr;
static HFONT g_hFont = nullptr;
static NOTIFYICONDATA nid = {};
static HMENU hTrayMenu = nullptr;

struct ErrorEntry {
    std::wstring path;
    std::wstring reason;
};
static std::vector<ErrorEntry> g_errors;
static std::mutex g_errorsMutex;    
static HWND g_hErrorWnd = nullptr;
static HWND g_hErrorList = nullptr;
static HWND g_hBtnRetrySel = nullptr;
static HWND g_hBtnRetryAll = nullptr;
static void CreateErrorWindow();
static void ShowErrorWindow();
static void UpdateErrorList();
static void ClearErrors();
static void RetrySelectedError();
static void RetryAllErrors();
static LRESULT CALLBACK ErrorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

    // папка слежки
    MakeLabel(L"Слежка:", 10, 12, 70, 20);
    g_hLblWatch = MakeLabel(Config::Get(L"watchPath", L"не выбрана").c_str(),
        85, 12, 380, 20, (HMENU)ID_BTN_WATCH);
    MakeButton(L"Выбрать...", 472, 8, 100, 26, (HMENU)ID_BTN_WATCH);

    // папка бэкапа
    MakeLabel(L"Бэкап:", 10, 42, 70, 20);
    g_hLblDest = MakeLabel(Config::Get(L"destPath", L"не выбрана").c_str(),
        85, 42, 380, 20, (HMENU)ID_BTN_DEST);
    MakeButton(L"Выбрать...", 472, 38, 100, 26, (HMENU)ID_BTN_DEST);

    // кнопки управления
    g_hBtnStart = MakeButton(L"▶  Старт", 10, 75, 120, 30, (HMENU)ID_BTN_START);
    g_hBtnStop = MakeButton(L"■  Стоп", 140, 75, 120, 30, (HMENU)ID_BTN_STOP);
    g_hBtnOpenDest = MakeButton(L"📁 Открыть бэкап", 280, 75, 150, 30, (HMENU)ID_BTN_OPEN_DEST);
    // кнопка "Ошибки"
    MakeButton(L"⚠ Ошибки", 440, 75, 100, 30, (HMENU)ID_BTN_ERRORS);
    EnableWindow(g_hBtnStop, FALSE);
    EnableWindow(g_hBtnOpenDest, FALSE);

    //статус
    g_hLblStatus = MakeLabel(L"Ожидание...", 10, 115, 565, 20);
    g_hLblStats = MakeLabel(L"", 10, 135, 565, 18);

    // список событий
    MakeLabel(L"Журнал событий:", 10, 160, 200, 18);
    g_hList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS,
        10, 180, 565, 265, hWnd, (HMENU)ID_LIST, nullptr, nullptr);
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
}

static void AddTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION); 
    wcscpy_s(nid.szTip, L"Network Backup");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void CreateTrayMenu(HWND hWnd){
    hTrayMenu = CreatePopupMenu();
    AppendMenuW(hTrayMenu, MF_STRING, 2, L"Скрыть");
    AppendMenuW(hTrayMenu, MF_STRING, 3, L"Выйти");
}

static void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

//Функиця Полной Синхронизации.
static void FullSync(const std::wstring& watchRoot, const std::wstring& destDir, HWND hWnd) {
    uint64_t fileCount = 0;

    auto finishWithError = [&](const std::wstring& errorMsg) {
        g_isFullSyncRunning.store(false);
        PostMessageW(hWnd, WM_FULLSYNC_UPDATE, (WPARAM)(new std::wstring(L"Ошибка: " + errorMsg)), 0);
        EnableWindow(g_hBtnStart, TRUE);
    };

    try {
        fs::path root(watchRoot);
        if (!fs::exists(root)) {
            finishWithError(L"Ошибка: папка слежки не существует");
            return;
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

        if (!g_isFullSyncRunning.load()) {
            finishWithError(L"Синхронизация прервана пользователем");
            return;
        }

        wchar_t finalMsg[256];
        swprintf_s(finalMsg, L"Сканирование завершено. Файлов добавлено в очередь: %llu", fileCount);
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
    std::wstring watch = Config::Get(L"watchPath");
    std::wstring dest = Config::Get(L"destPath");

    if (watch.empty() || dest.empty()) 
    {
        MessageBoxW(hWnd, L"Выберите обе папки.", L"Ошибка", MB_ICONWARNING);
        return;
    }
    if (watch == dest) 
    {
        MessageBoxW(hWnd, L"Папки должны быть разными.", L"Ошибка", MB_ICONWARNING);
        return;
    }
    if (!fs::exists(watch)) {
        MessageBoxW(hWnd, L"Папка слежки не существует", L"Ошибка", MB_ICONERROR);
        return;
    }
    if (!fs::exists(dest)) {
        MessageBoxW(hWnd, L"Папка назначения не существует", L"Ошибка", MB_ICONERROR);
        return;
    }
    if (g_isFullSyncRunning.load() || g_isRunning.load()) return;
    
    if (g_syncThread.joinable())
        g_syncThread.join();

    //очищаем список старых ошибок при новом запуске
    ClearErrors();

    g_isFullSyncRunning.store(true);

    if (!g_queue.Start(watch, dest, [](const std::wstring& path, bool ok, uint64_t bytes, const std::wstring& errorMsg) {
        struct Payload { std::wstring path; bool ok; uint64_t bytes; std::wstring reason; };
        auto* p = new Payload{ path, ok, bytes, errorMsg };
        PostMessageW(g_hWnd, WM_LOG_UPDATE, (WPARAM)p, 0);
    })) {
        MessageBoxW(hWnd, L"Не удалось запустить очередь копирования", L"Ошибка", MB_ICONERROR);
        g_isFullSyncRunning.store(false);
        return;
    }

    EnableWindow(g_hBtnStart, FALSE);
    EnableWindow(g_hBtnStop, FALSE);
    SetWindowTextW(g_hLblStatus, L"Выполняется первоначальная синхронизация...");

    g_syncThread = std::thread([hWnd, watch, dest]() {
        FullSync(watch, dest, hWnd);
    });
}

static void ActuallyStartWatcher() {
    Logger::Info(L"[DEBUG] ActuallyStartWatcher вызвана");
    
    g_isFullSyncRunning.store(false);
    
    if (g_isRunning.load()) {
        Logger::Info(L"[DEBUG] Watcher уже запущен, выходим");
        return;
    }

    std::wstring watch = Config::Get(L"watchPath");
    std::wstring dest = Config::Get(L"destPath");
    Logger::Info(L"[DEBUG] Попытка запустить watcher для: " + watch);

    bool started = g_watcher.Start(watch, true,
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
        g_queue.Stop();
        MessageBoxW(g_hWnd, L"Не удалось открыть папку слежки.", L"Ошибка", MB_ICONERROR);
        EnableWindow(g_hBtnStart, TRUE);
        g_isRunning.store(false);
        return;
    }

    g_isRunning.store(true);
    g_isFullSyncRunning.store(false);
    SetWindowTextW(g_hLblStatus, (L"Слежу: " + watch).c_str());
    EnableWindow(g_hBtnStop, TRUE);
    EnableWindow(g_hBtnOpenDest, TRUE);
    Logger::Info(L"Запущен. Слежка: " + watch + L" → " + dest);
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
    
    g_watcher.Stop();
    g_queue.Stop();
    g_isRunning.store(false);
    g_isFullSyncRunning.store(false);
    
    SetWindowTextW(g_hLblStatus, L"Остановлено.");
    EnableWindow(g_hBtnStart, TRUE);
    EnableWindow(g_hBtnStop, FALSE);
    
    Logger::Info(L"Остановлено.");
}

// оконная процедура для окна ошибок
static LRESULT CALLBACK ErrorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        {
            g_hErrorList = CreateWindowW(WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                10, 10, 460, 250, hWnd, nullptr, nullptr, nullptr);
            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 200;
            col.pszText = (LPWSTR)L"Файл";
            ListView_InsertColumn(g_hErrorList, 0, &col);
            col.cx = 260;
            col.pszText = (LPWSTR)L"Причина";
            ListView_InsertColumn(g_hErrorList, 1, &col);
            
            // Кнопки
            g_hBtnRetrySel = CreateWindowW(L"BUTTON", L"Повторить выбранное",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 270, 150, 30, hWnd, (HMENU)1, nullptr, nullptr);
            g_hBtnRetryAll = CreateWindowW(L"BUTTON", L"Повторить все",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 270, 130, 30, hWnd, (HMENU)2, nullptr, nullptr);
            
            UpdateErrorList();
        }
        break;
        
    case WM_SIZE:
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            SetWindowPos(g_hErrorList, nullptr, 10, 10, rc.right-20, rc.bottom-70, SWP_NOZORDER);
            SetWindowPos(g_hBtnRetrySel, nullptr, 10, rc.bottom-50, 150, 30, SWP_NOZORDER);
            SetWindowPos(g_hBtnRetryAll, nullptr, 170, rc.bottom-50, 130, 30, SWP_NOZORDER);
        }
        break;
        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1:
            RetrySelectedError();
            break;
        case 2:
            RetryAllErrors();
            break;
        }
        break;
        
    case WM_UPDATE_ERRORS:
        UpdateErrorList();
        break;
        
    case WM_DESTROY:
        g_hErrorWnd = nullptr;
        g_hErrorList = nullptr;
        break;
        
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

static void CreateErrorWindow() {
    if (g_hErrorWnd && IsWindow(g_hErrorWnd)) {
        ShowWindow(g_hErrorWnd, SW_SHOW);
        SetForegroundWindow(g_hErrorWnd);
        return;
    }
    g_hErrorWnd = CreateWindowW(L"NetBackupErrorWindow", L"Список ошибок",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    ShowWindow(g_hErrorWnd, SW_SHOW);
}

static void ShowErrorWindow() {
    if (!g_hErrorWnd || !IsWindow(g_hErrorWnd))
        CreateErrorWindow();
    else {
        ShowWindow(g_hErrorWnd, SW_SHOW);
        SetForegroundWindow(g_hErrorWnd);
    }
}

static void UpdateErrorList() {
    if (!g_hErrorList) return;
    ListView_DeleteAllItems(g_hErrorList);
    
    std::lock_guard<std::mutex> lock(g_errorsMutex);
    int idx = 0;
    for (const auto& err : g_errors) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.pszText = (LPWSTR)err.path.c_str();
        ListView_InsertItem(g_hErrorList, &item);
        ListView_SetItemText(g_hErrorList, idx, 1, (LPWSTR)err.reason.c_str());
        ++idx;
    }
}

static void ClearErrors() {
    {
        std::lock_guard<std::mutex> lock(g_errorsMutex);
        g_errors.clear();
    }
    UpdateErrorList();
}

static void RetrySelectedError() {
    if (!g_hErrorList) return;
    int sel = ListView_GetNextItem(g_hErrorList, -1, LVNI_SELECTED);
    if (sel == -1) return;
    
    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(g_errorsMutex);
        if (sel < (int)g_errors.size()) {
            path = g_errors[sel].path;
            g_errors.erase(g_errors.begin() + sel);
        }
    }
    if (!path.empty()) {
        g_queue.Enqueue(path);
        UpdateErrorList();
    }
}

static void RetryAllErrors() {
    std::vector<std::wstring> paths;
    {
        std::lock_guard<std::mutex> lock(g_errorsMutex);
        for (const auto& err : g_errors)
            paths.push_back(err.path);
        g_errors.clear();
    }
    for (const auto& p : paths)
        g_queue.Enqueue(p);
    UpdateErrorList();
}

// КАЛЛ БЕК главного окна
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        g_hWnd = hWnd;
        CreateControls(hWnd);
        SetTimer(hWnd, ID_TIMER_UI, 500, nullptr);
        CreateTrayMenu(hWnd);
        AddTrayIcon(hWnd);
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_UI) RefreshStats();
        break;
    
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
            PostMessage(hWnd, WM_NULL, 0, 0);
        } else if (lParam == WM_LBUTTONDBLCLK){
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_WATCH: {
            std::wstring p = PickFolder(hWnd, L"Папка для слежки");
            if (!p.empty()) {
                Config::Set(L"watchPath", p);
                Config::Save();
                SetWindowTextW(g_hLblWatch, p.c_str());
            }
            break;
        }
        case ID_BTN_DEST: {
            std::wstring p = PickFolder(hWnd, L"Папка для бэкапа");
            if (!p.empty()) {
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
        case ID_BTN_ERRORS:
            ShowErrorWindow();
            break;
        case 1:
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
            break;
        case 2:
            ShowWindow(hWnd, SW_HIDE);
            break;
        case 3:
            RemoveTrayIcon();
            DestroyWindow(hWnd);
            break;
        }
        break;

    case WM_LOG_UPDATE: {
        struct Payload { std::wstring path; bool ok; uint64_t bytes; std::wstring reason; };
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

        //если ошибка, добавляем в список ошибок (без дублей)
        if (!p->ok) {
            std::lock_guard<std::mutex> lock(g_errorsMutex);
            bool exists = false;
            for (const auto& e : g_errors) {
                if (e.path == p->path && e.reason == p->reason) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                g_errors.push_back({p->path, p->reason});
                if (g_hErrorWnd && IsWindow(g_hErrorWnd)) {
                    PostMessage(g_hErrorWnd, WM_UPDATE_ERRORS, 0, 0);
                }
            }
        }

        delete p;
        break;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        if (g_hList)
            SetWindowPos(g_hList, nullptr, 10, 180,
                rc.right - 20, rc.bottom - 190, SWP_NOZORDER);
        break;
    }

    case WM_FULLSYNC_UPDATE:   
    {
        std::wstring* msg = (std::wstring*)wParam;
        SetWindowTextW(g_hLblStatus, msg->c_str());
        delete msg;
        break;
    }

    case WM_START_WATCHER: 
    {
        ActuallyStartWatcher();
        break;
    }
    
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_UI);
        if (g_isRunning) StopBackup();
        RemoveTrayIcon();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitialize(nullptr);

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"NetBackup_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"NetBackupMVP", nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    std::wstring appDir = FileUtils::GetAppDataDir();
    CreateDirectoryW(appDir.c_str(), nullptr);

    if (GetLastError() != ERROR_ALREADY_EXISTS && GetLastError() != 0) {
        MessageBoxW(nullptr, L"Не удалось создать папку для настроек", L"Ошибка", MB_ICONERROR);
    }

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

    WNDCLASSW wcErrors = {};
    wcErrors.lpfnWndProc = ErrorWndProc;
    wcErrors.hInstance = hInstance;
    wcErrors.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcErrors.lpszClassName = L"NetBackupErrorWindow";
    wcErrors.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wcErrors);

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

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Logger::Info(L"=== NetBackup завершён ===");
    CloseHandle(hMutex);
    CoUninitialize();
    return (int)msg.wParam;
}