#pragma once
#define WIN32_LEAN_AND_MEAN
#include <mutex>
#include <queue>
#include <condition_variable>
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
    static void SetAccessToken(const std::wstring& token);
    static void UploadFile(const std::wstring& localFilePath, 
                           const std::wstring& parentFolderId,
                           UploadCallback callback);
    static bool IsAuthorized();
    static bool RefreshTokenIfNeeded();
    static void InitializeUploadQueue();
    static void ShutdownUploadQueue();
    
private:
    static std::wstring s_accessToken;
    static std::mutex s_mutex;
    static std::wstring GetMimeType(const std::wstring& filePath);
    static std::string ReadFileBinary(const std::wstring& path, std::vector<char>& buffer);
    static void UploadFileSync(const std::wstring& localFilePath,
                               const std::wstring& parentFolderId,
                               UploadCallback callback);
};