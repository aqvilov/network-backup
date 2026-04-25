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

    struct CopyResultWithCRC {
        bool        success = false;
        bool        integrityOk = true;
        std::wstring error;
        uint64_t    bytesCopied = 0;
        uint32_t    sourceCRC = 0;
        uint32_t    destCRC = 0;
    };

    inline std::wstring GetRelativePath(const std::wstring& fullPath, const std::wstring& rootPath) {
        fs::path full(fullPath);
        fs::path root(rootPath);
        auto rel = fs::relative(full, root);
        return rel.wstring();
    }

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

    inline bool VerifyFileIntegrity(const std::wstring& originalPath, const std::wstring& copyPath) {
        uint32_t crcOriginal = ComputeCRC32(originalPath);
        uint32_t crcCopy = ComputeCRC32(copyPath);

        if (crcOriginal == 0 && crcCopy == 0) {
            return false;
        }

        return crcOriginal == crcCopy;
    }

    inline std::wstring Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), result.data(), len);
        return result;
    }

    inline CopyResultWithCRC CopyToBackupWithVerify(const std::wstring& src,
        const std::wstring& watchRoot,
        const std::wstring& destDir)
    {
        CopyResultWithCRC result;

        static int counter = 0;
        counter++;
        if (counter == 3) {
            result.success = false;
            result.error = L"CRC32 не совпадает (ТЕСТ)";
            return result;
        }

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

            result.sourceCRC = ComputeCRC32(src);
            if (result.sourceCRC == 0) {
                if (fs::file_size(srcPath) == 0) {
                    result.sourceCRC = 0xFFFFFFFF;
                }
                else {
                    result.error = L"Не удалось вычислить CRC32 исходного файла: " + src;
                    return result;
                }
            }

            std::wstring relative;
            try {
                relative = GetRelativePath(src, watchRoot);
            }
            catch (...) {
                relative = filename;
            }

            fs::path target = destPath / relative;
            fs::create_directories(target.parent_path());
            fs::copy_file(srcPath, target, fs::copy_options::overwrite_existing);

            result.bytesCopied = fs::file_size(target);

            result.destCRC = ComputeCRC32(target.wstring());
            if (result.destCRC == 0 && fs::file_size(target) > 0) {
                result.error = L"Не удалось вычислить CRC32 копии: " + target.wstring();
                return result;
            }

            result.integrityOk = (result.sourceCRC == result.destCRC);

            if (result.integrityOk) {
                result.success = true;
            }
            else {
                result.success = false;
                result.error = L"CRC32 не совпадает! Оригинал: " +
                    std::to_wstring(result.sourceCRC) +
                    L", Копия: " + std::to_wstring(result.destCRC);
            }
        }
        catch (const std::exception& e) {
            std::wstring err = Utf8ToWide(e.what());
            result.error = err;
        }
        catch (...) {
            result.error = L"Неизвестная ошибка при копировании";
        }
        return result;
    }

    inline std::wstring FormatSize(uint64_t bytes) {
        std::wostringstream ss;
        if (bytes < 1024)       ss << bytes << L" B";
        else if (bytes < 1048576)    ss << std::fixed << std::setprecision(1) << bytes / 1024.0 << L" KB";
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

    static std::wstring GetFileName(const std::wstring& path) {
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }
}
