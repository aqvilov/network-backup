#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <vector>

struct UploadResult 
{
    bool success;
    std::wstring fileId;      // ID файла на Google Drive
    std::wstring errorMsg;
    uint64_t bytesUploaded;
};

using UploadCallback = std::function<void(const std::wstring& localPath, const UploadResult& result)>;

class GoogleDriveUploader 
{
public:
    // Инициализация с access_token (обновлять перед каждой загрузкой, если истёк)
    static void SetAccessToken(const std::wstring& token);
    
    // Загрузить файл в корень Google Drive (или в указанную папку)
    static void UploadFile(const std::wstring& localFilePath, 
                           const std::wstring& parentFolderId, // L"" для корня
                           UploadCallback callback);
    
    // Проверка, авторизованы ли мы (есть ли refresh_token и он рабочий)
    static bool IsAuthorized();
    
    // Обновить access_token из сохранённого refresh_token (вернёт true если успешно)
    static bool RefreshTokenIfNeeded();
    
private:
    static std::wstring s_accessToken;
    static std::mutex s_mutex;
    static std::wstring GetMimeType(const std::wstring& filePath);
    static std::string ReadFileBinary(const std::wstring& path, std::vector<char>& buffer);
};