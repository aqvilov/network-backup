#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace FileUtils {

    struct CopyResult {
        bool        success = false;
        std::wstring error;
        uint64_t    bytesCopied = 0;
    };

    // Простая функция для получения относительного пути (работает даже на разных дисках)
    inline std::wstring GetRelativePath(const std::wstring& fullPath, const std::wstring& rootPath) {
        fs::path full(fullPath);
        fs::path root(rootPath);
        // Если пути одинаковые или root является родителем full
        auto rel = fs::relative(full, root);
        return rel.wstring();
    }

    inline CopyResult CopyToBackup(const std::wstring& src,
                                   const std::wstring& watchRoot,
                                   const std::wstring& destDir)
    {
        CopyResult result;
        try {
            fs::path srcPath(src);
            fs::path rootPath(watchRoot);
            fs::path destPath(destDir);

            if (!fs::exists(srcPath)) {
                result.error = L"Файл не найден: " + src;
                return result;
            }

            std::wstring filename = srcPath.filename().wstring();
            if (!filename.empty() && filename[0] == L'~') {
                result.error = L"Пропущен временный файл";
                return result;
            }
            if (filename == L"desktop.ini" || filename == L"thumbs.db") {
                result.error = L"Пропущен системный файл";
                return result;
            }

            // Безопасно получаем относительный путь
            std::wstring relative;
            try {
                relative = GetRelativePath(src, watchRoot);
            } catch (...) {
                // Если не удалось вычислить относительный путь, просто берём имя файла
                relative = filename;
            }

            fs::path target = destPath / relative;

            // Создаём целевую папку (если нужно)
            fs::create_directories(target.parent_path());

            // Копируем с перезаписью
            fs::copy_file(srcPath, target, fs::copy_options::overwrite_existing);

            result.success = true;
            result.bytesCopied = fs::file_size(target);
        }
        catch (const std::exception& e) {
            int len = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
            std::wstring wide(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, wide.data(), len);
            result.error = wide;
        }
        catch (...) {
            result.error = L"Неизвестная ошибка при копировании";
        }
        return result;
    }

    inline uint32_t ComputeCRC32(const std::wstring& filePath) {
        std::ifstream f(fs::path(filePath), std::ios::binary);
        if (!f.is_open()) return 0;
        uint32_t crc = 0xFFFFFFFF;
        char buf[4096];
        while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
            for (std::streamsize i = 0; i < f.gcount(); i++) {
                crc ^= (uint8_t)buf[i];
                for (int j = 0; j < 8; j++)
                    crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        return crc ^ 0xFFFFFFFF;
    }

    inline std::wstring FormatSize(uint64_t bytes) {
        std::wostringstream ss;
        if      (bytes < 1024)       ss << bytes << L" B";
        else if (bytes < 1048576)    ss << std::fixed << std::setprecision(1) << bytes / 1024.0    << L" KB";
        else if (bytes < 1073741824) ss << std::fixed << std::setprecision(1) << bytes / 1048576.0 << L" MB";
        else                         ss << std::fixed << std::setprecision(2) << bytes / 1073741824.0 << L" GB";
        return ss.str();
    }

    inline std::wstring GetAppDataDir() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
            return std::wstring(path) + L"\\NetBackup";
        return L".";
    }

    inline std::wstring Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), result.data(), len);
        return result;
    }
}