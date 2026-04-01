
// --- THÔNG TIN CẤU HÌNH ---
#define WIFI_SSID "PC_NET_MOBILE"
#define WIFI_PASSWORD "1234567890"
#define API_KEY "AIzaSyCq8JmNI8-tQHTojwvBSuktYPLi8FmhHSg" // g
#define USER_EMAIL "phamvanchinh203@gmail.com"
#define USER_PASSWORD "phamchinh202"
#define DATABASE_URL "androidapp-41ff4-default-rtdb.firebaseio.com"

#include <WiFi.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>

// Thông tin WiFi
char ssid[] = "PC_NET_MOBILE";
char pass[] = "1234567890";

// Thông tin Google API
const char *server = "identitytoolkit.googleapis.com";
const char *apiKey = API_KEY; // Thay bằng API Key thật
const int port = 443;

WiFiSSLClient client;

void setup()
{
  Serial.begin(115200);

  // Kết nối WiFi
  Serial.print("Dang ket noi WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi da ket noi!");
}

void loop()
{
  // Thực hiện POST mỗi 30 giây (ví dụ)
  checkFreeHeap();
  postToGoogle();
  delay(30000);
}

void postToGoogle()
{
  Serial.println("\nDang thiet lap ket noi HTTPS...");

  // 1. Ket noi den Server
  if (client.connect(server, port))
  {
    Serial.println("Da ket noi thanh cong!");

    // 2. Chuan bi Body JSON
    String httpRequestData = "{\"returnSecureToken\": true}";

    // 3. Gui HTTP POST Request
    client.print(String("POST ") + "/v1/accounts:signUp?key=" + apiKey + " HTTP/1.1\r\n");
    client.print(String("Host: ") + server + "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(httpRequestData.length()) + "\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");          // Dong trong de ket thuc Header
    client.print(httpRequestData); // Gui Body

    // 4. Doc phan hoi tu Google
    unsigned long timeout = millis();
    while (client.available() == 0)
    {
      if (millis() - timeout > 5000)
      {
        Serial.println(">>> Timeout!");
        client.stop();
        return;
      }
    }

    Serial.println("Phan hoi tu Google:+++++++++++++++++++++++++++++++++++++++");
    String line;

    while (client.available())
    {
      line += client.readStringUntil('\r');
    }
    Serial.println(line);
    Serial.println("Phan hoi tu Google ket thuc:------------------------------------");
    // 2. Đọc phần Body
    // Vì có 'Transfer-Encoding: chunked', chúng ta cần đọc cẩn thận.
    // Cách đơn giản nhất cho BW16 là đọc đến khi gặp dấu '{' đầu tiên
    int jsonStart = line.indexOf('{');
    int jsonEnd = line.lastIndexOf('}');
    String jsonBody;
    Serial.println("Json getter : //////////////////////////");
    jsonBody = line.substring(jsonStart, jsonEnd + 1);
    Serial.println(jsonBody);

    // 3. Phân tích JSON (Parsing)
    // idToken của Google rất dài (>800 ký tự), nên dùng DynamicJsonDocument
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonBody);

    if (!error)
    {
      const char *idToken = doc["idToken"];
      const char *localId = doc["localId"];

      Serial.println("--- KẾT QUẢ TRÍCH XUẤT ---");
      Serial.print("User ID: ");
      Serial.println(localId);
      Serial.print("Token (đã lấy thành công): ");
      Serial.println(String(idToken).substring(0, 20) + "..."); // In thử 20 ký tự đầu

      // Lưu idToken này vào một biến toàn cục để dùng cho các request sau
    }
    else
    {
      Serial.print("Lỗi phân tích JSON: ");
      Serial.println(error.c_str());
    }
    //
    checkFreeHeap();

    client.stop(); // Ngat ket noi de giai phong RAM
  }
  else
  {
    Serial.println("Ket noi that bai!");
  }
}
void checkFreeHeap()
{
  Serial.print("Dynamic memory size: ");
  Serial.println(os_get_free_heap_size_arduino());
  Serial.println();
}