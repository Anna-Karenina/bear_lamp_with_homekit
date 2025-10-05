#include <Arduino.h>
#include <arduino_homekit_server.h>
#include <Adafruit_NeoPixel.h>
#include "wifi_info.h"

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

#define NEOPIN          D2
#define NUMPIXELS       12
#define BATTERY_SAMPLES 5

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

bool received_sat = false;
bool received_hue = false;
bool is_on = false;
float current_brightness =  100.0;
float current_sat = 0.0;
float current_hue = 0.0;
int rgb_colors[3];


//==============================
// Battery setup 
//==============================
const int chargingPin = D1;
const int batteryPin = A0; 
const float voltageDivider = 2.0;
const float fullBatteryVoltage = 4.20;
const float emptyBatteryVoltage = 3.0;
const float calibration = 0.0; //Check battery voltage using multimeter

// Настройки для определения зарядки (возможно нужно изменить)
const bool CHARGING_PIN_ACTIVE_LOW = true; // true если зарядка = LOW, false если зарядка = HIGH

// Функция для калибровки батареи (вызывать при полной зарядке)
void calibrate_battery() {
  LOG_D("=== BATTERY CALIBRATION ===");
  LOG_D("Connect multimeter to battery terminals");
  LOG_D("Current readings:");
  
  for (int i = 0; i < 10; i++) {
    float raw = analogRead(batteryPin);
    float voltage = (raw / 1023.0) * 3.3 * voltageDivider + calibration;
    LOG_D("Sample %d: Raw=%.0f, Voltage=%.3fV", i+1, raw, voltage);
    delay(500);
  }
  LOG_D("=== END CALIBRATION ===");
}
float battery_samples[BATTERY_SAMPLES];
int battery_sample_index = 0;

float read_battery_level() {
  // Собираем несколько samples для усреднения
  battery_samples[battery_sample_index] = analogRead(batteryPin);
  battery_sample_index = (battery_sample_index + 1) % BATTERY_SAMPLES;
  
  // Усредняем показания
  float avg_reading = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    avg_reading += battery_samples[i];
  }
  avg_reading /= BATTERY_SAMPLES;
  
  float voltage = (avg_reading / 1023.0) * 3.3 * voltageDivider + calibration;
  
  // Линейная интерполяция для Li-ion батареи (более точная)
  float battery_percentage = 100.0 * (voltage - emptyBatteryVoltage) / 
                            (fullBatteryVoltage - emptyBatteryVoltage);
  
  battery_percentage = constrain(battery_percentage, 0, 100);
  
  // Отладочная информация
  LOG_D("Raw ADC: %.0f, Voltage: %.2fV, Battery: %.1f%%", avg_reading, voltage, battery_percentage);
  return battery_percentage;
}

void setup() {
  Serial.begin(115200);

  pinMode(chargingPin, INPUT);
  pinMode(batteryPin, INPUT);

  // Инициализируем массив батареи с задержками для стабилизации
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    battery_samples[i] = analogRead(batteryPin);
    if (i < BATTERY_SAMPLES - 1) {
      delay(10); // Небольшая задержка между чтениями
    }
  }

  pixels.begin(); 
  for(int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, 0, 0, 0);
  }
  pixels.show();
  delay(1000);

  rgb_colors[0] = 255;
  rgb_colors[1] = 255;
  rgb_colors[2] = 255;
  wifi_connect(); // in wifi_info.h

  my_homekit_setup();
}

