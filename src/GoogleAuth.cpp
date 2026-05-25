#include "../include/GoogleAuth.h"
#include "../include/Logger.h"
#include "../include/Config.h" // для хранения refresh_token
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <thread>
#include <shellapi.h> // ShellExecuteW

using json = nlohmann::json;

std::wstring GoogleAuth::s_clientId;
std::wstring GoogleAuth::s_clientSecret;

bool GoogleAuth::Initialize(const std::wstring& clientId, const std::wstring& clientSecret) 
{
    s_clientId = clientId;
    s_clientSecret = clientSecret;
    return !clientId.empty() && !clientSecret.empty();
}

void GoogleAuth::Authorize(AuthCallback callback) 
{
    // Запускаем локальный HTTP-сервер в отдельном потоке
    std::thread([callback]() {
        StartLocalServer(8080, callback);
    }).detach();
    
    // Формируем URL для OAuth
    std::string redirectUri = "http://localhost:8080/";
    std::string scope = "https://www.googleapis.com/auth/drive.file";
    std::string clientIdStr = WideToUtf8(s_clientId);
    
    std::string authUrl = "https://accounts.google.com/o/oauth2/v2/auth?"
        "response_type=code&"
        "client_id=" + clientIdStr + "&"
        "redirect_uri=" + UrlEncode(redirectUri) + "&"
        "scope=" + UrlEncode(scope) + "&"
        "access_type=offline&"
        "prompt=consent";
    
    // Открываем браузер
    std::wstring wideUrl = Utf8ToWide(authUrl);
    ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void GoogleAuth::StartLocalServer(int port, AuthCallback callback) 
{
    httplib::Server svr;
    
    svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) 
    {
        // Получаем параметр code из запроса
        if (req.has_param("code")) 
        {
            std::string authCode = req.get_param_value("code");
            // Обмениваем код на токены
            httplib::Client cli("https://oauth2.googleapis.com");
            httplib::Params params{
                {"code", authCode},
                {"client_id", WideToUtf8(s_clientId)},
                {"client_secret", WideToUtf8(s_clientSecret)},
                {"redirect_uri", "http://localhost:8080/"},
                {"grant_type", "authorization_code"}
            };
            
            std::string body;
            for (const auto& p : params) 
            {
                if (!body.empty()) body += "&";
                body += p.first + "=" + UrlEncode(p.second);
            }
            
            auto result = cli.Post("/token", body, "application/x-www-form-urlencoded");
            if (result && result->status == 200) 
            {
                try 
                {
                    json respJson = json::parse(result->body);
                    GoogleTokens tokens;
                    tokens.access_token = Utf8ToWide(respJson["access_token"].get<std::string>());
                    tokens.refresh_token = Utf8ToWide(respJson.value("refresh_token", ""));
                    tokens.expires_in = respJson["expires_in"].get<int>();
                    
                    // Сохраняем refresh_token
                    if (!tokens.refresh_token.empty()) 
                    {
                        StoreRefreshToken(tokens.refresh_token);
                    }
                    
                    res.set_content("Authorization successful! You can close this window.", "text/plain");
                    callback(true, tokens);
                } catch (...) 
                {
                    Logger::Error(L"Failed to parse token response");
                    callback(false, {});
                }
            } 
            else 
            {
                Logger::Error(L"Token exchange failed");
                callback(false, {});
            }
        } 
        else 
        {
            res.set_content("Authorization failed: no code parameter.", "text/plain");
            callback(false, {});
        }
        svr.stop();
    });
    
    svr.listen("localhost", port);
}

bool GoogleAuth::RefreshAccessToken(const std::wstring& refreshToken, GoogleTokens& outTokens) 
{
    httplib::Client cli("https://oauth2.googleapis.com");
    httplib::Params params{
        {"refresh_token", WideToUtf8(refreshToken)},
        {"client_id", WideToUtf8(s_clientId)},
        {"client_secret", WideToUtf8(s_clientSecret)},
        {"grant_type", "refresh_token"}
    };
    
    std::string body;
    for (const auto& p : params) 
    {
        if (!body.empty()) body += "&";
        body += p.first + "=" + UrlEncode(p.second);
    }
    
    auto result = cli.Post("/token", body, "application/x-www-form-urlencoded");
    if (result && result->status == 200) 
    {
        try 
        {
            json respJson = json::parse(result->body);
            outTokens.access_token = Utf8ToWide(respJson["access_token"].get<std::string>());
            outTokens.expires_in = respJson["expires_in"].get<int>();
            // refresh_token при обновлении не приходит (если только не выдан новый)
            outTokens.refresh_token = refreshToken;
            return true;
        } 
        catch (...) 
        {
            Logger::Error(L"Failed to parse refresh response");
        }
    }
    return false;
}

std::wstring GoogleAuth::GetStoredRefreshToken() 
{
    return Config::Get(L"google_refresh_token", L"");
}

void GoogleAuth::StoreRefreshToken(const std::wstring& token) 
{
    Config::Set(L"google_refresh_token", token);
    Config::Save();
}

// Вспомогательные функции
std::string GoogleAuth::UrlEncode(const std::string& value) 
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) 
    {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') 
        {
            escaped << c;
        } 
        else 
        {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

std::wstring GoogleAuth::Utf8ToWide(const std::string& utf8) 
{
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), result.data(), len);
    return result;
}

std::string GoogleAuth::WideToUtf8(const std::wstring& wide) 
{
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), result.data(), len, nullptr, nullptr);
    return result;
}