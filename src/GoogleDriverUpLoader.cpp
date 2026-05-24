#include <filesystem>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

namespace fs = std::filesystem;

#include "../include/GoogleDriveUploader.h"
#include "../include/Logger.h"
#include "../include/GoogleAuth.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ========== Вспомогательные функции (кодировка) ==========
static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), result.data(), len, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), result.data(), len);
    return result;
}

// ========== Статические члены класса ==========
std::wstring GoogleDriveUploader::s_accessToken;
std::mutex GoogleDriveUploader::s_mutex;

// ========== Реализация методов ==========
void GoogleDriveUploader::SetAccessToken(const std::wstring& token) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_accessToken = token;
}

bool GoogleDriveUploader::IsAuthorized() {
    return !GoogleAuth::GetStoredRefreshToken().empty();
}

bool GoogleDriveUploader::RefreshTokenIfNeeded() {
    std::wstring refresh = GoogleAuth::GetStoredRefreshToken();
    if (refresh.empty()) return false;
    GoogleTokens newTokens;
    if (GoogleAuth::RefreshAccessToken(refresh, newTokens)) {
        SetAccessToken(newTokens.access_token);
        return true;
    }
    return false;
}

std::wstring GoogleDriveUploader::GetMimeType(const std::wstring& filePath) {
    std::wstring ext = fs::path(filePath).extension().wstring();
    if (ext == L".txt") return L"text/plain";
    if (ext == L".jpg" || ext == L".jpeg") return L"image/jpeg";
    if (ext == L".png") return L"image/png";
    if (ext == L".pdf") return L"application/pdf";
    if (ext == L".docx") return L"application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == L".xlsx") return L"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    return L"application/octet-stream";
}

std::string GoogleDriveUploader::ReadFileBinary(const std::wstring& path, std::vector<char>& buffer) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(size);
    file.read(buffer.data(), size);
    return std::string(buffer.data(), size);
}

void GoogleDriveUploader::UploadFile(const std::wstring& localFilePath,
                                     const std::wstring& parentFolderId,
                                     UploadCallback callback) {
    std::thread([localFilePath, parentFolderId, callback]() {
        std::wstring token;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            token = s_accessToken;
        }
        if (token.empty()) {
            if (!RefreshTokenIfNeeded()) {
                UploadResult res{false, L"", L"No access token", 0};
                callback(localFilePath, res);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                token = s_accessToken;
            }
        }

        std::vector<char> fileData;
        std::string fileDataStr = GoogleDriveUploader::ReadFileBinary(localFilePath, fileData);
        if (fileDataStr.empty()) {
            UploadResult res{false, L"", L"Cannot read file", 0};
            callback(localFilePath, res);
            return;
        }

        std::wstring filename = fs::path(localFilePath).filename().wstring();
        std::string mimeType = WideToUtf8(GoogleDriveUploader::GetMimeType(localFilePath));

        json metadata;
        metadata["name"] = WideToUtf8(filename);
        if (!parentFolderId.empty()) {
            metadata["parents"] = json::array({WideToUtf8(parentFolderId)});
        }
        std::string metadataStr = metadata.dump();

        std::string boundary = "-------" + std::to_string(GetCurrentThreadId()) + "boundary";
        std::string body;
        body += "--" + boundary + "\r\n";
        body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
        body += metadataStr + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Type: " + mimeType + "\r\n\r\n";
        body += fileDataStr + "\r\n";
        body += "--" + boundary + "--\r\n";

        httplib::Client cli("https://www.googleapis.com");
        httplib::Headers headers = {
            {"Authorization", "Bearer " + WideToUtf8(token)},
            {"Content-Type", "multipart/related; boundary=" + boundary}
        };

        auto result = cli.Post("/upload/drive/v3/files?uploadType=multipart", headers, body);

        UploadResult res;
        if (result && result->status == 200) {
            try {
                json resp = json::parse(result->body);
                res.success = true;
                res.fileId = Utf8ToWide(resp["id"].get<std::string>());
                res.bytesUploaded = fileData.size();
            } catch (...) {
                res.success = false;
                res.errorMsg = L"Invalid JSON response";
            }
        } else {
            res.success = false;
            if (result) {
                res.errorMsg = L"HTTP " + std::to_wstring(result->status);
            } else {
                res.errorMsg = L"Connection error";
            }
        }
        callback(localFilePath, res);
    }).detach();
}