void loop() {
  static unsigned long last_battery_update = 0;
  static unsigned long last_homekit_update = 0;
  static unsigned long last_heap_print = 0;
  unsigned long current_time = millis();

  check_wifi_connection();

  // Обновляем HomeKit каждые 100 мс (вместо 10 мс для снижения нагрузки)
  if (current_time - last_homekit_update >= 100) {
      my_homekit_loop();
      last_homekit_update = current_time;
  }

  // Обновляем уровень заряда батареи каждые 30 секунд (вместо 5)
  if (current_time - last_battery_update >= 30000) {
    refresh_battery_status();
    last_battery_update = current_time;
  }

  // Вывод информации о памяти каждые 60 секунд
  if (current_time - last_heap_print >= 60000) {
    LOG_D("Free heap: %d, HomeKit clients: %d",
        ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
    last_heap_print = current_time;
    
    // Автоперезагрузка при малом количестве памяти
    if (ESP.getFreeHeap() < 4000) {
      LOG_D("Low memory! Restarting...");
      delay(1000);
      ESP.restart();
    }
  }
}

//==============================
// HomeKit setup and loop
//==============================
// access your HomeKit characteristics defined in device.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_bright;
extern "C" homekit_characteristic_t cha_sat;
extern "C" homekit_characteristic_t cha_hue;
extern "C" homekit_characteristic_t cha_battery_level;
extern "C" homekit_characteristic_t cha_charging_state; 
extern "C" homekit_characteristic_t cha_status_low_battery; 


void my_homekit_setup() {
  cha_on.setter = set_on;
  cha_bright.setter = set_bright;
  cha_sat.setter = set_sat;
  cha_hue.setter = set_hue;
  cha_battery_level.setter = NULL;
  cha_charging_state.setter = NULL;
  cha_status_low_battery.setter = NULL;
  arduino_homekit_setup(&accessory_config);
}

void refresh_battery_status() {
  float level = read_battery_level();
  bool charging = is_charging();
  
  // Обновляем значения характеристик
  cha_battery_level.value.float_value = level;
  cha_charging_state.value.int_value = charging ? 1 : 0;
  
  // Обновление состояния низкого уровня батареи
  if (level < 15) { 
    cha_status_low_battery.value.int_value = 1;
  } else {
    cha_status_low_battery.value.int_value = 0;
  }
  
  // Детальная диагностика
  LOG_D("Battery Status Update - Level: %.1f%%, Charging: %s, Low Battery: %s", 
        level, charging ? "YES" : "NO", (level < 15) ? "YES" : "NO");
  
  // Если уровень критически низкий и не заряжается - уходим в глубокий сон
  if (level < 5 && !charging) {
    LOG_D("Critical battery! Going to deep sleep...");
    delay(1000);
    ESP.deepSleep(0);
  }

  // Уведомляем HomeKit о изменениях
  homekit_characteristic_notify(&cha_battery_level, cha_battery_level.value);
  homekit_characteristic_notify(&cha_status_low_battery, cha_status_low_battery.value);
  homekit_characteristic_notify(&cha_charging_state, cha_charging_state.value);
}

bool is_charging() {
  int charging_pin_state = digitalRead(chargingPin);
  bool charging;
  
  // Настраиваемая логика для определения зарядки
  if (CHARGING_PIN_ACTIVE_LOW) {
    charging = (charging_pin_state == LOW);
  } else {
    charging = (charging_pin_state == HIGH);
  }
  
  // Отладочная информация для диагностики
  static unsigned long last_debug = 0;
  if (millis() - last_debug > 10000) { // Каждые 10 секунд
    LOG_D("Charging pin (D1): %d, Active Low: %s, Charging: %s", 
          charging_pin_state, CHARGING_PIN_ACTIVE_LOW ? "YES" : "NO", charging ? "YES" : "NO");
    last_debug = millis();
  }
  
  return charging;
}

void my_homekit_loop() {
  arduino_homekit_loop();
}

void set_on(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on.value.bool_value = on; //sync the value
    if(on) {
        is_on = true;
        Serial.println("On");
    } else  {
        is_on = false;
        Serial.println("Off");
    }
    updateColor();
}

  void set_hue(const homekit_value_t v) {
      Serial.println("set_hue");
      float hue = v.float_value;
      
      // Валидация значения hue (0-360)
      if (hue < 0.0) hue = 0.0;
      if (hue > 360.0) hue = 360.0;
      
      cha_hue.value.float_value = hue; //sync the value
      current_hue = hue;
      received_hue = true;
      
      updateColor();
  }

  void set_sat(const homekit_value_t v) {
      Serial.println("set_sat");
      float sat = v.float_value;
      
      // Валидация значения saturation (0-100)
      if (sat < 0.0) sat = 0.0;
      if (sat > 100.0) sat = 100.0;
      
      cha_sat.value.float_value = sat; //sync the value
      current_sat = sat;
      received_sat = true;
      
      updateColor();
  }

  void set_bright(const homekit_value_t v) {
      Serial.println("set_bright");
      int bright = v.int_value;
      
      // Валидация значения brightness (0-100)
      if (bright < 0) bright = 0;
      if (bright > 100) bright = 100;
      
      cha_bright.value.int_value = bright; //sync the value
      current_brightness = bright;

      updateColor();
  }

void updateColor() {
  if(is_on) {
    // Обновляем RGB только если изменились hue или saturation
    if(received_hue || received_sat) {
      HSV2RGB(current_hue, current_sat, current_brightness);
      received_hue = false;
      received_sat = false;
    }
        
    int b = map(current_brightness, 0, 100, 75, 255);
    pixels.setBrightness(b);
    for(int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(rgb_colors[0], rgb_colors[1], rgb_colors[2]));
    }
    pixels.show();
  } else {
    // Выключаем лампу
    pixels.setBrightness(0);
    for(int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }
    pixels.show();
  }
}

void HSV2RGB(float h, float s, float v) {
    // Валидация входных параметров
    if (h < 0) h = 0;
    if (h >= 360) h = fmod(h, 360);
    if (s < 0) s = 0;
    if (s > 100) s = 100;
    if (v < 0) v = 0;
    if (v > 100) v = 100;

    int i;
    float m, n, f;

    s /= 100;
    v /= 100;

    if(s==0){
      rgb_colors[0]=rgb_colors[1]=rgb_colors[2]=round(v*255);
      return;
    }

    h/=60;
    i=floor(h);
    f=h-i;

    if(i&1){
      f=1-f;
    }

    m=v*(1-s);
    n=v*(1-s*f);

    switch (i) {
      case 0: case 6:
        rgb_colors[0]=round(v*255);
        rgb_colors[1]=round(n*255);
        rgb_colors[2]=round(m*255);
      break;

      case 1:
        rgb_colors[0]=round(n*255);
        rgb_colors[1]=round(v*255);
        rgb_colors[2]=round(m*255);
      break;

      case 2:
        rgb_colors[0]=round(m*255);
        rgb_colors[1]=round(v*255);
        rgb_colors[2]=round(n*255);
      break;

      case 3:
        rgb_colors[0]=round(m*255);
        rgb_colors[1]=round(n*255);
        rgb_colors[2]=round(v*255);
      break;

      case 4:
        rgb_colors[0]=round(n*255);
        rgb_colors[1]=round(m*255);
        rgb_colors[2]=round(v*255);
      break;

      case 5:
        rgb_colors[0]=round(v*255);
        rgb_colors[1]=round(m*255);
        rgb_colors[2]=round(n*255);
      break;
    }
}