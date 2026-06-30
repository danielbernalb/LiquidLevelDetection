#ifndef LIQUID_LEVEL_DETECTION_H
#define LIQUID_LEVEL_DETECTION_H

#include <Arduino.h>

#if defined(ESP8266)
#include <SoftwareSerial.h>
#endif

class LiquidLevelDetection {
public:

    // ── Constructor ──────────────────────────────────────────────────────────
    // ESP32: acepta un puntero al puerto hardware (Serial1 por defecto, puede
    //        pasarse &Serial2 si Serial1 está ocupado)
    // ESP8266: solo necesita los pines RX/TX para crear el SoftwareSerial
    #if defined(ESP32)
    LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin,
                         uint8_t device_addr = 0x01,
                         HardwareSerial* serial_port = &Serial1);
    #elif defined(ESP8266)
    LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin,
                         uint8_t device_addr = 0x01);
    #endif

    ~LiquidLevelDetection();

    // ── API pública ──────────────────────────────────────────────────────────
    // Inicializa la comunicación. En ESP8266 usar máximo 9600 baud.
    bool begin(long baudrate = 9600);

    // Rango máximo configurado (unidad: metro). Retorna -1.0 si hay error.
    float getRange();

    // Distancia del sensor a la superficie del líquido (unidad: metro).
    // Retorna -1.0 si hay error de comunicación o CRC.
    float getEmptyHeight();

    // Escribe la altura de instalación en el sensor (unidad: centímetro).
    bool setInstallationHeight(float height_cm);

    // Altura de instalación configurada (unidad: metro). Retorna -1.0 si hay error.
    float getInstallationHeight();

    // Nivel de agua (unidad: metro). Retorna -1.0 si hay error.
    float getWaterLevel();

private:
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    uint8_t _device_addr;

    // Puntero al puerto serial: hardware en ESP32, virtual en ESP8266
    #if defined(ESP32)
    HardwareSerial* _serial;
    #elif defined(ESP8266)
    SoftwareSerial* _serial;
    #endif

    // ── Modbus internals ─────────────────────────────────────────────────────
    bool     sendModbusCommand(uint16_t reg_addr, uint16_t value);
    float    readModbusRegister(uint16_t reg_addr);
    uint16_t calculateCRC16(uint8_t* buffer, uint8_t length);
};

#endif // LIQUID_LEVEL_DETECTION_H