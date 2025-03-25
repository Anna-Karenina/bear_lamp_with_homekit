#include <Arduino.h>
#include <arduino_homekit_server.h>
#include <Adafruit_NeoPixel.h>
#include "wifi_info.h"


#define LOG_D(fmt, ...) printf_P(PSTR(fmt "\n"), ##__VA_ARGS__)
#define NEOPIN D2
#define NUMPIXELS 12
#define CHARGING_PIN D1
#define BATTERY_PIN A0
#define BATTERY_UPDATE_INTERVAL 1000
#define HOMEKIT_UPDATE_INTERVAL 10
#define LOW_BATTERY_THRESHOLD 10

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_bright;
extern "C" homekit_characteristic_t cha_sat;
extern "C" homekit_characteristic_t cha_hue;
extern "C" homekit_characteristic_t cha_battery_level;
extern "C" homekit_characteristic_t cha_charging_state; 
extern "C" homekit_characteristic_t cha_status_low_battery; 


// LED State
struct {
    bool is_on = false;
    bool received_sat = false;
    bool received_hue = false;
    float brightness = 100.0f;
    float saturation = 0.0f;
    float hue = 0.0f;
    uint8_t rgb[3] = {255, 255, 255};
} ledState;

// Battery Configuration
const struct {
    const float voltageDivider = 2.0f;
    const float fullVoltage = 4.20f;
    const float emptyVoltage = 3.3f;
    const float calibration = 0.0f;
} batteryConfig;

// ====================== BATTERY FUNCTIONS ======================
float readBatteryLevel() {
    int analogValue = analogRead(BATTERY_PIN);
    float voltage = (analogValue / 1023.0f) * 3.3f * batteryConfig.voltageDivider + batteryConfig.calibration;
    float percentage = ((voltage - batteryConfig.emptyVoltage) / 
                      (batteryConfig.fullVoltage - batteryConfig.emptyVoltage)) * 100.0f;
    return constrain(percentage, 1.0f, 100.0f);
}

bool isCharging() {
    // With INPUT_PULLUP, LOW means charging, HIGH means not charging
    return digitalRead(CHARGING_PIN) == LOW;
}

void refreshBatteryStatus() {
    static bool lastChargingState = false;
    static float lastBatteryLevel = 0;
    
    // Check charging state
    bool currentChargingState = isCharging();
    if (currentChargingState != lastChargingState) {
        cha_charging_state.value.int_value = currentChargingState ? 1 : 0;
        homekit_characteristic_notify(&cha_charging_state, cha_charging_state.value);
        lastChargingState = currentChargingState;
        LOG_D("Charging state changed to: %s", currentChargingState ? "CHARGING" : "NOT CHARGING");
    }
    
    // Check battery level
    float currentLevel = readBatteryLevel();
    if (abs(currentLevel - lastBatteryLevel) > 1.0) { // Only update if significant change
        cha_battery_level.value.float_value = currentLevel;
        homekit_characteristic_notify(&cha_battery_level, cha_battery_level.value);
        lastBatteryLevel = currentLevel;
        
        // Update low battery status
        uint8_t lowBatt = (currentLevel < LOW_BATTERY_THRESHOLD) ? 1 : 0;
        if (cha_status_low_battery.value.int_value != lowBatt) {
            cha_status_low_battery.value.int_value = lowBatt;
            homekit_characteristic_notify(&cha_status_low_battery, cha_status_low_battery.value);
        }
    }
}

