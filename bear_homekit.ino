  #include <Arduino.h>
  #include <arduino_homekit_server.h>
  #include <Adafruit_NeoPixel.h>
  #include "wifi_info.h"

  #define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

#define NEOPIN          D2
#define NUMPIXELS       8

#define chargingPin     D1


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

  const int batteryPin = A0; 
  const float voltageDivider = 2.0;
  const float fullBatteryVoltage = 4.2;
  const float emptyBatteryVoltage = 3.0;
  const float calibration = 0.0; //Check battery voltage using multimeter

  float read_battery_level() {
      int analog_value = analogRead(batteryPin); // Чтение с аналогового пина (A0)
      LOG_D("analog_value: %d", analog_value);

      float voltage = (analog_value / 1023.0) * 3.3 * voltageDivider + calibration; // Примените делитель напряжения
      float battery_percentage = ((voltage - 3.0) / (fullBatteryVoltage - emptyBatteryVoltage)) * 100; // Преобразование в проценты
      if (battery_percentage < 0) battery_percentage = 1;
      if (battery_percentage > 100) battery_percentage = 100;
      LOG_D("battery_percentage: %f", battery_percentage);
      return battery_percentage;
  }



  void setup() {
    Serial.begin(115200);
    wifi_connect(); // in wifi_info.h

    pixels.begin(); 
    for(int i = 0; i < NUMPIXELS; i++)
    {
      pixels.setPixelColor(i, 0, 0, 0);
    }
    pixels.show();
    delay(1000);
    rgb_colors[0] = 255;
    rgb_colors[1] = 255;
    rgb_colors[2] = 255;

    pinMode(chargingPin, INPUT);

    my_homekit_setup();
  }

  void loop() {
      static unsigned long last_battery_update = 0; // Время последнего обновления батареи
      static unsigned long last_homekit_update = 0;  // Время последнего обновления HomeKit

      unsigned long current_time = millis(); // Получаем текущее время

      // Обновляем HomeKit каждую 10 мс
      if (current_time - last_homekit_update >= 10) {
          my_homekit_loop();
          last_homekit_update = current_time; // Обновляем время последнего обновления HomeKit
      }

      // Обновляем уровень заряда батареи каждые 10 секунд
      if (current_time - last_battery_update >= 10000) {
          update_battery_level();
          last_battery_update = current_time; // Обновляем время последнего обновления батареи
      }

  }

  //==============================
  // HomeKit setup and loop
  //==============================

  // access your HomeKit characteristics defined in my_accessory.c

  extern "C" homekit_server_config_t accessory_config;
  extern "C" homekit_characteristic_t cha_on;
  extern "C" homekit_characteristic_t cha_bright;
  extern "C" homekit_characteristic_t cha_sat;
  extern "C" homekit_characteristic_t cha_hue;
  extern "C" homekit_characteristic_t cha_battery_level;
  extern "C" homekit_characteristic_t cha_charging_state; 
  extern "C" homekit_characteristic_t cha_status_low_battery; 



  static uint32_t next_heap_millis = 0;

  void my_homekit_setup() {
    cha_on.setter = set_on;
    cha_bright.setter = set_bright;
    cha_sat.setter = set_sat;
    cha_hue.setter = set_hue;
    cha_battery_level.setter = NULL;
    cha_charging_state.setter = NULL; // добавлено
    cha_status_low_battery.setter = NULL; // добавлено

    arduino_homekit_setup(&accessory_config);

  }

  void update_battery_level() {
      cha_battery_level.value.float_value = read_battery_level();
      
       // Обновление состояния зарядки и низкого уровня батареи
      if (cha_battery_level.value.float_value < 20) { // Например, если уровень батареи меньше 20%
        cha_status_low_battery.value.int_value = 1;   // Низкий заряд
      } else {
        cha_status_low_battery.value.int_value = 0;   // Нормальный заряд
      }
      cha_charging_state.value.int_value = is_charging(); // Функция, которая определяет, заряжается ли устройство

      homekit_characteristic_notify(&cha_battery_level, cha_battery_level.value);
      homekit_characteristic_notify(&cha_status_low_battery, cha_status_low_battery.value);
      homekit_characteristic_notify(&cha_charging_state, cha_charging_state.value);
  }

  bool is_charging() {
   return digitalRead(chargingPin) == HIGH;
  }

  void my_homekit_loop() {
    arduino_homekit_loop();
    const uint32_t t = millis();
    if (t > next_heap_millis) {
      // show heap info every 5 seconds
      next_heap_millis = t + 5 * 1000;
      LOG_D("Free heap: %d, HomeKit clients: %d",
          ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

    }
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
      cha_hue.value.float_value = hue; //sync the value

      current_hue = hue;
      received_hue = true;
      
      updateColor();
  }

  void set_sat(const homekit_value_t v) {
      Serial.println("set_sat");
      float sat = v.float_value;
      cha_sat.value.float_value = sat; //sync the value

      current_sat = sat;
      received_sat = true;
      
      updateColor();

  }

  void set_bright(const homekit_value_t v) {
      Serial.println("set_bright");
      int bright = v.int_value;
      cha_bright.value.int_value = bright; //sync the value

      current_brightness = bright;

      updateColor();
  }

  void updateColor()
  {
    if(is_on)
    {
    
        if(received_hue && received_sat)
        {
          HSV2RGB(current_hue, current_sat, current_brightness);
          received_hue = false;
          received_sat = false;
        }
        
        int b = map(current_brightness,0, 100,75, 255);
        Serial.println(b);
    
        pixels.setBrightness(b);
        for(int i = 0; i < NUMPIXELS; i++)
        {
    
          pixels.setPixelColor(i, pixels.Color(rgb_colors[0],rgb_colors[1],rgb_colors[2]));
    
        }
        pixels.show();

      }
    else if(!is_on) //lamp - switch to off
    {
        Serial.println("is_on == false");
        pixels.setBrightness(0);
        for(int i = 0; i < NUMPIXELS; i++)
        {
          pixels.setPixelColor(i, pixels.Color(0,0,0));
        }
        pixels.show();
    }
  }

  void HSV2RGB(float h,float s,float v) {

    int i;
    float m, n, f;

    s/=100;
    v/=100;

    if(s==0){
      rgb_colors[0]=rgb_colors[1]=rgb_colors[2]=round(v*255);
      return;
    }

    h/=60;
    i=floor(h);
    f=h-i;

    if(!(i&1)){
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