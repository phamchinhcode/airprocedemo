#ifndef FIREBASE_BW16_H
#define FIREBASE_BW16_H

#include <Arduino.h>
#include <WiFiSSLClient.h>
#include "GAuthBW16.h"

typedef void (*StreamCallback)(String path, String data);

class FirebaseBW16
{
private:
    WiFiSSLClient *_client;
    GAuthBW16 *_auth;
    String _host;
    String _pathStream;
    StreamCallback _onDataChange = nullptr; // Biến lưu trữ hàm callback
    bool _isStreaming = false;
    uint32_t _timerTokrnRefresh = 0;
    // Hàm nội bộ để kết nối và kiểm tra trạng thái SSL
    bool _reconnect()
    {
        if (_client->connected())
            return true;

        Serial.println("[Firebase] Reconnecting SSL...");
        _client->stop();
        delay(500);
        if (_client->connect(_host.c_str(), 443))
        {
            // Thiết lập timeout nhỏ để tránh treo khi đọc dữ liệu
            _client->setTimeout(2000);
            Serial.println("[Firebase] Reconnecting OK ...");
            return true;
        }
        Serial.println("[Firebase] Reconnecting Failed ...");
        return false;
    }

public:
    FirebaseBW16(WiFiSSLClient &client, GAuthBW16 &auth, String databaseUrl)
    {
        _client = &client;
        _auth = &auth;
        _host = databaseUrl;
        _host.replace("https://", "");
        if (_host.endsWith("/"))
            _host.remove(_host.length() - 1);
    }

    bool login()
    {
        return _auth->authenticate();
    }
    // Hàm để người dùng đăng ký hàm xử lý dữ liệu
    void setStreamCallback(StreamCallback cb)
    {
        _onDataChange = cb;
    }
    void startStream(String path)
    {
        _isStreaming = true;
        _pathStream = path;
    }
    void stopStream()
    {
        _isStreaming = false;
        stop();
    }
    void beginStream(String path)
    {
        if (!_reconnect())
            return;
        String fullPath = path + ".json?auth=" + _auth->getIdToken();
        _client->print("GET " + fullPath + " HTTP/1.1\r\n");
        _client->print("Host: " + _host + "\r\n");
        _client->print("Accept: text/event-stream\r\n");
        _client->print("Connection: keep-alive\r\n\r\n");
    }
    void loopStream()
    {
        if (_isStreaming)
        {
            if (!_client->connected())
            {

                Serial.println("[Firebase] Stream disconnected, attempting to reconnect...");
                beginStream(_pathStream);
                return;
            }
            if (_auth->tokenIsAlive() == false)
            {
                Serial.println("[Firebase] Token expired during streaming, attempting to refresh...");
                if (login())
                {
                    Serial.println("[Firebase] Token refreshed successfully, restarting stream...");
                    beginStream(_pathStream);
                }
                else
                {
                    Serial.println("[Firebase] Token refresh failed during streaming.");
                    stopStream();
                }
                return;
            }
            if (_client->available())
            {
                // Serial.println("Reaad ..");
                String line = _client->readStringUntil('\n');
                // Serial.println(line);
                // Serial.println("ok read ..");
                line.trim();

                if (line.startsWith("data:"))
                {
                    String jsonData = line.substring(5);
                    jsonData.trim();

                    if (jsonData == "null")
                    {
                        Serial.println("[STREAM] keep alive  ----");
                        jsonData = "";
                        line = "";
                        return;
                    }

                    // Tách Path và Data thô từ gói tin Stream của Firebase
                    // Định dạng: {"path":"/led","data":1}
                    int pathIdx = jsonData.indexOf("\"path\":\"");
                    int dataIdx = jsonData.indexOf("\"data\":");

                    if (pathIdx != -1 && dataIdx != -1)
                    {
                        String path = jsonData.substring(pathIdx + 8, jsonData.indexOf("\"", pathIdx + 8));
                        String data = jsonData.substring(dataIdx + 7, jsonData.lastIndexOf("}"));

                        // Nếu đã đăng ký hàm callback thì gọi nó
                        if (_onDataChange != nullptr)
                        {
                            _onDataChange(path, data);
                        }
                        // Giải phóng bộ nhớ của path và data ngay sau khi sử dụng
                        path = "";
                        data = "";
                    }
                    // Giải phóng bộ nhớ của jsonData
                    jsonData = "";
                }
                // Giải phóng bộ nhớ của line
                line = "";
            }
        }
    }
    // Phương thức PUT JSON
    bool put(String path, String jsonPayload)
    {
        if (_auth->tokenIsAlive() == false)
        {
            Serial.println("[Firebase] Token expired, cannot perform PUT request.");
            if (login())
            {
                Serial.println("[Firebase] Token refreshed successfully.");
                _reconnect();
            }
            else
            {
                Serial.println("[Firebase] Token refresh failed.");
                return false;
            }
        }
        if (!_reconnect())
            return false;

        jsonPayload.replace(" ", ""); // Loại bỏ newline để so sánh chính xác hơn
        String fullPath = path;
        if (!fullPath.startsWith("/"))
            fullPath = "/" + fullPath;
        fullPath += ".json?auth=" + _auth->getIdToken();

        // Gửi Header sử dụng Keep-Alive
        _client->print("PUT " + fullPath + " HTTP/1.1\r\n");
        _client->print("Host: " + _host + "\r\n");
        _client->print("Content-Type: application/json\r\n");
        _client->print("Content-Length: " + String(jsonPayload.length()) + "\r\n");
        _client->print("Connection: keep-alive\r\n\r\n");
        _client->print(jsonPayload);

        // Đọc phản hồi nhanh để kiểm tra 200 OK
        unsigned long startWait = millis();
        while (millis() - startWait < 3000)
        {
            // if (_client->available() > 0)
            // {

            //     char c = _client->read();
            //     Serial.print(c);
            //     // if (line.indexOf("200 OK") != -1)
            //     // {
            //     //     line = "";
            //     //     return true;
            //     // }
            //     // line = ""; // Giải phóng bộ nhớ sau mỗi dòng đọc
            // }
            if (_client->available())
            {
                String line = _client->readStringUntil('\n');
                // if (line.indexOf("200 OK") != -1)
                // {
                //     line = "";
                //     return true;
                // }

                if (line == jsonPayload)
                {
                    line = "";
                    return true;
                }
                line = ""; // Giải phóng bộ nhớ sau mỗi dòng đọc
            }
        }
        return false;
    }

