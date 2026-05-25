#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <shlobj.h>
#include <chrono>
#include <algorithm>
#include "Config.h"

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

    // Проверка, является ли один путь подпапкой другого
    inline bool IsSubdirectory(const std::wstring& potentialParent, 
                               const std::wstring& potentialChild) 
    {
        try {
            fs::path parent = fs::canonical(potentialParent);
            fs::path child = fs::canonical(potentialChild);
            
            // Проверяем, является ли child подпапкой parent
            auto mismatch_pair = std::mismatch(
                parent.begin(), parent.end(),
                child.begin(), child.end()
            );
            
            return mismatch_pair.first == parent.end();
        } catch (...) {
            return false;
        }
    }
        // ========== Версионирование ==========
    inline fs::path GetVersionsRoot(const fs::path& destDir) 
    {
        return destDir / L".versions";
    }

    inline fs::path GetVersionDir(const fs::path& targetFile, const fs::path& destRoot) 
    {
        fs::path rel = fs::relative(targetFile, destRoot);
        fs::path versionsRoot = GetVersionsRoot(destRoot);
        return versionsRoot / rel.parent_path() / rel.filename();
    }

    inline bool SaveVersion(const fs::path& targetFile, const fs::path& destRoot) 
    {
        if (!fs::exists(targetFile)) return false;
        if (!Config::IsVersionedExtension(targetFile.filename().wstring())) return false;

        fs::path versionDir = GetVersionDir(targetFile, destRoot);
        fs::create_directories(versionDir);

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &time);
        wchar_t timeBuf[64];
        wcsftime(timeBuf, 64, L"_%Y-%m-%d_%H-%M-%S", &tm);

        std::wstring versionName = targetFile.stem().wstring() + timeBuf + targetFile.extension().wstring();
        fs::path versionPath = versionDir / versionName;

        try {
            fs::rename(targetFile, versionPath);
            return true;
        } catch (...) {
            return false;
        }
    }

    inline void RotateVersions(const fs::path& targetFile, const fs::path& destRoot, int maxVersions) 
    {
        fs::path versionDir = GetVersionDir(targetFile, destRoot);
        if (!fs::exists(versionDir)) return;

        std::vector<fs::path> versions;
        for (const auto& entry : fs::directory_iterator(versionDir)) 
        {
            if (entry.is_regular_file())
                versions.push_back(entry.path());
        }
        if (versions.size() <= (size_t)maxVersions) return;

        std::sort(versions.begin(), versions.end(),
            [](const fs::path& a, const fs::path& b) 
            {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

        size_t toDelete = versions.size() - maxVersions;
        for (size_t i = 0; i < toDelete; ++i) 
        {
            try { fs::remove(versions[i]); } catch (...) {}
        }
    }

    inline CopyResult CopyToBackup(const std::wstring& src,
                                   const std::wstring& watchRoot,
                                   const std::wstring& destDir)
    {
        CopyResult result;
        try {
            fs::path srcPath(src);
            fs::path rootPath(watchRoot);
            fs::path destRoot(destDir);
            std::wstring relative = GetRelativePath(src, watchRoot);
            fs::path destPath = destRoot / relative;

            if (!fs::exists(srcPath)) 
            {
                result.error = L"Файл не найден: " + src;
                return result;
            }

            std::wstring filename = srcPath.filename().wstring();
            if (!filename.empty() && filename[0] == L'~') 
            {
                result.error = L"Пропущен временный файл";
                return result;
            }
            if (filename == L"desktop.ini" || filename == L"thumbs.db") 
            {
                result.error = L"Пропущен системный файл";
                return result;
            }
            if (Config::IsExtensionIgnored(filename)) 
            {
                result.error = L"Пропущен по фильтру расширений";
                return result;
            }

            // ВЕРСИОНИРОВАНИЕ: сохраняем старую версию, если файл уже существует и подлежит версионированию
            if (fs::exists(destPath) && Config::IsVersionedExtension(filename)) 
            {
                SaveVersion(destPath, destRoot);
            }

            fs::create_directories(destPath.parent_path());
            fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing);

            // После копирования – ротация версий (если файл версионируемый)
            if (Config::IsVersionedExtension(filename)) 
            {
                int maxVersions = Config::GetMaxVersions();
                RotateVersions(destPath, destRoot, maxVersions);
            }

            result.success = true;
            result.bytesCopied = fs::file_size(destPath);
        }
        catch (const std::exception& e) 
        {
            int len = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
            std::wstring wide(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, wide.data(), len);
            result.error = wide;
        }
        catch (...) 
        {
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

    inline bool IsRootOrProtectedPath(const std::wstring& path) {
        if (path.empty()) return true;
        
        try {
            fs::path p = fs::absolute(path);
            std::wstring normalized = p.wstring();
            
            // Проверка на корень диска (C:\, D:\ и т.д.)
            if (normalized.length() <= 3) {
                return true;
            }
            
            // Проверка на точный корень диска
            if (normalized.length() == 3 && 
                normalized[1] == L':' && 
                normalized[2] == L'\\') {
                return true;
            }
            
            // Получаем специальные папки Windows
            wchar_t desktopPath[MAX_PATH];
            wchar_t documentsPath[MAX_PATH];
            wchar_t profilePath[MAX_PATH];
            
            std::vector<std::wstring> protectedPaths;
            
            // Рабочий стол
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath))) {
                protectedPaths.push_back(desktopPath);
            }
            
            // Мои документы
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, documentsPath))) {
                protectedPaths.push_back(documentsPath);
            }
            
            // Профиль пользователя
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, profilePath))) {
                protectedPaths.push_back(profilePath);
            }
            
            // Проверяем, не является ли выбранный путь одной из защищенных папок
            for (const auto& protectedPath : protectedPaths) {
                try {
                    fs::path protected_canonical = fs::canonical(protectedPath);
                    if (fs::equivalent(p, protected_canonical)) {
                        return true;
                    }
                } catch (...) {
                    // Игнорируем ошибки при проверке отдельных путей
                }
            }
            
            return false;
        } catch (...) {
            return true; // В случае ошибки считаем путь защищенным (безопаснее)
        }
    }
}