#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void my_accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}

homekit_characteristic_t cha_on = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, "Bear Lamp");
homekit_characteristic_t cha_bright = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 50);
homekit_characteristic_t cha_sat = HOMEKIT_CHARACTERISTIC_(SATURATION, (float) 0);
homekit_characteristic_t cha_hue = HOMEKIT_CHARACTERISTIC_(HUE, (float) 180);
homekit_characteristic_t cha_battery_level = HOMEKIT_CHARACTERISTIC_(BATTERY_LEVEL, 100);
homekit_characteristic_t cha_charging_state = HOMEKIT_CHARACTERISTIC_(CHARGING_STATE, 0);
homekit_characteristic_t cha_status_low_battery = HOMEKIT_CHARACTERISTIC_(STATUS_LOW_BATTERY, 0);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Bear Lamp"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "AK::HomeKit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "11.09.1192"),
            HOMEKIT_CHARACTERISTIC(MODEL, "001"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.2"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            &cha_on,
            &cha_name,
            &cha_bright,
            &cha_sat,
            &cha_hue,
            NULL
        }),
        HOMEKIT_SERVICE(BATTERY_SERVICE, .characteristics=(homekit_characteristic_t*[]) {
            &cha_battery_level,  // Используйте характеристику уровня заряда, которую вы уже определили
            &cha_charging_state, // Добавляем характеристику состояния зарядки
            &cha_status_low_battery, // Добавляем характеристику низкого заряда
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t accessory_config = {
    .accessories = accessories,
    .password = "111-11-111"
};