// ====================== LED CONTROL FUNCTIONS ======================
void hsvToRgb(float h, float s, float v, uint8_t* rgb) {
    s /= 100.0f;
    v /= 100.0f;
    
    if (s == 0) {
        rgb[0] = rgb[1] = rgb[2] = static_cast<uint8_t>(v * 255);
        return;
    }

    h /= 60.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    if (!(i & 1)) f = 1.0f - f;
    
    float m = v * (1.0f - s);
    float n = v * (1.0f - s * f);
    
    switch (i % 6) {
        case 0: case 6:
            rgb[0] = static_cast<uint8_t>(v * 255); 
            rgb[1] = static_cast<uint8_t>(n * 255); 
            rgb[2] = static_cast<uint8_t>(m * 255);
            break;
        case 1:
            rgb[0] = static_cast<uint8_t>(n * 255); 
            rgb[1] = static_cast<uint8_t>(v * 255); 
            rgb[2] = static_cast<uint8_t>(m * 255);
            break;
        case 2:
            rgb[0] = static_cast<uint8_t>(m * 255); 
            rgb[1] = static_cast<uint8_t>(v * 255); 
            rgb[2] = static_cast<uint8_t>(n * 255);
            break;
        case 3:
            rgb[0] = static_cast<uint8_t>(m * 255); 
            rgb[1] = static_cast<uint8_t>(n * 255); 
            rgb[2] = static_cast<uint8_t>(v * 255);
            break;
        case 4:
            rgb[0] = static_cast<uint8_t>(n * 255); 
            rgb[1] = static_cast<uint8_t>(m * 255); 
            rgb[2] = static_cast<uint8_t>(v * 255);
            break;
        case 5:
            rgb[0] = static_cast<uint8_t>(v * 255); 
            rgb[1] = static_cast<uint8_t>(m * 255); 
            rgb[2] = static_cast<uint8_t>(n * 255);
            break;
    }
}

void updateLedStrip() {
    if (ledState.is_on) {
        if (ledState.received_hue && ledState.received_sat) {
            hsvToRgb(ledState.hue, ledState.saturation, ledState.brightness, ledState.rgb);
            ledState.received_hue = ledState.received_sat = false;
        }
        
        uint8_t brightness = map(ledState.brightness, 0, 100, 75, 255);
        pixels.setBrightness(brightness);
        
        for (int i = 0; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, pixels.Color(ledState.rgb[0], ledState.rgb[1], ledState.rgb[2]));
        }
    } else {
        pixels.setBrightness(0);
        for (int i = 0; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, 0);
        }
    }
    pixels.show();
}

// ====================== HOMEKIT CALLBACKS ======================
void setOn(const homekit_value_t v) {
    ledState.is_on = v.bool_value;
    cha_on.value.bool_value = ledState.is_on;
    LOG_D("Power: %s", ledState.is_on ? "ON" : "OFF");
    updateLedStrip();
}

void setBrightness(const homekit_value_t v) {
    ledState.brightness = v.int_value;
    cha_bright.value.int_value = ledState.brightness;
    LOG_D("Brightness: %.0f%%", ledState.brightness);
    updateLedStrip();
}

void setSaturation(const homekit_value_t v) {
    ledState.saturation = v.float_value;
    cha_sat.value.float_value = ledState.saturation;
    ledState.received_sat = true;
    LOG_D("Saturation: %.0f%%", ledState.saturation);
    updateLedStrip();
}

void setHue(const homekit_value_t v) {
    ledState.hue = v.float_value;
    cha_hue.value.float_value = ledState.hue;
    ledState.received_hue = true;
    LOG_D("Hue: %.0f°", ledState.hue);
    updateLedStrip();
}

// ====================== MAIN FUNCTIONS ======================
void setup() {
    Serial.begin(115200);
    wifi_connect();
    
    // Initialize pins
    pinMode(CHARGING_PIN, INPUT_PULLUP);
    pinMode(BATTERY_PIN, INPUT);
    
    // Initialize LED strip
    pixels.begin();
    pixels.clear();
    pixels.show();
    
    // Setup HomeKit callbacks
    cha_on.setter = setOn;
    cha_bright.setter = setBrightness;
    cha_sat.setter = setSaturation;
    cha_hue.setter = setHue;
    
    arduino_homekit_setup(&accessory_config);
}

void loop() {
    static unsigned long lastBatteryUpdate = 0;
    static unsigned long lastHomeKitUpdate = 0;
    unsigned long currentTime = millis();
    
    // Handle HomeKit updates
    if (currentTime - lastHomeKitUpdate >= HOMEKIT_UPDATE_INTERVAL) {
        arduino_homekit_loop();
        lastHomeKitUpdate = currentTime;
    }
    
    // Handle battery updates
    if (currentTime - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
        refreshBatteryStatus();
        lastBatteryUpdate = currentTime;
        
        // Log heap status periodically
        LOG_D("Free heap: %d, Clients: %d", 
              ESP.getFreeHeap(), 
              arduino_homekit_connected_clients_count());
    }
}