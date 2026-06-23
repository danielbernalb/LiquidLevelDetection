#include "LiquidLevelDetection.h"

LiquidLevelDetection::LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin, uint8_t device_addr) {
    _rx_pin = rx_pin;
    _tx_pin = tx_pin;
    _device_addr = device_addr;

    #if defined(ESP32)
    _serial = &Serial1; // En ESP32 usamos el puerto nativo de Hardware Serial1
    #elif defined(ESP8266)
    _serial = new SoftwareSerial(_rx_pin, _tx_pin); // En ESP8266 creamos un puerto virtual
    #endif
}

LiquidLevelDetection::~LiquidLevelDetection() {
    #if defined(ESP8266)
    if (_serial != nullptr) {
        delete _serial; // Liberamos memoria solo si se creó el objeto virtual en ESP8266
    }
    #endif
}

bool LiquidLevelDetection::begin(long baudrate) {
    #if defined(ESP32)
    // En el ESP32 iniciamos el HardwareSerial asignándole los pines elegidos dinámicamente
    _serial->begin(baudrate, SERIAL_8N1, _rx_pin, _tx_pin);
    #elif defined(ESP8266)
    _serial->begin(baudrate);
    #endif
    return true;
}

float LiquidLevelDetection::getRange() {
    return readModbusRegister(0x07D4);
}

float LiquidLevelDetection::getEmptyHeight() {
    float val = readModbusRegister(0x0001);
    if (val < 0) return -1.0; // Retorna -1.0 si hubo error de lectura/CRC
    return val;
}

bool LiquidLevelDetection::setInstallationHeight(float height_cm) {
    return sendModbusCommand(0x0005, (uint16_t)height_cm);
}

float LiquidLevelDetection::getInstallationHeight() {
    float val = readModbusRegister(0x0005);
    if (val < 0) return -1.0;
    return val / 100.0;
}

float LiquidLevelDetection::getWaterLevel() {
    float val = readModbusRegister(0x0003);
    if (val < 0) return -1.0;
    return val;
}

bool LiquidLevelDetection::sendModbusCommand(uint16_t reg_addr, uint16_t value) {
    // 1. LIMPIEZA: Vaciar cualquier residuo o basura previa en el búfer
    while (_serial->available() > 0) {
        _serial->read();
    }

    uint8_t buffer[8];
    buffer[0] = _device_addr;    // Dirección del dispositivo
    buffer[1] = 0x06;            // Código de función: escribir registro único
    buffer[2] = reg_addr >> 8;   
    buffer[3] = reg_addr & 0xFF; 
    buffer[4] = value >> 8;      
    buffer[5] = value & 0xFF;    
    
    uint16_t crc = calculateCRC16(buffer, 6);
    buffer[6] = crc & 0xFF;      
    buffer[7] = crc >> 8;        
    
    _serial->write(buffer, 8);
    
    // 2. TIMEOUT DINÁMICO: Esperar respuesta (espera 8 bytes para la función 0x06)
    unsigned long startTime = millis();
    while (_serial->available() < 8) {
        if (millis() - startTime > 150) { // Timeout de 150ms
            return false;
        }
        yield(); // Permite que el ESP8266 atienda el Wi-Fi y evita reseteos por Watchdog
    }
    
    uint8_t response[8];
    for (int i = 0; i < 8; i++) {
        response[i] = _serial->read();
    }
    
    // 3. VALIDACIÓN DE CRC
    uint16_t receivedCRC = (response[7] << 8) | response[6];
    uint16_t calculatedCRC = calculateCRC16(response, 6);
    if (receivedCRC != calculatedCRC) {
        return false; // Datos corruptos, ignorar
    }
    
    return true;
}

float LiquidLevelDetection::readModbusRegister(uint16_t reg_addr) {
    // 1. LIMPIEZA: Vaciar cualquier residuo o basura previa en el búfer antes de preguntar
    while (_serial->available() > 0) {
        _serial->read();
    }

    uint8_t buffer[8];
    buffer[0] = _device_addr;    // Dirección del dispositivo
    buffer[1] = 0x03;            // Código de función: leer registro de retención
    buffer[2] = reg_addr >> 8;   
    buffer[3] = reg_addr & 0xFF; 
    buffer[4] = 0x00;            
    buffer[5] = 0x01;            // Cantidad de registros a leer (1)
    
    uint16_t crc = calculateCRC16(buffer, 6);
    buffer[6] = crc & 0xFF;      
    buffer[7] = crc >> 8;        
    
    _serial->write(buffer, 8);
    
    // 2. TIMEOUT DINÁMICO: Esperar respuesta (7 bytes para la función 0x03 leyendo 1 registro)
    unsigned long startTime = millis();
    while (_serial->available() < 7) {
        if (millis() - startTime > 150) { // Timeout de 150ms
            return -1.0; // Retornamos -1.0 para indicar un error de comunicación
        }
        yield(); 
    }
    
    uint8_t response[7];
    for (int i = 0; i < 7; i++) {
        response[i] = _serial->read();
    }
    
    // 3. VALIDACIÓN DE CRC: Asegurar que el mensaje no se haya corrompido
    uint16_t receivedCRC = (response[6] << 8) | response[5];
    uint16_t calculatedCRC = calculateCRC16(response, 5);
    if (receivedCRC != calculatedCRC) {
        return -1.0; // Error de CRC, ignorar medición ruidosa
    }
    
    // Extraer datos de manera segura
    uint16_t value = (response[3] << 8) | response[4];
    return (float)value;
}

uint16_t LiquidLevelDetection::calculateCRC16(uint8_t* buffer, uint8_t length) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}