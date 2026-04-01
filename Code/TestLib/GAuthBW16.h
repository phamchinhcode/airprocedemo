#ifndef GAUTH_BW16_H
#define GAUTH_BW16_H
#define TOKEN_TIMEOUT 3600 // 1 giờ

#include <Arduino.h>
#include <WiFiSSLClient.h>

class GAuthBW16
{
private:
    WiFiSSLClient *_client;
    String _apiKey;
    String _email;
    String _password;
    String _idToken;
    String _refreshToken;
    String _localId;
    uint32_t _tokenStartTime = 0;

    // Hàm nội bộ để trích xuất giá trị từ chuỗi JSON bằng substring
    String extractValue(String json, String key)
    {
        String searchKey = "\"" + key + "\": \"";
        int startPos = json.indexOf(searchKey);
        if (startPos == -1)
            return "";
        startPos += searchKey.length();
        int endPos = json.indexOf("\"", startPos);
        if (endPos == -1)
            return "";
        return json.substring(startPos, endPos);
    }

public:
    // Khởi tạo 1: Anonymous Auth
    GAuthBW16(WiFiSSLClient &client, String apiKey)
    {
        _client = &client;
        _apiKey = apiKey;
        _email = "";
        _password = "";
    }

    // Khởi tạo 2: Email/Password Auth
    GAuthBW16(WiFiSSLClient &client, String apiKey, String email, String password)
    {
        _client = &client;
        _apiKey = apiKey;
        _email = email;
        _password = password;
    }

    // Thực hiện đăng nhập/đăng ký
    bool authenticate()
    {
        if (!_client)
            return false;
        _client->stop(); // Đảm bảo kết nối sạch trước khi bắt đầu
        delay(500);

        String host = "identitytoolkit.googleapis.com";
        String url;
        String body;

        if (_email == "")
        {
            // Anonymous
            url = "/v1/accounts:signUp?key=" + _apiKey;
            body = "{\"returnSecureToken\":true}";
        }
        else
        {
            // Email/Password
            url = "/v1/accounts:signInWithPassword?key=" + _apiKey;
            body = "{\"email\":\"" + _email + "\",\"password\":\"" + _password + "\",\"returnSecureToken\":true}";
        }

        if (_client->connect(host.c_str(), 443))
        {
            _client->print("POST " + url + " HTTP/1.1\r\n");
            _client->print("Host: " + host + "\r\n");
            _client->print("Content-Type: application/json\r\n");
            _client->print("Content-Length: " + String(body.length()) + "\r\n");
            _client->print("Connection: close\r\n\r\n");
            _client->print(body);

            // Đọc phản hồi sử dụng buffer cố định để tránh memory fragmentation
            char responseBuffer[2048];
            int respIdx = 0;
            unsigned long timeout = millis();
            while (_client->connected() && millis() - timeout < 5000)
            {
                if (_client->available() && respIdx < sizeof(responseBuffer) - 1)
                {
                    responseBuffer[respIdx++] = (char)_client->read();
                }
            }
            responseBuffer[respIdx] = '\0';
            _client->stop();

            // Trích xuất dữ liệu từ buffer
            String response(responseBuffer);
            _idToken = extractValue(response, "idToken");
            _refreshToken = extractValue(response, "refreshToken");
            _localId = extractValue(response, "localId");
            _tokenStartTime = millis() / 1000; // Lưu thời điểm nhận token
            response = "";                     // Giải phóng bộ nhớ của String

            return (_idToken.length() > 0);
        }
        return false;
    }
    bool tokenIsAlive()
    {
        if (_idToken.length() == 0)
            return false;
        uint32_t currentTime = millis() / 1000;
        return (currentTime - _tokenStartTime) < TOKEN_TIMEOUT || currentTime < _tokenStartTime; // Xử lý trường hợp millis() tràn
    }

    // Setters
    void setLocalId(String val) { _localId = val; }
    void setRefreshToken(String val) { _refreshToken = val; }
    void setIdToken(String val) { _idToken = val; }

    // Getters
    String getIdToken() { return _idToken; }
    String getRefreshToken() { return _refreshToken; }
    String getLocalId() { return _localId; }
};

#endif