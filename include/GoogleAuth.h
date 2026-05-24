#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <memory>

// Результат авторизации
struct GoogleTokens 
{
    std::wstring access_token;
    std::wstring refresh_token;
    int expires_in; // секунды до истечения access_token
};

// Callback после завершения авторизации
using AuthCallback = std::function<void(bool success, const GoogleTokens& tokens)>;

class GoogleAuth 
{
public:
    static bool Initialize(const std::wstring& clientId, const std::wstring& clientSecret);
    
    // Начать процесс авторизации (открыть браузер, запустить локальный сервер)
    static void Authorize(AuthCallback callback);
    
    // Обновить access_token по refresh_token (синхронно, но вызывать в отдельном потоке)
    static bool RefreshAccessToken(const std::wstring& refreshToken, GoogleTokens& outTokens);
    
    // Получить сохранённый refresh_token из конфига (чтобы не входить каждый раз)
    static std::wstring GetStoredRefreshToken();
    static void StoreRefreshToken(const std::wstring& token);
    
private:
    static std::wstring s_clientId;
    static std::wstring s_clientSecret;
    static void StartLocalServer(int port, AuthCallback callback);
    static std::string UrlEncode(const std::string& value);
    static std::wstring Utf8ToWide(const std::string& utf8);
    static std::string WideToUtf8(const std::wstring& wide);
};