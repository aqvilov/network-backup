#pragma once
// Config.h — сохранение и загрузка настроек в простой текстовый файл
// Формат: ключ=значение, по одному на строку

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
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

private:
    static inline std::wstring s_path;
    static inline std::unordered_map<std::wstring, std::wstring> s_data;
};