    // Phương thức GET JSON tối ưu
    String get(String path)
    {
        if (_auth->tokenIsAlive() == false)
        {
            Serial.println("[Firebase] Token expired, cannot perform PUT request.");
            if (login())
            {
                Serial.println("[Firebase] Token refreshed successfully.");
                _reconnect();
            }
            else
            {
                Serial.println("[Firebase] Token refresh failed.");
                return "Error: Token Expired";
            }
        }
        // Serial.println("4");
        if (!_reconnect())
            return "Error: Connection Failed";
        // Serial.println("5");
        //  _client->flush(); // Đảm bảo buffer sạch trước khi gửi yêu cầu mới
        //  Serial.println("7");
        String fullPath = path;
        if (!fullPath.startsWith("/"))
            fullPath = "/" + fullPath;
        fullPath += ".json?auth=" + _auth->getIdToken();

        _client->print("GET " + fullPath + " HTTP/1.1\r\n");
        _client->print("Host: " + _host + "\r\n");
        _client->print("Connection: keep-alive\r\n\r\n");

        unsigned long startWait = millis();
        // Serial.println("0");
        while (_client->available() == 0 && millis() - startWait < 3000)
            ;
        // Serial.println("0");
        //  Sử dụng buffer cố định thay vì String concatenation để tránh memory leak
        char responseBuffer[4096];
        int respIdx = 0;
        char headerBuffer[1024];
        int headerIdx = 0;
        bool isBody = false;
        int braceCounter = 0;
        bool foundJson = false;

        while (_client->available() || _client->connected())
        {
            if (_client->available())
            {
                char c = _client->read();
                // Serial.print(c); // Debug: In từng ký tự nhận được

                if (!isBody)
                {
                    if (headerIdx < sizeof(headerBuffer) - 1)
                        headerBuffer[headerIdx++] = c;
                    else
                        Serial.println("Header buffer overflow!"); // Debug: Kiểm tra tràn header buffer

                    // Kiểm tra end of headers
                    if (headerIdx >= 4 && headerBuffer[headerIdx - 4] == '\r' &&
                        headerBuffer[headerIdx - 3] == '\n' &&
                        headerBuffer[headerIdx - 2] == '\r' &&
                        headerBuffer[headerIdx - 1] == '\n')
                    {
                        isBody = true;
                        respIdx = 0;
                    }
                }
                else
                {
                    // Serial.print(c); // Debug: In từng ký tự nhận được
                    if (respIdx < sizeof(responseBuffer) - 1)
                        responseBuffer[respIdx++] = c;
                    // Logic nhận diện JSON
                    if (c == '{')
                    {
                        braceCounter++;
                        foundJson = true;
                    }
                    if (c == '}')
                    {
                        braceCounter--;
                        foundJson = true;
                    }

                    // Nếu đã bắt đầu nhận JSON và ngoặc đã đóng hết
                    if (foundJson && braceCounter == 0)
                        break;
                }
            }
            else
            {
                delay(1);
                if (millis() - startWait > 5000)
                {
                    // Serial.println("1");
                    break;
                }
            }
        }
        responseBuffer[respIdx] = '\0';

        // Xử lý kết quả từ buffer
        String response(responseBuffer);
        // Serial.println("Raw response: " + String(responseBuffer));

        int start = response.indexOf('{');
        int end = response.lastIndexOf('}');

        if (start != -1 && end != -1)
        {
            String result = response.substring(start, end + 1);
            response = ""; // Giải phóng bộ nhớ
            return result;
        }

        response.trim();
        String result = response;
        response = ""; // Giải phóng bộ nhớ
        return result;
    }

    // Hàm chủ động đóng kết nối khi cần giải phóng RAM hoàn toàn
    void stop()
    {
        if (_client)
            _client->stop();
    }
};

#endif