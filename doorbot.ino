#include <LiquidCrystal.h>
#include <Button.h>
#include <TicksPerSecond.h>
#include <RotaryEncoderAcelleration.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 2, 3, 4, 5);

const int contrast_pin = 6;
const int backlight_pin = 9;
const int pir_pin = 15;
const int red_pin = 14;
const int green_pin = 16;
const int rotor_button_pin = A0;
const int rotor_a_pin = A1;
const int rotor_b_pin = A2;
const int relay_pin = A3;

Button pir;
Button red;
Button green;
Button rotor_button;

RotaryEncoderAcelleration rotor;

long rotor_old_pos = 0;
long rotor_new_pos = 0;

unsigned long relay_off_time = 0;

void setup() {

  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  
  // output pins with default values
  pinMode(contrast_pin, OUTPUT); analogWrite(contrast_pin, 10);
  pinMode(backlight_pin, OUTPUT); analogWrite(backlight_pin, 255);
  pinMode(relay_pin, OUTPUT); digitalWrite(relay_pin, LOW);
  
  // input pints with internal pull-ups
  pinMode(pir_pin, INPUT_PULLUP);
  pinMode(red_pin, INPUT_PULLUP);
  pinMode(green_pin, INPUT_PULLUP);
  pinMode(rotor_button_pin, INPUT_PULLUP);
  pinMode(rotor_b_pin, INPUT_PULLUP);
  pinMode(rotor_b_pin, INPUT_PULLUP);
  
  // buttons
  pir.initialize(pir_pin);
  red.initialize(red_pin);
  green.initialize(green_pin);
  rotor_button.initialize(rotor_button_pin);
  
  // rotory encoder
  rotor.initialize(rotor_a_pin, rotor_b_pin);
  rotor.setMinMax(-100, 100);
  rotor.setPosition(0);

  // initial LCD message
  lcd.setCursor(0, 0); lcd.print("****************");
  lcd.setCursor(0, 1); lcd.print("****************");
  delay(500);
  lcd.setCursor(0, 0); lcd.print("Hello!          ");
  lcd.setCursor(0, 1); lcd.print("                ");

  // set up serial and wait for it to become ready
  Serial.begin(115200);
  while (!Serial);
  Serial.println("READY");

}

void loop() {
  byte cmd;
  byte val;
  char buf[17];

  if (Serial.available()) {
    cmd = Serial.read();
    switch (cmd) {
      case 0x01:
        // Line 1
        Serial.readBytes(buf, 16);
        buf[16] = 0;
        lcd.setCursor(0, 0);
        lcd.print(buf);
        break;
      case 0x02:
        // Line 2
        Serial.readBytes(buf, 16);
        buf[16] = 0;
        lcd.setCursor(0, 1);
        lcd.print(buf);
        break;
      case 0x03:
        // Backlight
        val = Serial.read();
        analogWrite(backlight_pin, val);
        break;
      case 0x04:
        // Contrast
        val = Serial.read();
        analogWrite(contrast_pin, val);
        break;
      case 0x05:
        // Relay
        val = Serial.read();
        if (val==0) {
          digitalWrite(relay_pin, LOW);
          relay_off_time = 0;
        } else if (val==255) {
          digitalWrite(relay_pin, HIGH);
          relay_off_time = 0;
        } else {
          digitalWrite(relay_pin, HIGH);
          relay_off_time = millis()+(val*1000);
        }
    }
  }

  pir.update();
  if (pir.isPressed()) {
    Serial.println("PIR_ON");
  } else if (pir.isReleased()) {
    Serial.println("PIR_OFF");
  }

  red.update();
  if (red.isPressed()) {
    Serial.println("RED_DOWN");
  } else if (red.isReleased()) {
    Serial.println("RED_UP");
  }

  green.update();
  if (green.isPressed()) {
    Serial.println("GREEN_DOWN");
  } else if (green.isReleased()) {
    Serial.println("GREEN_UP");
  }
  
  rotor_button.update();
  if (rotor_button.isPressed()) {
    Serial.println("ROTOR_DOWN"); 
  } else if (rotor_button.isReleased()) {
    Serial.println("ROTOR_UP"); 
  }
  
  rotor.update();
  rotor_new_pos = rotor.getPosition();
  if (rotor_old_pos < rotor_new_pos) {
    Serial.print("-");
    Serial.println(rotor_new_pos-rotor_old_pos);
    rotor_old_pos = rotor_new_pos;
  } else if (rotor_old_pos > rotor_new_pos) {
    Serial.print("+");
    Serial.println(rotor_old_pos-rotor_new_pos);
    rotor_old_pos = rotor_new_pos;
  }
    
  if (rotor_new_pos >= 90 || rotor_new_pos < -90) {
    rotor.setPosition(0);
    rotor_old_pos = 0;
  }
  
  if (relay_off_time != 0) {
    if (millis() > relay_off_time) {
      digitalWrite(relay_pin, LOW);
      relay_off_time = 0;
    }
  }
  
}

