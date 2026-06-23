#ifndef LIQUID_LEVEL_DETECTION_H
#define LIQUID_LEVEL_DETECTION_H

#include <Arduino.h>

// Si es un ESP8266, necesitamos incluir la librería de SoftwareSerial
#if defined(ESP8266)
#include <SoftwareSerial.h>
#endif

class LiquidLevelDetection {
public:
    // El constructor se mantiene idéntico para que tu archivo principal no cambie
    LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin, uint8_t device_addr = 0x01);
    ~LiquidLevelDetection(); // Destructor para liberar memoria en el ESP8266
    
    // Initialize sensor communication, set baudrate
    bool begin(long baudrate = 115200);
    
    // Get current range (unit: meter)
    float getRange();
    
    // Get empty height (distance from sensor to liquid surface, unit: meter)
    float getEmptyHeight();
    
    // Set installation height (unit: centimeter)
    bool setInstallationHeight(float height_cm);
    
    // Get installation height (return unit: meter)
    float getInstallationHeight();
    
    // Get water level height (unit: meter)
    float getWaterLevel();

private:
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    uint8_t _device_addr;

    // Aquí ocurre la magia: El tipo de puntero cambia según la arquitectura de la placa
    #if defined(ESP32)
    HardwareSerial* _serial;
    #elif defined(ESP8266)
    SoftwareSerial* _serial;
    #endif
    
    bool sendModbusCommand(uint16_t reg_addr, uint16_t value);
    float readModbusRegister(uint16_t reg_addr);
    uint16_t calculateCRC16(uint8_t* buffer, uint8_t length);
};

#endif