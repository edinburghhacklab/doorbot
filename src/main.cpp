#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Bounce2.h>
#include <Ticker.h>

extern "C" {
#include "user_interface.h"
}

#include "config.h"

const int sda_pin = 13;
const int scl_pin = 12;
const int red_button_pin = 4;
const int green_button_pin = 5;
const int buzzer_pin = 14;

char my_id[40];
char mqtt_id[24];
char lcd_message1[17];
char lcd_message2[17];
char lcd_flash1[17];
char lcd_flash2[17];

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient client;
PubSubClient mqtt(client);
Bounce red_button;
Bounce green_button;
Ticker buzzer_ticker;

unsigned long last_red_down = 0;
unsigned long last_green_down = 0;
bool red_is_long_pressed = false;
bool green_is_long_pressed = false;
unsigned long flash_expires = 0;
bool flash_active = false;

void set_message(String message, bool flash)
{
    String line1 = "";
    String line2 = "";
    int split_pos = message.indexOf("\n");
    if (split_pos >= 0) {
        line1 = message.substring(0, split_pos).substring(0, 16);
        line2 = message.substring(split_pos + 1).substring(0, 16);
    } else {
        line1 = message.substring(0, 16);
        line2 = message.substring(16, 32);
    }
    if (flash) {
        snprintf(lcd_flash1, sizeof(lcd_flash1), "%-16s", line1.c_str());
        snprintf(lcd_flash2, sizeof(lcd_flash2), "%-16s", line2.c_str());
        Serial.println("setting flash message: ");
        Serial.println(lcd_flash1);
        Serial.println(lcd_flash2);
        lcd.setCursor(0, 0); lcd.print(lcd_flash1);
        lcd.setCursor(0, 1); lcd.print(lcd_flash2);
        flash_expires = millis() + flash_time;
        flash_active = true;
    } else {
        snprintf(lcd_message1, sizeof(lcd_message1), "%-16s", line1.c_str());
        snprintf(lcd_message2, sizeof(lcd_message1), "%-16s", line2.c_str());
        Serial.print("setting normal message: ");
        Serial.println(lcd_message1);
        Serial.println(lcd_message2);
        lcd.setCursor(0, 0); lcd.print(lcd_message1);
        lcd.setCursor(0, 1); lcd.print(lcd_message2);
    }
}

void set_backlight(int level)
{
    if (level == 0) {
        Serial.println("backlight off");
        lcd.setBacklight(0);
    } else {
        Serial.println("backlight on");
        lcd.setBacklight(1);
    }
}

void buzzer_callback()
{
    pinMode(buzzer_pin, INPUT);
}

void set_buzzer(long ms)
{
    if (ms <= max_buzzer_time) {
        Serial.print("buzzing for ");
        Serial.print(ms, DEC);
        Serial.println("ms");
        buzzer_ticker.once_ms(ms, buzzer_callback);
    } else {
        Serial.print("buzzing for ");
        Serial.print(max_buzzer_time, DEC);
        Serial.println("ms");
        buzzer_ticker.once_ms(max_buzzer_time, buzzer_callback);
    }
    digitalWrite(buzzer_pin, LOW);
    pinMode(buzzer_pin, OUTPUT);
}

void mqtt_callback(const char *topic, byte *payload, unsigned int length)
{
    String ts = topic;
    String ps = (char*)payload;
    ps = ps.substring(0, length);

    if (ts.equals("display/doorbot/message")) {
        set_message(ps, false);
    } else if (ts.equals("display/doorbot/flash")) {
        set_message(ps, true);
    } else if (ts.equals("display/doorbot/backlight")) {
        set_backlight(ps.toInt());
    } else if (ts.equals("display/doorbot/buzzer")) {
        set_buzzer(ps.toInt());
    }
}

