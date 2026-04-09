//АРС, ЭТО ТВОЙ ФАЙЛ, У МЕНЯ С НИМ НИЧЕГО НЕ РАБОТАЛО, Я НЕМНОГО ИЗМЕНИЛ ЕГО. Я ОСТАВЛЯЮ ЭТОТ ФАЙЛ НА СЛУЧАЙ, ЕСЛИ НУЖНО БУДЕТ ВЕРНУТЬСЯ К НЕМУ.



#pragma once
// FileUtils.h — утилиты для работы с файлами
// Копирование с сохранением структуры папок, вычисление хэша

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <shlobj.h>     // для SHGetFolderPathW
#include <objbase.h>    // для CoTaskMemFree (если используется)

namespace fs = std::filesystem;

namespace FileUtils {

    // Результат операции копирования
    struct CopyResult {
        bool        success  = false;
        std::wstring error;
        uint64_t    bytesCopied = 0;
    };

    // Скопировать файл из src в destDir, сохраняя структуру относительно watchRoot
    // Например: watchRoot=C:\docs, src=C:\docs\work\file.txt, destDir=D:\backup
    //           → результат: D:\backup\work\file.txt
    inline CopyResult CopyToBackup(const std::wstring& src,
                                   const std::wstring& watchRoot,
                                   const std::wstring& destDir)
    {
        CopyResult result;
        try {
            fs::path srcPath(src);
            fs::path rootPath(watchRoot);
            fs::path destPath(destDir);

            // Проверяем что файл существует
            if (!fs::exists(srcPath)) {
                result.error = L"Файл не найден: " + src;
                return result;
            }

            // Пропускаем временные файлы (Word, Excel создают ~$file.docx)
            std::wstring filename = srcPath.filename().wstring();
            if (!filename.empty() && filename[0] == L'~') {
                result.error = L"Пропущен временный файл";
                return result;
            }
            // Пропускаем системные файлы
            if (filename == L"desktop.ini" || filename == L"thumbs.db") {
                result.error = L"Пропущен системный файл";
                return result;
            }

            fs::path relative = fs::relative(srcPath, rootPath);
            fs::path target   = destPath / relative;

            // Проверяем создание папки.
            if (!fs::create_directories(target.parent_path())) 
            {
                result.error = L"Не удалось создать папку: " + target.parent_path().wstring();
                return result;
            }

            fs::copy_file(srcPath, target, fs::copy_options::overwrite_existing);

            result.success    = true;
            result.bytesCopied = fs::file_size(target);
        }
        catch (const std::exception& e) {
            int len = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
            std::wstring wide(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, wide.data(), len);
            result.error = wide;
        }
        return result;
    }

    // Простой CRC32 для обнаружения изменений (быстрее SHA-256 для MVP)
    inline uint32_t ComputeCRC32(const std::wstring& filePath) {
        std::ifstream f(filePath, std::ios::binary);
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
