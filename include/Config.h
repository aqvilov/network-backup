#pragma once
// Config.h — сохранение и загрузка настроек в простой текстовый файл
// Формат: ключ=значение, по одному на строку

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>

class Config {
public:
    // Загрузить настройки из файла
    static bool Load(const std::wstring& path) {
        s_path = path;
        std::wifstream f(path);
        if (!f.is_open()) return false;

        std::wstring line;
        while (std::getline(f, line)) {
            // Пропускаем пустые строки и комментарии (#)
            if (line.empty() || line[0] == L'#') continue;
            auto pos = line.find(L'=');
            if (pos == std::wstring::npos) continue;
            std::wstring key = line.substr(0, pos);
            std::wstring val = line.substr(pos + 1);
            s_data[key] = val;
        }
        return true;
    }

    // Сохранить настройки в файл
    static bool Save() {
        std::wofstream f(s_path);
        if (!f.is_open()) return false;
        f << L"# NetBackup config\n";
        for (auto& [k, v] : s_data)
            f << k << L"=" << v << L"\n";
        return true;
    }

    static void        Set(const std::wstring& key, const std::wstring& val) { s_data[key] = val; }
    static std::wstring Get(const std::wstring& key, const std::wstring& def = L"") {
        auto it = s_data.find(key);
        return it != s_data.end() ? it->second : def;
    }
    static bool Has(const std::wstring& key) { return s_data.count(key) > 0; }

    // Методы для работы с массивом путей слежки
    static void SetWatchPaths(const std::vector<std::wstring>& paths) {
        // Удаляем старые пути
        ClearWatchPaths();
        // Сохраняем новые
        for (size_t i = 0; i < paths.size(); ++i) {
            s_data[L"watchPath_" + std::to_wstring(i)] = paths[i];
        }
    }

    static std::vector<std::wstring> GetWatchPaths() {
        std::vector<std::wstring> result;
        for (size_t i = 0; ; ++i) {
            std::wstring key = L"watchPath_" + std::to_wstring(i);
            if (!Has(key)) break;
            std::wstring path = Get(key);
            if (!path.empty()) {
                result.push_back(path);
            }
        }
        return result;
    }

    static void AddWatchPath(const std::wstring& path) {
        auto paths = GetWatchPaths();
        paths.push_back(path);
        SetWatchPaths(paths);
    }

    static void RemoveWatchPath(size_t index) {
        auto paths = GetWatchPaths();
        if (index < paths.size()) {
            paths.erase(paths.begin() + index);
            SetWatchPaths(paths);
        }
    }

    static void ClearWatchPaths() {
        // Удаляем все ключи вида watchPath_N
        auto it = s_data.begin();
        while (it != s_data.end()) {
            if (it->first.rfind(L"watchPath_", 0) == 0) {
                it = s_data.erase(it);
            } else {
                ++it;
            }
        }
    }

    
    static int GetMaxVersions() {
    std::wstring val = Get(L"maxVersions", L"5");
    if (val.empty()) return 5;
    return _wtoi(val.c_str());
}

    static void SetMaxVersions(int n) 
    {
        Set(L"maxVersions", std::to_wstring(n));
    }

    static std::vector<std::wstring> GetVersionedExtensions() 
    {
        std::vector<std::wstring> exts;
        std::wstring val = Get(L"versionedExtensions", 
            L".docx,.xlsx,.txt,.pdf,.cpp,.h,.hpp,.c,.cc,.cs,.java,.py,.js,.xml,.json,.md");
        std::wstringstream ss(val);
        std::wstring item;
        while (std::getline(ss, item, L',')) {
            if (!item.empty()) exts.push_back(item);
        }
        return exts;
    }

    static bool IsVersionedExtension(const std::wstring& filename) 
    {
        size_t dot = filename.find_last_of(L'.');
        if (dot == std::wstring::npos) return false;
        std::wstring ext = filename.substr(dot);
        for (auto& c : ext) c = towlower(c);
        auto exts = GetVersionedExtensions();
        for (const auto& e : exts) {
            std::wstring lowerExt = e;
            for (auto& c : lowerExt) c = towlower(c);
            if (ext == lowerExt) return true;
        }
        return false;
    }

    //Игнорирование расширений
    static void SetIgnoredExtensions(const std::vector<std::wstring>& exts) 
    {
        // очистить старые
        auto it = s_data.begin();
        while (it != s_data.end()) {
            if (it->first.rfind(L"ignoreExt_", 0) == 0)
                it = s_data.erase(it);
            else ++it;
        }
        for (size_t i = 0; i < exts.size(); ++i)
            s_data[L"ignoreExt_" + std::to_wstring(i)] = exts[i];
    }

    static std::vector<std::wstring> GetIgnoredExtensions() 
    {
        std::vector<std::wstring> result;
        for (size_t i = 0; ; ++i) {
            std::wstring key = L"ignoreExt_" + std::to_wstring(i);
            if (!Has(key)) break;
            std::wstring ext = Get(key);
            if (!ext.empty()) result.push_back(ext);
        }
        return result;
    }

    static bool IsExtensionIgnored(const std::wstring& filename) 
    {
        size_t dot = filename.find_last_of(L'.');
        if (dot == std::wstring::npos) return false;
        std::wstring ext = filename.substr(dot);
        for (auto& c : ext) c = towlower(c);
        auto ignored = GetIgnoredExtensions();
        for (const auto& ign : ignored) {
            std::wstring lowerIgn = ign;
            for (auto& c : lowerIgn) c = towlower(c);
            if (ext == lowerIgn) return true;
        }
        return false;
    }

private:
    static inline std::wstring s_path;
    static inline std::unordered_map<std::wstring, std::wstring> s_data;
};
