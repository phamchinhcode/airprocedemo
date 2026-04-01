#include "STM32UartFlash.h"

STM32UartFlash::STM32UartFlash(HardwareSerial &serial)
{
    _serial = &serial;
}

bool STM32UartFlash::begin(int boot0Pin, int resetPin)
{
    _boot0 = boot0Pin;
    _reset = resetPin;
    pinMode(_boot0, OUTPUT);
    pinMode(_reset, OUTPUT);
    digitalWrite(_boot0, LOW);
    digitalWrite(_reset, HIGH);
    // Lưu ý: BW16 cần cấu hình 8E1 (8 bit, Even parity, 1 stop bit)
    _serial->begin(115200, SERIAL_8E1);
    return true;
}

void STM32UartFlash::hardReset()
{
    digitalWrite(_boot0, LOW);
    digitalWrite(_reset, LOW);
    delay(100);
    digitalWrite(_reset, HIGH);
}

bool STM32UartFlash::sync()
{
    digitalWrite(_boot0, HIGH);
    digitalWrite(_reset, LOW);
    delay(100);
    digitalWrite(_reset, HIGH);
    delay(200);

    _serial->write(0x7F);
    return waitACK(1000);
}

bool STM32UartFlash::waitACK(uint32_t timeout)
{
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
        if (_serial->available())
        {
            uint8_t res = _serial->read();
            if (res == 0x79)
                return true;
            if (res == 0x1F)
                return false;
        }
    }
    return false;
}

bool STM32UartFlash::writeBlock256(uint32_t addr, uint8_t *data)
{
    // 1. Gửi lệnh Write Memory (0x31)
    _serial->write(0x31);
    _serial->write(0xCE);
    if (!waitACK(500))
        return false;

    // 2. Gửi địa chỉ (4 byte) + Checksum địa chỉ
    uint8_t addrBuf[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    uint8_t addrCS = addrBuf[0] ^ addrBuf[1] ^ addrBuf[2] ^ addrBuf[3];
    _serial->write(addrBuf, 4);
    _serial->write(addrCS);
    if (!waitACK(500))
        return false;

    // 3. Gửi số lượng byte (N-1) + Dữ liệu + Checksum dữ liệu
    uint8_t n = 255; // n-1 = 255 tức là gửi 256 byte
    _serial->write(n);
    uint8_t dataCS = n;
    for (int i = 0; i < 256; i++)
    {
        _serial->write(data[i]);
        dataCS ^= data[i];
    }
    _serial->write(dataCS);
    return waitACK(1000);
}

bool STM32UartFlash::write1024(uint32_t startAddress, uint8_t *data)
{
    for (int i = 0; i < 4; i++)
    {
        uint32_t currentAddr = startAddress + (i * 256);
        if (!writeBlock256(currentAddr, &data[i * 256]))
        {
            return false; // Thất bại tại khối thứ i
        }
    }
    return true;
}
bool STM32UartFlash::eraseAll()
{
    // 1. Gửi lệnh Erase (0x43) và Checksum nghịch đảo (0xBC)
    _serial->write(0x43);
    _serial->write(0xBC);

    if (!waitACK(2000))
    { // Đợi ACK đầu tiên (Xác nhận lệnh)
        return false;
    }

    // 2. Gửi tham số Special Erase (0xFF) để xóa toàn bộ Flash
    // Theo giao thức: 0xFF + Checksum (0x00)
    _serial->write((uint8_t)0xFF);
    _serial->write((uint8_t)0x00);

    // Đợi phản hồi ACK (Quá trình xóa có thể mất từ 500ms đến vài giây tùy dung lượng Flash)
    if (!waitACK(5000))
    {
        return false;
    }
    return true;
}
uint16_t STM32UartFlash::getPID()
{
    _serial->write(0x02);
    _serial->write(0xFD);

    if (!waitACK(500))
        return 0;
    timeout = millis();
    while (_serial->available() < 3 || millis() - timeout > 1000)
        ;

    uint8_t len = _serial->read(); // Thường là 0x01 (nghĩa là có 2 byte ID)
    uint8_t msb = _serial->read();
    uint8_t lsb = _serial->read();

    waitACK(500); // Nhận byte ACK kết thúc lệnh

    return (uint16_t)(msb << 8 | lsb);
}

uint16_t STM32UartFlash::getFlashSize(uint32_t address)
{
    // Gửi lệnh Read Memory (0x11)
    _serial->write(0x11);
    _serial->write(0xEE);
    if (!waitACK(500))
        return 0;

    // Gửi địa chỉ (ví dụ 0x1FFFF7E0 cho F1)
    uint8_t addrBuf[4] = {(uint8_t)(address >> 24), (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    uint8_t addrCS = addrBuf[0] ^ addrBuf[1] ^ addrBuf[2] ^ addrBuf[3];
    _serial->write(addrBuf, 4);
    _serial->write(addrCS);
    if (!waitACK(500))
        return 0;

    // Gửi số lượng byte muốn đọc (N-1). Muốn đọc 2 byte thì gửi 0x01
    _serial->write(0x01);
    _serial->write(0xFE); // Checksum của 0x01
    if (!waitACK(500))
        return 0;
    while (_serial->available() < 2 || millis() - timeout > 1000)
        ;
    uint8_t lsb = _serial->read();
    uint8_t msb = _serial->read();
    return (uint16_t)(msb << 8 | lsb);
}