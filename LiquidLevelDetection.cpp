#include "LiquidLevelDetection.h"

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

#if defined(ESP32)
LiquidLevelDetection::LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin,
                                           uint8_t device_addr,
                                           HardwareSerial* serial_port) {
    _rx_pin      = rx_pin;
    _tx_pin      = tx_pin;
    _device_addr = device_addr;
    _serial      = serial_port;   // FIX: configurable, no hardcodeado a Serial1
}

#elif defined(ESP8266)
LiquidLevelDetection::LiquidLevelDetection(uint8_t rx_pin, uint8_t tx_pin,
                                           uint8_t device_addr) {
    _rx_pin      = rx_pin;
    _tx_pin      = tx_pin;
    _device_addr = device_addr;
    _serial      = new SoftwareSerial(rx_pin, tx_pin);
}
#endif

LiquidLevelDetection::~LiquidLevelDetection() {
    #if defined(ESP8266)
    if (_serial != nullptr) {
        delete _serial;
        _serial = nullptr;
    }
    #endif
}

// ════════════════════════════════════════════════════════════════════════════
// begin()
// ════════════════════════════════════════════════════════════════════════════

bool LiquidLevelDetection::begin(long baudrate) {
    #if defined(ESP32)
    // Asigna los pines dinámicamente al iniciar (no en el constructor)
    _serial->begin(baudrate, SERIAL_8N1, _rx_pin, _tx_pin);
    #elif defined(ESP8266)
    _serial->begin(baudrate);
    #endif
    delay(100); // Tiempo de estabilización del puerto serial
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// API pública
// ════════════════════════════════════════════════════════════════════════════

float LiquidLevelDetection::getRange() {
    float val = readModbusRegister(0x07D4); // FIX: ahora también valida -1.0
    if (val < 0) return -1.0;
    return val;
}

float LiquidLevelDetection::getEmptyHeight() {
    float val = readModbusRegister(0x0001); // Registro en mm
    if (val < 0) return -1.0;
    return val / 1000.0; // Convertir a metros
}

bool LiquidLevelDetection::setInstallationHeight(float height_cm) {
    return sendModbusCommand(0x0005, (uint16_t)height_cm);
}

float LiquidLevelDetection::getInstallationHeight() {
    float val = readModbusRegister(0x0005); // Registro en cm
    if (val < 0) return -1.0;
    return val / 100.0; // Convertir a metros
}

float LiquidLevelDetection::getWaterLevel() {
    float val = readModbusRegister(0x0003); // Registro en mm
    if (val < 0) return -1.0;
    return val / 1000.0; // Convertir a metros
}

// ════════════════════════════════════════════════════════════════════════════
// Modbus: escritura de registro (FC06)
// ════════════════════════════════════════════════════════════════════════════

bool LiquidLevelDetection::sendModbusCommand(uint16_t reg_addr, uint16_t value) {
    // FIX: Vaciar buffer RX antes de enviar para evitar desalineación de frames
    while (_serial->available() > 0) _serial->read();

    uint8_t buffer[8];
    buffer[0] = _device_addr;
    buffer[1] = 0x06;             // FC06: write single register
    buffer[2] = reg_addr >> 8;
    buffer[3] = reg_addr & 0xFF;
    buffer[4] = value >> 8;
    buffer[5] = value & 0xFF;

    uint16_t crc = calculateCRC16(buffer, 6);
    buffer[6] = crc & 0xFF;       // CRC byte bajo primero (little-endian Modbus)
    buffer[7] = crc >> 8;

    _serial->write(buffer, 8);

    // FIX: Espera con timeout y yield() (no delay() ciego que puede trabar el watchdog)
    unsigned long startTime = millis();
    while (_serial->available() < 8) {
        if (millis() - startTime > 500) return false; // Timeout 500ms
        yield(); // Alimenta el watchdog y el stack WiFi del ESP8266/ESP32
    }

    uint8_t response[8];
    for (int i = 0; i < 8; i++) response[i] = _serial->read();

    // FIX: Validar CRC de la respuesta
    uint16_t receivedCRC   = response[6] | ((uint16_t)response[7] << 8);
    uint16_t calculatedCRC = calculateCRC16(response, 6);
    if (receivedCRC != calculatedCRC) return false;

    // FC06 devuelve un eco idéntico al request
    if (response[0] != _device_addr || response[1] != 0x06) return false;

    delay(5); // FIX: Inter-frame gap Modbus RTU (~3.5 tiempos de carácter)
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Modbus: lectura de registro (FC03)
// ════════════════════════════════════════════════════════════════════════════

float LiquidLevelDetection::readModbusRegister(uint16_t reg_addr) {
    const int MAX_RETRIES = 3;

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        // FIX: Vaciar buffer RX en cada intento, no solo al inicio
        while (_serial->available() > 0) _serial->read();

        uint8_t buffer[8];
        buffer[0] = _device_addr;
        buffer[1] = 0x03;             // FC03: read holding register
        buffer[2] = reg_addr >> 8;
        buffer[3] = reg_addr & 0xFF;
        buffer[4] = 0x00;
        buffer[5] = 0x01;             // Leer 1 registro

        uint16_t crc = calculateCRC16(buffer, 6);
        buffer[6] = crc & 0xFF;
        buffer[7] = crc >> 8;

        _serial->write(buffer, 8);

        // Esperar respuesta de 7 bytes con timeout
        // Respuesta FC03 (1 registro): [addr][0x03][0x02][Hi][Lo][CRC_L][CRC_H]
        unsigned long startTime = millis();
        while (_serial->available() < 7) {
            if (millis() - startTime > 500) break; // Timeout → break al for
            yield();
        }

        if (_serial->available() < 7) {
            delay(50);
            continue; // Timeout, reintentar
        }

        uint8_t response[7];
        for (int i = 0; i < 7; i++) response[i] = _serial->read();

        // FIX: Validar CRC
        uint16_t receivedCRC   = response[5] | ((uint16_t)response[6] << 8);
        uint16_t calculatedCRC = calculateCRC16(response, 5);
        if (receivedCRC != calculatedCRC) {
            delay(50);
            continue; // Frame corrupto, reintentar
        }

        // Validar estructura del frame
        if (response[0] != _device_addr ||
            response[1] != 0x03         ||
            response[2] != 0x02) {        // 0x02 = 2 bytes de datos (1 registro)
            delay(50);
            continue;
        }

        uint16_t value = (response[3] << 8) | response[4];
        delay(5); // FIX: Inter-frame gap antes de la siguiente consulta
        return (float)value;
    }

    return -1.0; // Todos los reintentos fallaron
}

// ════════════════════════════════════════════════════════════════════════════
// CRC-16/IBM (polinomio 0xA001, estándar Modbus RTU)
// ════════════════════════════════════════════════════════════════════════════

uint16_t LiquidLevelDetection::calculateCRC16(uint8_t* buffer, uint8_t length) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else               { crc >>= 1; }
        }
    }
    return crc;
}