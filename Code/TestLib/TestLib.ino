#include <WiFi.h>
#include <WiFiSSLClient.h>
#include "GAuthBW16.h"
#include "FirebaseBW16.h"

char ssid[] = "PC_NET_MOBILE";
char pass[] = "1234567890";
// char ssid[] = "180/ thang";
// char pass[] = "admin092618#";
String API_KEY = "AIzaSyCq8JmNI8-tQHTojwvBSuktYPLi8FmhHSg";
String DATABASE_URL = "androidapp-41ff4-default-rtdb.firebaseio.com";

const char *host = "raw.githubusercontent.com";
const char *url = "/PhamChinhCode/AirProceDemo/main/Code/Firmware/test.txt";

WiFiSSLClient sslClient;
WiFiSSLClient sslStream; // Dành riêng cho Stream
GAuthBW16 authAnon(sslClient, API_KEY, "phamvanchinh203@gmail.com", "phamchinh202");
GAuthBW16 auth(sslStream, API_KEY, "phamvanchinh203@gmail.com", "phamchinh202"); // Auth dùng chung Token
FirebaseBW16 firebase(sslClient, authAnon, DATABASE_URL);
FirebaseBW16 fbStream(sslStream, auth, DATABASE_URL);

uint32_t mil;

// --- ĐÂY LÀ HÀM XỬ LÝ CỦA BẠN ---
void myDataHandler(String path, String data)
{
    Serial.println("---------- NEW DATA UPDATE ----------");
    Serial.println("Path: " + path);
    Serial.println("Value: " + data);

    // Ví dụ điều khiển thiết bị
    if (path == "/led")
    {
        int value = data.toInt();
        switch (value)
        {
        case 0:
            digitalWrite(PA12, LOW);
            digitalWrite(PA13, LOW);
            digitalWrite(PA14, LOW);
            /* code */
            break;
        case 1:
            digitalWrite(PA12, HIGH);
            break;
        case 2:
            digitalWrite(PA13, HIGH);
            break;
        case 3:
            digitalWrite(PA14, HIGH);
            break;

        default:
            digitalWrite(PA12, 1);
            digitalWrite(PA13, 1);
            digitalWrite(PA14, 1);
            break;
        }
    }
    Serial.println("------------------------------------");
}

void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);
    Serial.println("\nConnected to WiFi");
    Serial.println("Unique ID: " + getUniqueID());
    Serial.println(showMemory());
    Serial.println();

    pinMode(PA12, OUTPUT);
    pinMode(PA13, OUTPUT);
    pinMode(PA14, OUTPUT);

    Serial.println("\n--- TEST AUTH ---");

    // Thử đăng nhập Email
    // if (authAnon.authenticate())
    // {
    //     Serial.println("Dang nhap Anonymous thanh cong!");
    //     Serial.println("LocalID: " + authAnon.getLocalId());
    //     Serial.println("Token: " + authAnon.getIdToken().substring(0, 15) + "...");
    //     Serial.println("Refresh Token: " + authAnon.getRefreshToken().substring(0, 15) + "...");
    // }
    // else
    // {
    //     Serial.println("Dang nhap Anonymous that bai!");
    // }
    // if (auth.authenticate())
    // {
    //     fbStream.setStreamCallback(myDataHandler);
    //     fbStream.startStream("/posts2"); // Stream toàn bộ DB hoặc node cụ thể
    // }
    // Serial.println("Auth OK! Token: " + authAnon.getIdToken().substring(0, 10) + "...");
    // Khởi tạo kết nối bảo mật
    // WiFiClient client;

    // Lưu ý: Với BW16/AmebaD, nếu không muốn check chứng chỉ quá ngặt nghèo (để test nhanh)
    // bạn có thể dùng lệnh: client.setInsecure();

    Serial.println("Connecting to GitHub...");
    if (!sslClient.connect(host, 443))
    {
        Serial.println("Connection failed!");
        return;
    }
    else
    {
        Serial.println("Connected to GitHub!");
    }

    // Gửi HTTP Request
    String dynamicUrl = String(url) + "?t=" + String(millis());
    sslClient.print(String("GET ") + dynamicUrl + " HTTP/1.1\r\n" +
                    "Host: " + host + "\r\n" +
                    "User-Agent: BW16-STM32\r\n" +
                    "Cache-Control: no-cache\r\n" +
                    "Pragma: no-cache\r\n" +
                    "Connection: close\r\n\r\n");

    // Kiểm tra phản hồi từ Server
    while (sslClient.connected())
    {
        if (sslClient.available())
        {
            char c = sslClient.read();
            Serial.write(c);
        }
        // String line = client.readStringUntil('\n');
        // if (line == "\r")
        // {
        //     Serial.println("Headers received, start downloading body...");
        //     break;
        // }
    }

    // Đọc nội dung file và in ra Serial (Đây là nơi bạn sẽ gửi sang STM32)
    while (sslClient.available())
    {
        char c = sslClient.read();
        Serial.write(c);
        // Sau này thay Serial.write(c) bằng Serial1.write(c) để gửi sang STM32
    }

    sslClient.stop();
}

void loop()
{

    // fbStream.loopStream(); // Luôn lắng nghe

    // if (millis() - mil > 6000)
    // {
    //     mil = millis();

    //     Serial.println(showMemory());

    //     // BƯỚC 2: Gửi dữ liệu (PUT)
    //     String data = "{\"nhiet_do\": " + String(millis()) + ", \"trang_thai\": \"ON\"}";
    //     if (firebase.put("/thiet_bi_1", data))
    //     {
    //         Serial.println("Du lieu gui di: " + data);
    //     }
    //     else
    //     {
    //         Serial.println("Ghi du lieu that bai!");
    //     }
    //     // Serial.println("6");
    //     String result = firebase.get("/thiet_bi_1");
    //     Serial.println("Du lieu doc ve: " + result);
    // }
}
String getUniqueID()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char uniqueID[13];
    sprintf(uniqueID, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(uniqueID);
}
String showMemory()
{
    return "[SYSTEM MEMORY]: " + String(os_get_free_heap_size_arduino()) + " Byte\n";
}