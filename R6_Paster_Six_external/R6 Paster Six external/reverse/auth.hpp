#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "json.hpp"

#ifdef AUTHFUSION_USE_WINHTTP
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#include "curl/curl.h"
#endif

class AuthFusion {
private:
    std::string ownerid = "3bd7dc39-eb3e-4463-91c7-2b7ae45fd022";
    std::string secret = "3c99f04ac7fb08579523ff2c584283f4";
    std::string base_url = "https://nxrauth-3x4c.onrender.com";

#ifndef AUTHFUSION_USE_WINHTTP
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

        std::wstring wurl = widen(base_url + endpoint);
        URL_COMPONENTS parts = { 0 };
        wchar_t host[256] = { 0 };
        wchar_t path[1024] = { 0 };
        parts.dwStructSize = sizeof(parts);
        parts.lpszHostName = host;
        parts.dwHostNameLength = _countof(host);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = _countof(path);
        if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &parts))
            return response;

        HINTERNET session = WinHttpOpen(L"AuthFusion/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return response;

        HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
        if (connection) {
            DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET request = WinHttpOpenRequest(connection, L"POST", path, NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (request) {
                const wchar_t* headers = L"Content-Type: application/json\r\n";
                if (WinHttpSendRequest(request, headers, (DWORD)-1,
                    (LPVOID)json_data.c_str(), (DWORD)json_data.size(),
                    (DWORD)json_data.size(), 0) && WinHttpReceiveResponse(request, NULL)) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                        std::vector<char> buffer(available + 1, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(request, buffer.data(), available, &read) || read == 0)
                            break;
                        response.append(buffer.data(), read);
                    }
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connection);
        }
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
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                    out += oss.str();
                }
                else out += c;
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

    AuthFusion() = default;
    AuthFusion(std::string oid, std::string sec) : ownerid(oid), secret(sec) {}

    static std::string get_hwid() {
        DWORD serial = 0;
        if (!GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0))
            serial = 0;
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << serial;
        return oss.str();
    }

    std::string user_login(std::string username, std::string password, std::string hwid) {
        std::string payload = "{\"ownerid\":\"" + escape(ownerid) + "\",\"app_secret\":\"" + escape(secret) +
                              "\",\"username\":\"" + escape(username) + "\",\"password\":\"" + escape(password) +
                              "\",\"hwid\":\"" + escape(hwid) + "\"}";
        return post_request("/api/1.0/user_login", payload);
    }

    result login(const std::string& username, const std::string& password, const std::string& hwid = get_hwid()) {
        result res;
        res.raw = user_login(username, password, hwid);
        if (res.raw.empty()) {
            res.message = "no response from auth server";
            return res;
        }
        try {
            nlohmann::json data = nlohmann::json::parse(res.raw);
            if (data.contains("success")) res.success = data["success"].get<bool>();
            if (data.contains("message") && data["message"].is_string()) res.message = data["message"].get<std::string>();
            if (data.contains("username") && data["username"].is_string()) res.username = data["username"].get<std::string>();
            if (data.contains("expiry") && data["expiry"].is_string()) res.expiry = data["expiry"].get<std::string>();
        }
        catch (const std::exception&) {
            res.success = false;
            res.message = "invalid response from auth server";
        }
        return res;
    }
};
