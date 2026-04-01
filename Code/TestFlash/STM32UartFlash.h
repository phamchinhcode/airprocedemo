#ifndef STM32UARTFLASH_H
#define STM32UARTFLASH_H

#include <Arduino.h>

class STM32UartFlash
{
public:
    STM32UartFlash(HardwareSerial &serial);
    bool begin(int boot0Pin, int resetPin);
    bool sync();
    bool eraseAll();
    // Hàm nạp 1024 byte
    bool write1024(uint32_t startAddress, uint8_t *data);
    void hardReset();

    uint16_t getPID();
    uint16_t getFlashSize(uint32_t address);

private:
    HardwareSerial *_serial;
    int _boot0, _reset;
    uint32_t timeout;
    bool waitACK(uint32_t timeout = 500);
    uint8_t calculateChecksum(uint8_t *data, int len);
    bool writeBlock256(uint32_t address, uint8_t *data);
};

#endif