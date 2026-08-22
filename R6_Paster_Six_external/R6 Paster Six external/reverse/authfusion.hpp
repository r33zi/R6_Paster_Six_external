#pragma once

#include <Windows.h>
#include <conio.h>
#include <string>
#include <cstdio>
#include "json.hpp"

#ifdef AUTHFUSION_USE_CURL
#include <curl/curl.h>
#else
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// Fill these in with the values from the AuthFusion panel.
#ifndef AUTHFUSION_OWNERID
#define AUTHFUSION_OWNERID ""
#endif
#ifndef AUTHFUSION_SECRET
#define AUTHFUSION_SECRET ""
#endif
#ifndef AUTHFUSION_URL
#define AUTHFUSION_URL "https://nxrauth-3x4c.onrender.com"
#endif

class AuthFusion {
private:
    std::string ownerid;
    std::string secret;
    std::string base_url;

#ifdef AUTHFUSION_USE_CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string post_request(const std::string& endpoint, const std::string& json_data) {
        CURL* curl = curl_easy_init();
        std::string response;
        if (curl) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, (base_url + endpoint).c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        return response;
    }
#else
    static std::wstring widen(const std::string& s) {
        if (s.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
        return out;
    }

    std::string post_request(const std::string& endpoint, const std::string& json_data) {
        std::string response;

        URL_COMPONENTS parts = { 0 };
        parts.dwStructSize = sizeof(parts);
        wchar_t host[256] = { 0 };
        wchar_t path[1024] = { 0 };
        parts.lpszHostName = host;
        parts.dwHostNameLength = ARRAYSIZE(host);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = ARRAYSIZE(path);

        const std::wstring full = widen(base_url + endpoint);
        if (!WinHttpCrackUrl(full.c_str(), (DWORD)full.size(), 0, &parts)) return response;

        HINTERNET session = WinHttpOpen(L"AuthFusion/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return response;

        HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
        if (!connection) { WinHttpCloseHandle(session); return response; }

        const DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(connection, L"POST", path, NULL,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!request) { WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return response; }

        const wchar_t* content_type = L"Content-Type: application/json\r\n";
        BOOL ok = WinHttpSendRequest(request, content_type, (DWORD)-1L,
            (LPVOID)json_data.c_str(), (DWORD)json_data.size(), (DWORD)json_data.size(), 0);
        if (ok) ok = WinHttpReceiveResponse(request, NULL);

        if (ok) {
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                std::string chunk(available, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(request, &chunk[0], available, &read)) break;
                response.append(chunk.data(), read);
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }
#endif

    static std::string escape(const std::string& value) {
        std::string out;
        for (char c : value) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
            }
        }
        return out;
    }

public:
    struct result {
        bool success = false;
        std::string message;
        std::string username;
        std::string expiry;
        std::string raw;
    };

    AuthFusion(std::string oid, std::string sec, std::string url = AUTHFUSION_URL)
        : ownerid(oid), secret(sec), base_url(url) {}

    // Volume serial of the system drive, stable per machine and enough to bind a session.
    static std::string hwid() {
        DWORD serial = 0;
        if (!GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) return "unknown";
        char buffer[32] = { 0 };
        sprintf_s(buffer, "%08lX", serial);
        return buffer;
    }

    std::string user_login(std::string username, std::string password, std::string hwid) {
        std::string payload = "{\"ownerid\":\"" + escape(ownerid) + "\",\"app_secret\":\"" + escape(secret) +
                              "\",\"username\":\"" + escape(username) + "\",\"password\":\"" + escape(password) +
                              "\",\"hwid\":\"" + escape(hwid) + "\"}";
        return post_request("/api/1.0/user_login", payload);
    }

    result login(const std::string& username, const std::string& password) {
        result r;
        r.raw = user_login(username, password, hwid());
        if (r.raw.empty()) {
            r.message = "no response from auth server";
            return r;
        }

        nlohmann::json parsed = nlohmann::json::parse(r.raw, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            r.message = "malformed response from auth server";
            return r;
        }

        if (parsed.contains("success")) {
            const auto& success = parsed["success"];
            if (success.is_boolean()) r.success = success.get<bool>();
            else if (success.is_string()) r.success = (success.get<std::string>() == "true");
        }
        if (parsed.contains("message") && parsed["message"].is_string()) r.message = parsed["message"].get<std::string>();
        if (parsed.contains("username") && parsed["username"].is_string()) r.username = parsed["username"].get<std::string>();
        if (parsed.contains("expiry") && parsed["expiry"].is_string()) r.expiry = parsed["expiry"].get<std::string>();
        if (r.message.empty()) r.message = r.success ? "authenticated" : "login failed";
        return r;
    }
};

namespace authfusion {
    inline std::string read_line() {
        std::string line;
        int c;
        while ((c = _getch()) != '\r') {
            if (c == 3) exit(0);
            if (c == '\b') { if (!line.empty()) { line.pop_back(); printf("\b \b"); } continue; }
            if (c == 0 || c == 0xE0) { _getch(); continue; }
            line += (char)c;
            printf("%c", c);
        }
        printf("\n");
        return line;
    }

    inline std::string read_masked() {
        std::string line;
        int c;
        while ((c = _getch()) != '\r') {
            if (c == 3) exit(0);
            if (c == '\b') { if (!line.empty()) { line.pop_back(); printf("\b \b"); } continue; }
            if (c == 0 || c == 0xE0) { _getch(); continue; }
            line += (char)c;
            printf("*");
        }
        printf("\n");
        return line;
    }

    // Blocking console login gate. Returns once the server accepts the credentials.
    inline AuthFusion::result gate(int max_attempts = 3) {
        AuthFusion api(AUTHFUSION_OWNERID, AUTHFUSION_SECRET);
        AuthFusion::result r;
        for (int attempt = 1; attempt <= max_attempts; attempt++) {
            printf("[*] Username: ");
            const std::string username = read_line();
            printf("[*] Password: ");
            const std::string password = read_masked();
            printf("[.] Authenticating...\n");

            r = api.login(username, password);
            if (r.success) {
                printf("[+] Welcome, %s\n", (r.username.empty() ? username : r.username).c_str());
                if (!r.expiry.empty()) printf("[+] Subscription: %s\n", r.expiry.c_str());
                return r;
            }
            printf("[!] %s (attempt %d/%d)\n", r.message.c_str(), attempt, max_attempts);
        }
        return r;
    }
}