void setup() {
    snprintf(my_id, sizeof(my_id), "doorbot-%06x", ESP.getChipId());
    snprintf(mqtt_id, sizeof(mqtt_id), "%08x", ESP.getChipId());
    snprintf(lcd_message1, sizeof(lcd_message1), "%-16s", "");
    snprintf(lcd_message2, sizeof(lcd_message2), "%-16s", "");

    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.print(my_id);
    Serial.print(" ");
    Serial.println(ESP.getSketchMD5());

    red_button.attach(red_button_pin, INPUT_PULLUP);
    green_button.attach(green_button_pin, INPUT_PULLUP);

    Wire.begin(sda_pin, scl_pin);

    lcd.begin();
    lcd.clear();
    lcd.setBacklight(1);
    lcd.setCursor(0, 0);
    lcd.print(my_id);

    wifi_station_set_hostname(my_id);
    WiFi.mode(WIFI_STA);

    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(mqtt_callback);
}

void loop() {

    red_button.update();
    green_button.update();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("Connecting to ");
        Serial.print(wifi_ssid);
        Serial.println("...");
        lcd.clear();
        lcd.print("WiFi down");
        WiFi.begin(wifi_ssid, wifi_password);

        unsigned long begin_started = millis();
        while (WiFi.status() != WL_CONNECTED) {
            delay(10);
            if (millis() - begin_started > 60000) {
                ESP.restart();
            }
        }
        Serial.println("WiFi up");
        lcd.clear();
        lcd.print("WiFi up, MQTT down");
    }

    if (!mqtt.connected()) {
        if (mqtt.connect(mqtt_id, "display/doorbot/connected", 1, true, "0")) {
            Serial.println("MQTT up");
            mqtt.publish((const char*)"display/doorbot/connected", (const uint8_t*)"1", true);
            mqtt.subscribe("display/doorbot/message");
            mqtt.subscribe("display/doorbot/flash");
            mqtt.subscribe("display/doorbot/contrast");
            mqtt.subscribe("display/doorbot/backlight");
            mqtt.subscribe("display/doorbot/buzzer");
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print(lcd_message1);
            lcd.setCursor(0, 1); lcd.print(lcd_message2);
        } else {
            Serial.println("MQTT connection failed");
            lcd.clear();
            lcd.print("WiFi up, MQTT down");
            delay(2000);
            return;
        }
    }

    mqtt.loop();

    if (red_button.rose()) {
        Serial.println("red up");
        mqtt.publish("display/doorbot/buttons/red/state", "up");
        if (millis() - last_red_down < long_press_time) {
            Serial.println("red shortpress");
            mqtt.publish("display/doorbot/buttons/red/shortpress", "");
        }
        red_is_long_pressed = false;
    } else if (red_button.fell()) {
        last_red_down = millis();
        Serial.println("red down");
        mqtt.publish("display/doorbot/buttons/red/state", "down");
    }

    if (green_button.rose()) {
        Serial.println("green up");
        mqtt.publish("display/doorbot/buttons/green/state", "up");
        if (millis() - last_green_down < long_press_time) {
            Serial.println("green shortpress");
            mqtt.publish("display/doorbot/buttons/green/shortpress", "");
        }
        green_is_long_pressed = false;
    } else if (green_button.fell()) {
        last_green_down = millis();
        Serial.println("green down");
        mqtt.publish("display/doorbot/buttons/green/state", "down");
    }

    if ((!red_is_long_pressed) && red_button.read() == LOW && red_button.duration() > long_press_time) {
        red_is_long_pressed = true;
        Serial.println("red longpress");
        mqtt.publish("display/doorbot/buttons/red/longpress", "");
    }

    if ((!green_is_long_pressed) && green_button.read() == LOW && green_button.duration() > long_press_time) {
        green_is_long_pressed = true;
        Serial.println("green longpress");
        mqtt.publish("display/doorbot/buttons/green/longpress", "");
    }

    if (flash_active && millis() > flash_expires) {
        Serial.println("clearing flash message");
        lcd.setCursor(0, 0); lcd.print(lcd_message1);
        lcd.setCursor(0, 1); lcd.print(lcd_message2);
        flash_active = false;
    }

}