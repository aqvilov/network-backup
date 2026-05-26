// GoogleDriveUploader.cpp
#include "../include/GoogleDriveUploader.h"
#include "../include/Logger.h"
#include "../include/GoogleAuth.h"
#include "../include/FileUtils.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <iomanip>
#include <ctime>
#include <filesystem>

using json = nlohmann::json;

extern void AddErrorRecord(const std::wstring& filePath, const std::wstring& errorMessage, 
                           bool isDriveError = false, const std::wstring& watchRoot = L"");

// Определение статических членов класса
std::wstring GoogleDriveUploader::s_accessToken;
std::mutex GoogleDriveUploader::s_mutex;

void GoogleDriveUploader::SetAccessToken(const std::wstring& token)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_accessToken = token;
}

bool GoogleDriveUploader::IsAuthorized()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_accessToken.empty())
        return true;
    // Попробуем получить refresh_token из конфига
    std::wstring refresh = GoogleAuth::GetStoredRefreshToken();
    return !refresh.empty();
}

bool GoogleDriveUploader::RefreshTokenIfNeeded()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_accessToken.empty())
        return true; // уже есть токен

    std::wstring refresh = GoogleAuth::GetStoredRefreshToken();
    if (refresh.empty())
    {
        Logger::Error(L"[GoogleDrive] Нет refresh_token, требуется повторная авторизация");
        return false;
    }

    GoogleTokens newTokens;
    if (!GoogleAuth::RefreshAccessToken(refresh, newTokens))
    {
        Logger::Error(L"[GoogleDrive] Не удалось обновить access_token");
        return false;
    }

    s_accessToken = newTokens.access_token;
    Logger::Info(L"[GoogleDrive] Access_token обновлён");
    return true;
}

std::wstring GoogleDriveUploader::GetMimeType(const std::wstring& filePath)
{
    // Простейшее определение по расширению (можно расширить)
    size_t dot = filePath.find_last_of(L'.');
    if (dot == std::wstring::npos)
        return L"application/octet-stream";

    std::wstring ext = filePath.substr(dot);
    for (auto& c : ext) c = towlower(c);

    if (ext == L".txt") return L"text/plain";
    if (ext == L".pdf") return L"application/pdf";
    if (ext == L".jpg" || ext == L".jpeg") return L"image/jpeg";
    if (ext == L".png") return L"image/png";
    if (ext == L".gif") return L"image/gif";
    if (ext == L".html" || ext == L".htm") return L"text/html";
    if (ext == L".css") return L"text/css";
    if (ext == L".js") return L"application/javascript";
    if (ext == L".json") return L"application/json";
    if (ext == L".xml") return L"application/xml";
    if (ext == L".zip") return L"application/zip";
    if (ext == L".docx") return L"application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == L".xlsx") return L"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (ext == L".pptx") return L"application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (ext == L".cpp" || ext == L".h" || ext == L".hpp") return L"text/x-c++src";
    // default
    return L"application/octet-stream";
}

std::string GoogleDriveUploader::ReadFileBinary(const std::wstring& path, std::vector<char>& buffer)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return "Failed to open file";

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(static_cast<size_t>(size));
    if (file.read(buffer.data(), size))
        return "";
    else
        return "Failed to read file";
}

void GoogleDriveUploader::InitializeUploadQueue()
{
    // Очередь инициализируется статически, здесь можно добавить
    // дополнительную логику инициализации при необходимости
    Logger::Info(L"[GoogleDrive] Очередь загрузки инициализирована");
}

void GoogleDriveUploader::ShutdownUploadQueue()
{
    Logger::Info(L"[GoogleDrive] Очередь загрузки остановлена");
}

void GoogleDriveUploader::UploadFile(const std::wstring& localFilePath,
                                     const std::wstring& parentFolderId,
                                     UploadCallback callback)
{
    // Асинхронная загрузка через отдельный поток
    std::thread([localFilePath, parentFolderId, callback]() {
        UploadFileSync(localFilePath, parentFolderId, callback);
    }).detach();
}

