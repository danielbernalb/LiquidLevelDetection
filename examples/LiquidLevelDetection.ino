#include <LiquidLevelDetection.h>

// Define RX and TX pins (Valid for both ESP32 and ESP8266)
// Note for ESP8266: Pin 3 corresponds to GPIO3 (RXD0). If you encounter flashing or 
// serial terminal conflicts on ESP8266, consider using alternative pins like 13 and 12.
#define RX_PIN 2
#define TX_PIN 3

// Define baud rate
#define BAUD_RATE 115200

// Create sensor object
LiquidLevelDetection sensor(RX_PIN, TX_PIN);

void setup() {
    // Initialize serial communication with 115200 baud rate
    Serial.begin(BAUD_RATE);
    
    #if defined(ESP32)
    Serial.println("System Architecture: ESP32 (HardwareSerial active)");
    #elif defined(ESP8266)
    Serial.println("System Architecture: ESP8266 (SoftwareSerial active)");
    #endif

    Serial.println("Millimeter-wave Liquid Level Sensor Test");
    
    // Initialize sensor with 115200 baud rate
    if (!sensor.begin(BAUD_RATE)) {
        Serial.println("Sensor initialization failed!");
        while (1);
    }
    
    char buffer[10];  // Buffer for formatting float numbers
    
    // Set installation height (unit: centimeter)
    if (sensor.setInstallationHeight(200)) {  // 2 meters = 200 centimeters
        Serial.println("Installation height set successfully"); 
        // Wait a moment for the device to update the range
        delay(1000);
      
        // Read current range
        float range = sensor.getRange();
        Serial.print("Current range: ");
        if (range >= 0) {
            dtostrf(range, 1, 3, buffer);
            Serial.print(buffer);
            Serial.println(" m");
        } else {
            Serial.println("Communication Error");
        }
        Serial.println("------------------------");
    } else {
        Serial.println("Failed to set installation height");
    }
}

void loop() {
    char buffer[10];  // Buffer for formatting float numbers
    
    // Read installation height
    float installHeight = sensor.getInstallationHeight();
    Serial.print("Installation height: ");
    if (installHeight >= 0) {
        dtostrf(installHeight, 1, 3, buffer);
        Serial.print(buffer);
        Serial.println(" m");
    } else {
        Serial.println("Reading Error");
    }
    delay(5);
    
    // Read empty height (distance from sensor to liquid surface)
    float emptyHeight = sensor.getEmptyHeight();
    Serial.print("Empty height: ");
    if (emptyHeight >= 0) {
        dtostrf(emptyHeight, 1, 3, buffer);
        Serial.print(buffer);
        Serial.println(" m");
    } else {
        Serial.println("Reading Error");
    }
    delay(5);
    
    // Read water level height
    float waterLevel = sensor.getWaterLevel();
    Serial.print("Water level height: ");
    if (waterLevel >= 0) {
        dtostrf(waterLevel, 1, 3, buffer);
        Serial.print(buffer);
        Serial.println(" m");
    } else {
        Serial.println("Reading Error");
    }
    Serial.println("------------------------");
    delay(1000);  // Update every 3 seconds
}