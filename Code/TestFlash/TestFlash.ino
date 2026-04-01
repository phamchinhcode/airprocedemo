#include <WiFi.h>
#include <WiFiSSLClient.h>
#include "STM32UartFlash.h"

#define STM32_BOOT0_PIN PA12
#define STM32_RESET_PIN PA13

char ssid[] = "PC_NET_MOBILE";
char pass[] = "1234567890";
const char *host = "raw.githubusercontent.com";
const char *url = "/PhamChinhCode/AirProceDemo/refs/heads/main/Code/Firmware/firmware.bin";
// https://raw.githubusercontent.com/PhamChinhCode/AirProceDemo/refs/heads/main/Code/Firmware/firmware.bin
WiFiSSLClient sslClient;
STM32UartFlash stm32(Serial1);

uint32_t mil;

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

    // pinMode(PA12, OUTPUT);
    // pinMode(PA13, OUTPUT);
    pinMode(PA14, OUTPUT);

    stm32.begin(STM32_BOOT0_PIN, STM32_RESET_PIN);
    stm32.hardReset();

    delay(500);

    if (!stm32.sync())
    {
        Serial.println("Failed to sync with STM32!");
        return;
    }
    else
    {
        Serial.println("Sync with STM32 successful!");
        checkBeforeFlash();
    }

    Serial.println("Connecting to GitHub...");
    if (!sslClient.connect(host, 443))
    {
        Serial.println("Connection failed!");
        return;
    }
    else
    {
        Serial.println("Connected to GitHub!");
        mil = millis();
        // Gửi HTTP Request
        sslClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "User-Agent: BW16-STM32\r\n" +
                        "Cache-Control: no-cache\r\n" +
                        "Pragma: no-cache\r\n" +
                        "Connection: close\r\n\r\n");
        // Kiểm tra phản hồi từ Server
        while (sslClient.available() == 0 && millis() - mil < 5000)
            ;
        mil = millis();
        // Đọc nội dung file và in ra Serial (Đây là nơi bạn sẽ gửi sang STM32)
        uint32_t index = 0;
        uint32_t numOfByte;
        int headerIdx = 0;
        bool headerEnded = false;
        uint32_t currentAddr = 0x08000000; // Địa chỉ bắt đầu của Flash STM32
        uint8_t pageBuffer[1024];          // Buffer tạm để gom đủ 1024 byte
        int bufferIdx = 0;
        bool writeSuccess = true;
        while (sslClient.available() > 0 && millis() - mil < 5000)
        {
            if (!headerEnded)
            {
                String c = sslClient.readStringUntil('\n');
                // Serial.println(c); // Debug: In từng ký tự nhận được
                if (c.indexOf("Content-Length:") != -1)
                {
                    String contentLengthStr = c.substring(c.indexOf(":") + 1);
                    contentLengthStr.trim();
                    numOfByte = contentLengthStr.toInt();
                }
                // Kiểm tra end of headers
                if (c == "\r")
                {
                    headerEnded = true;
                    index = 0;
                    Serial.println("Header end <<<\n----------------------------------------");
                    stm32.sync(); // Đồng bộ lại với STM32 trước khi bắt đầu gửi dữ liệu
                    Serial.println("\n[1/3] Erasing Flash...");
                    if (!stm32.eraseAll())
                    {
                        Serial.println("Erase failed! Aborting...");
                        sslClient.stop();
                        return;
                    }
                    Serial.println("Erase OK!");
                    Serial.println("[2/3] Writing Firmware...");
                }
            }
            else
            {
                if (sslClient.available())
                {
                    mil = millis();
                    pageBuffer[bufferIdx++] = sslClient.read();

                    // Khi gom đủ 1024 byte, tiến hành ghi vào STM32
                    if (bufferIdx == 1024)
                    {
                        if (!stm32.write1024(currentAddr, pageBuffer))
                        {
                            Serial.println("\nWrite failed at 0x" + String(currentAddr, HEX));
                            writeSuccess = false;
                            break;
                        }
                        currentAddr += 1024;
                        index += 1024;
                        bufferIdx = 0;     // Reset buffer cho khối tiếp theo
                        Serial.print("."); // In dấu chấm để báo hiệu đang tiến triển
                    }
                }
                if (millis() - mil > 10000)
                {
                    Serial.println("\nError: SSL Timeout!");
                    writeSuccess = false;
                    break;
                }
            }
        }
        // 3. Xử lý khối dữ liệu cuối cùng (nếu còn dư dưới 1024 byte)
        if (writeSuccess && bufferIdx > 0)
        {
            index += bufferIdx;
            // Serial.println("\nWriting last chunk with padding...");
            //  Bù byte 0xFF cho đủ 1024 byte
            for (int i = bufferIdx; i < 1024; i++)
            {
                pageBuffer[i] = 0xFF;
            }
            if (!stm32.write1024(currentAddr, pageBuffer))
            {
                writeSuccess = false;
            }
            Serial.println("Done !");
        }
        if (writeSuccess)
        {
            Serial.println("\n[3/3] Firmware Update Successful!");
            Serial.println("Total " + String(index) + " bytes written: " + String(currentAddr - 0x08000000 + (bufferIdx > 0 ? 1024 : 0)));

            // Đưa BOOT0 về 0 và Reset để chạy chương trình mới
            stm32.hardReset();
            Serial.println("STM32 is now running new firmware.");
        }
        else
        {
            Serial.println("\nUpdate Failed!");
        }
        Serial.println("\n----------------------------------------");
        Serial.println("Finished reading from GitHub " + String(numOfByte) + " byte. Total bytes: " + String(index));

        sslClient.stop();
    }
}

void loop()
{
    if (Serial.available() > 0)
    {
        String x = Serial.readString();
        x.trim();
        if (x == "reset")
        {
            stm32.hardReset();
            Serial.println("STM32 RESET: OK");
        }

        if (x == "sync")
        {
            Serial.print("STM32 SYNC: ");
            Serial.println(stm32.sync() ? "OK" : "Failed");
        }
        if (x == "erase")
        {
            Serial.print("STM32 ERASE: ");
            Serial.println(stm32.eraseAll() ? "OK" : "Failed");
        }
        if (x == "check")
        {
            checkBeforeFlash();
        }
        if (x == "id")
        {
            mil = millis();
            stm32.sync();
            Serial1.write(0x02);
            Serial1.write(0xFD);
            while (Serial1.available() == 0 && millis() - mil < 5000)
                ;
            mil = millis();
            while (Serial1.available() > 0 && millis() - mil < 5000)
            {
                uint8_t c = Serial1.read();
                if (c < 0x10)
                    Serial.print("0");
                Serial.print(c, HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    }
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
void checkBeforeFlash()
{
    uint16_t pid = stm32.getPID();
    Serial.print("Chip PID: 0x");
    Serial.println(pid, HEX);

    if (pid == 0x0412)
    { // Nếu đúng là F103
        uint16_t size = stm32.getFlashSize(0x1FFFF7E0);
        Serial.print("Flash Size: ");
        Serial.print(size);
        Serial.println(" KB");

        // Giả sử file bin từ GitHub của bạn nặng 40KB
        if (size < 40)
        {
            Serial.println("ERROR: Over flash size !");
            return;
        }
        // Tiến hành Erase và Write...
    }
    else
    {
        Serial.println("ERROR: Not chip target !");
    }
}