void GoogleDriveUploader::UploadFileSync(const std::wstring& localFilePath,
                                         const std::wstring& parentFolderId,
                                         UploadCallback callback)
{
    // 1. Убедимся, что есть access_token
    if (!RefreshTokenIfNeeded()) {
        UploadResult res;
        res.success = false;
        res.errorMsg = L"Нет действительного access_token. Выполните вход через Google.";
        AddErrorRecord(localFilePath, res.errorMsg, true);
        callback(localFilePath, res);
        return;
    }
    
    // 2. Проверка существования файла
    if (!std::filesystem::exists(localFilePath)) {
        UploadResult res;
        res.success = false;
        res.errorMsg = L"Файл не найден: " + localFilePath;
        AddErrorRecord(localFilePath, res.errorMsg, true);
        callback(localFilePath, res);
        return;
    }

    // 3. Чтение файла
    std::vector<char> fileData;
    std::string readError = ReadFileBinary(localFilePath, fileData);
    if (!readError.empty()) {
        UploadResult res;
        res.success = false;
        res.errorMsg = FileUtils::Utf8ToWide(readError);
        AddErrorRecord(localFilePath, res.errorMsg, true);
        callback(localFilePath, res);
        return;
    }

    // Получаем access_token (уже обновлён в RefreshTokenIfNeeded)
    std::wstring accessToken;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        accessToken = s_accessToken;
    }

    // 4. Формирование метаданных
    std::wstring fileName = std::filesystem::path(localFilePath).filename().wstring();
    std::string utf8FileName = FileUtils::WideToUtf8(fileName);
    std::string utf8ParentId = FileUtils::WideToUtf8(parentFolderId);

    nlohmann::json metadata;
    metadata["name"] = utf8FileName;
    if (!utf8ParentId.empty())
        metadata["parents"] = { utf8ParentId };

    std::string metadataStr = metadata.dump();

    // 5. Multipart body
    std::string boundary = "-------" + std::to_string(std::time(nullptr)) + "-------";
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    body += metadataStr + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Type: " + FileUtils::WideToUtf8(GetMimeType(localFilePath)) + "\r\n\r\n";
    body.append(fileData.data(), fileData.size());
    body += "\r\n--" + boundary + "--\r\n";

    // 6. HTTP-запрос
    httplib::Client cli("https://www.googleapis.com");
    cli.set_read_timeout(30, 0);
    cli.set_write_timeout(30, 0);

    std::string url = "/upload/drive/v3/files?uploadType=multipart";
    httplib::Headers headers = {
        {"Authorization", "Bearer " + FileUtils::WideToUtf8(accessToken)},
        {"Content-Type", "multipart/related; boundary=" + boundary}
    };

    auto response = cli.Post(url, headers, body, "multipart/related; boundary=" + boundary);

    UploadResult result;
    result.success = false;
    result.bytesUploaded = fileData.size();

    if (!response) {
        result.errorMsg = L"Сетевая ошибка: " + FileUtils::Utf8ToWide(httplib::to_string(response.error()));
        Logger::Error(L"[GoogleDrive] " + result.errorMsg);
        AddErrorRecord(localFilePath, result.errorMsg, true);
        callback(localFilePath, result);
        return;
    }

    if (response->status != 200) {
        std::wstring err = L"HTTP " + std::to_wstring(response->status) + L": " + FileUtils::Utf8ToWide(response->body);
        result.errorMsg = err;
        Logger::Error(L"[GoogleDrive] Ошибка загрузки " + localFilePath + L" — " + err);
        AddErrorRecord(localFilePath, result.errorMsg, true);
        callback(localFilePath, result);
        return;
    }

    try {
        nlohmann::json respJson = nlohmann::json::parse(response->body);
        if (respJson.contains("id")) {
            result.success = true;
            result.fileId = FileUtils::Utf8ToWide(respJson["id"].get<std::string>());
            Logger::Info(L"[GoogleDrive] Загружен " + localFilePath + L", fileId=" + result.fileId);
        } else {
            result.errorMsg = L"Неожиданный ответ: " + FileUtils::Utf8ToWide(response->body);
            Logger::Error(L"[GoogleDrive] " + result.errorMsg);
            AddErrorRecord(localFilePath, result.errorMsg, true);
        }
    } catch (const std::exception& e) {
        result.errorMsg = L"Ошибка парсинга JSON: " + FileUtils::Utf8ToWide(e.what());
        Logger::Error(L"[GoogleDrive] " + result.errorMsg);
        AddErrorRecord(localFilePath, result.errorMsg, true);
    }

    callback(localFilePath, result);
}