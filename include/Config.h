#pragma once
// Config.h — сохранение и загрузка настроек в простой текстовый файл
// Формат: ключ=значение, по одному на строку

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cctype>      // для towlower
#include <algorithm>

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
        //После загрузки словаря, обновляем список игнорируемых расширений
        LoadIgnoredFromConfig();
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

    static void Set(const std::wstring& key, const std::wstring& val) { s_data[key] = val; }
    static std::wstring Get(const std::wstring& key, const std::wstring& def = L"") {
        auto it = s_data.find(key);
        return it != s_data.end() ? it->second : def;
    }
    static bool Has(const std::wstring& key) { return s_data.count(key) > 0; }

    static void SetIgnoredExtensions(const std::wstring& extList) 
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        Set(L"ignoredExtensions", extList);
        Save();                     // сразу сохраняем в файл
        ParseExtList(extList, s_ignoredExts);
    }

    static bool IsExtensionIgnored(const std::wstring& filename) 
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_ignoredExts.empty()) return false;
        size_t dot = filename.find_last_of(L'.');
        if (dot == std::wstring::npos) return false;
        std::wstring ext = filename.substr(dot + 1);
        for (auto& c : ext) c = towlower(c);
        return s_ignoredExts.count(ext) > 0;
    }

private:
    static void LoadIgnoredFromConfig() 
    {
        std::wstring list = Get(L"ignoredExtensions", L"");
        ParseExtList(list, s_ignoredExts);
    }

    static void ParseExtList(const std::wstring& list,
                             std::unordered_set<std::wstring>& outSet) {
        outSet.clear();
        if (list.empty()) return;
        // Разделители: пробел, запятая, точка с запятой
        std::wistringstream iss(list);
        std::wstring token;
        while (iss >> token) {
            // Дополнительное разбиение по запятым и точкам с запятой внутри токена
            size_t start = 0, end;
            while ((end = token.find_first_of(L",;", start)) != std::wstring::npos) {
                std::wstring ext = token.substr(start, end - start);
                NormalizeExtension(ext);
                if (!ext.empty()) outSet.insert(ext);
                start = end + 1;
            }
            std::wstring ext = token.substr(start);
            NormalizeExtension(ext);
            if (!ext.empty()) outSet.insert(ext);
        }
    }

    static void NormalizeExtension(std::wstring& ext) {
        if (ext.empty()) return;
        // Удаляем начальную точку и звёздочку
        if (ext[0] == L'.') ext.erase(0, 1);
        if (!ext.empty() && ext[0] == L'*') ext.erase(0, 1);
        // Приводим к нижнему регистру
        for (auto& c : ext) c = towlower(c);
    }

    static inline std::mutex s_mutex;
    static inline std::wstring s_path;
    static inline std::unordered_map<std::wstring, std::wstring> s_data;
    static inline std::unordered_set<std::wstring> s_ignoredExts;
};
