/***************************************************
  This is the Arduino 101 code for Grovey sLocks
  This software is written by Gerrikoio.
  MIT license applies, see copy of license text in github folder

  This example uses the curieBLE device and the code used
  is an extract from CurieEddystoneURL.ino, which is 
   * Copyright (c) 2016 Bradford Needham, North Plains, Oregon, USA
   * @bneedhamia, https://www.needhamia.com
   * and is Licensed under the Apache 2.0 License, a copy of which
   * is included in the following github repo:
   * https://github.com/bneedhamia/CurieEddystoneURL
  Thanks goes to bndeedhamia for saving me a lot of time
  
  The rgb_lcd.h library used for the RGB LCD is
  2013 Copyright (c) Seeed Technology Inc.  All right reserved.
  It is provided under the MIT License (MIT). Details can be found here:
  https://github.com/Seeed-Studio/Grove_LCD_RGB_Backlight
 ****************************************************/

// **** INCLUDES *****
#include "CurieTimerOne.h"
#include <CurieBLE.h>
#include <EEPROM.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <SoftwareSerial.h>

const byte
  PIN_RELAY1 =                          5,              // Relay to actuate the night light
  PIN_BUZZ =                            6,              // Door buzzer
  PIN_BTN2 =                            7,              // Indoor button (to exit)
  PIN_RELAY2 =                          8,              // Relay to actuate the electric door strike
  PIN_BLINK =                           13,             // BLE status LED indicator
  PIN_BTN1 =                            16,             // Outdoor button (to enter)
  PIN_STATUSLED =                       17,             // Status LED indicator when button 1 pressed
  PIN_ANGLE =                           A0,             // Rotary Angle Sensor
  PIN_LUX =                             A1,             // Light Level Sensor
  MIN_LIGHTLEVEL =                      10;             // The min light level for night light activation

const char* 
  MY_URL =                              "https://--- ENTER YOUR URL HERE ---";

const uint8_t 
  MAX_URL_FRAME_LENGTH =                1 + 1 + 1 + 17,
  FRAME_TYPE_EDDYSTONE_URL =            0x10,           // Eddystone-URL frame type
  URL_PREFIX_HTTP_WWW_DOT =             0x00,           // 0x00 = http://www.
  URL_PREFIX_HTTPS_WWW_DOT =            0x01,           // 0x01 = https://www.
  URL_PREFIX_HTTP_COLON_SLASH_SLASH =   0x02,           // 0x02 = http://
  URL_PREFIX_HTTPS_COLON_SLASH_SLASH =  0x03;           // 0x03 = https://

const int8_t
  MIN_EEPADDR =                         32,             // EEPROM address for calibrated min angular sensor value (arbitrary address used)
  MAX_EEPADDR =                         36,             // EEPROM address for calibrated max angular sensor value (arbitrary address used - make sure no overlap though!)
  TX_POWER_DBM =                        (-70 + 41),
  colorR =                              255,
  colorG =                              255,
  colorB =                              255;

const unsigned int
  CALIBTIME =                           20000,          // Calibration time period
  TIMERCYCLETRIGGER =                   30000,          // LED Timer Reset + Send "ping" to cloud + handle button 1 timeout
  NIGHTLIGHTTIMER =                     48000,          // The timer for the night light (48 seconds)
  RELAYTIMER =                          7000,           // Relay activated time (7 seconds)
  DOORMOTION_INTERVAL =                 500,            // every half a second monitor door motion when required
  BTN_INTERVAL =                        200;            // every 200ms measure btn press

const unsigned long
  TIMERUSECS =                          125000;         // The timer cycle time in microseconds (used for status LED flashes)

unsigned long 
  t_now =                               0L,             // Ping cycle timer
  t_light =                             0L,             // Night Light timer
  t_relay =                             0L,             // Relay timer
  t_door =                              0L,             // Door timer to help monitor door movement
  t_btn =                               0L,             // Button timer
  t_btn1 =                              0L,             // Button 1 timer (handles the timeout scenario)
  t_buzz =                              0L;             // Buzzer timer

int 
  AngleVal =                            0,             // rotary angle sensor value
  LuxVal =                              0,             // light sensor value
  prevAngleVal =                        0,             // previous value
  calAngleMin =                         1023,          // minimum sensor value for calibration
  calAngleMax =                         0;             // maximum sensor value for calibration

uint8_t 
  urlFrame[MAX_URL_FRAME_LENGTH],
  urlFrameLength;

byte
  Btn1Cntr =                            0,             // A button counter that controls when to timeout the button 1 is in active status
  prevBtn1Read =                        0,
  newBtn1Read =                         0,
  prevBtn2Read =                        0,
  newBtn2Read =                         0,
  doorAngle =                           1,              // A door angle grade (1=closed, 2=ajar, 3=open, 4=wide_open)
  doorMovmnt =                          0;              // Door movement (11=opening, 12=closing)

bool
  NightLightReady =                     false,        // A flag to determine if night light can be activated
  NightLightOn =                        false,        // A flag to determine if the night light is on
  Booking =                             false,        // A flag to indicate that the booking name is being sent
  RELAYon =                             false,        // A flag to indicate whether the door strike relay is activated or not
  canMONITORdoor =                      false,        // Flag is true if there are calibrated min and max values
  MONITORdoor =                         false,        // A flag to indicate whether to monitor door movement or not
  BUZZon =                              false,        // A flag to indicate that the buzzer is triggered
  BUZZtoggle =                          false,        // The buzzer beep toggle
  LEDtoggle =                           LOW,          // The LED status toggle
  BTN1press =                           false, 
  BTN2press =                           false,
  setupSucceeded =                      false;

BLEService eddyService("FEAA");
BLEPeripheral ble;                                    // Root of our BLE Peripheral (server) capability

rgb_lcd lcd;

SoftwareSerial impSerial(4, 3);                       // RX, TX

void setup() {
  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  pinMode(PIN_BLINK, OUTPUT);
  pinMode(PIN_STATUSLED, OUTPUT);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT);
  pinMode(PIN_ANGLE, INPUT);
  pinMode(PIN_LUX, INPUT);

  Serial.begin(9600);
  //while (!Serial);
  delay(100);
  
  impSerial.begin(9600);
  while (!impSerial);

  // Check to see if door calibration is needed
  // We need to have min and max values -- we store them in EEPROM
  calAngleMin = EEPROMReadInt(MIN_EEPADDR);
  calAngleMax = EEPROMReadInt(MAX_EEPADDR);

  Serial.print(F("MIN Angle Sensor Value: ")); Serial.println(calAngleMin);
  Serial.print(F("MAX Angle Sensor Value: ")); Serial.println(calAngleMax);

  if (!calAngleMax || abs(calAngleMin - calAngleMax) < 3) {
    // There was no values stored in EEPROM hence default to calibration mode
    Serial.println(F("Entering Calibration Mode"));
    calibrateDoorAngleSensor();
  }
  else {
    canMONITORdoor = true;

    //read angle sensor vals a couple of times to ensure good readings from analog pin 
    for (int i = 0; i < 3; i++) {
      AngleVal = analogRead(PIN_ANGLE);
      delay(2);
    }
  }
  delay(100);
  
  // read the LUX sensor reading ac ouple of times to ensure good readings from analog pin
  for (int i = 0; i < 3; i++) {
    LuxVal = analogRead(PIN_LUX);
    delay(2);
  }
  delay(100);
  // now take an initial reading
  LuxVal = analogRead(PIN_LUX);
  delay(30);                           // Response Time is 20 to 30 ms so lets wait to get an average
  LuxVal += analogRead(PIN_LUX);
  LuxVal /= 2;
  Serial.print(F("Initial Light Reading: ")); Serial.println(LuxVal);
  
  if (LuxVal < MIN_LIGHTLEVEL) NightLightReady = true;
  else NightLightReady = false;
  
  // Check for manual indoor button override to allow user to recalibrate the door sensor
  byte btncntr = 0;
  for (byte i = 0; i < 20; i++) {
    if (digitalRead(PIN_BTN2)) btncntr++;
    delay(100);
  }
  if (btncntr > 10) calibrateDoorAngleSensor();
  // ======= end of door calibration stuff

  // configure the BLE Eddystone Beacon information
  setupSucceeded = false;
  digitalWrite(PIN_BLINK, HIGH);

  if (!initEddystoneUrlFrame(TX_POWER_DBM, MY_URL)) {
    return; // don't start advertising if the URL won't work.
  }
    
  ble.setAdvertisedServiceUuid(eddyService.uuid());
  ble.setAdvertisedServiceData(eddyService.uuid(), urlFrame, urlFrameLength);
  ble.begin();
  setupSucceeded = true;
  digitalWrite(PIN_BLINK, LOW);
  // ======= end of BLE beacon stuff
  Serial.println("");
  Serial.println(F("+++++++++++++++++++++++++++++"));
  Serial.println(F("Grovey_Slocks now running..."));
  Serial.println(F("+++++++++++++++++++++++++++++"));
  Serial.flush();

  lcdGreeting();
  delay(3000);
  lcdEnd();
   
  impSerial.write(0x30);                          // Startup code sent to wifi module
  impSerial.print('\n');
  impSerial.flush();
  
  t_now = millis();

}

void loop() {

  // If BLE setup() failed, do nothing
  if (!setupSucceeded) {
    delay(1);
    return;
  }

  // Check for messages from serial port
  if (impSerial.available()) {
    if (BTN1press) CurieTimerOne.pause();
    if (Booking) {
      Booking = false;
      char BookData[16] = {""};
      byte kk = 0;
      for (byte j = 0; j < 18; j++) {
        byte newChar = impSerial.read();
        if (newChar == '\n') break;
        else {
          if (kk < 16) {
            if (isAlphaNumeric(newChar)) {
              BookData[kk] = char(newChar);
              kk++;
            }
          }
        }
        delay(1);             // add short delay to ensure serial buffer full
      }
      Serial.print(F("Name Recvd: ")); Serial.println(BookData);
      if (BTN1press) lcdBooking(BookData);
    }
    else {
      char newChar = char(impSerial.read());
      Serial.print(F("Received: ")); Serial.println(newChar);
      if (newChar == 0x41) {                          // Door unlock request
        if (BTN1press && !t_relay) relayActivate();
      }
      else if (newChar == 0x42) {                          // Night Light request
        if (NightLightReady && !NightLightOn && !RELAYon && !BUZZon) {
          digitalWrite(PIN_RELAY1, HIGH);
          NightLightOn = true;
          t_light = millis();
        }
      }
      else if (newChar == 0x43) {                          // Expect the Booking Name from database
        Booking = true;
      }
    }
    Serial.flush();
    if (BTN1press) CurieTimerOne.resume();
  }
  
  // Ping timer for monitoring that "all is ok" - sends msg to cloud server
  if ((millis() - t_now) > TIMERCYCLETRIGGER) {
    impSerial.write(0x30);
    impSerial.print('\n');
    impSerial.flush();
    // read the LUX sensor reading ac ouple of times to ensure good readings from analog pin
    for (int i = 0; i < 3; i++) {
      LuxVal = analogRead(PIN_LUX);
      delay(2);
    }
    delay(100);
    // now take an initial reading
    LuxVal = analogRead(PIN_LUX);
    delay(30);                           // Response Time is 20 to 30 ms so lets wait to get an average
    LuxVal += analogRead(PIN_LUX);
    LuxVal /= 2;
    Serial.print(F("L: ")); Serial.println(LuxVal);
    Serial.flush();
    if (LuxVal < MIN_LIGHTLEVEL) NightLightReady = true;
    else NightLightReady = false;
    t_now = millis();
  }
  else if ((millis() - t_now) < 0) t_now = millis();      // reset timer

  // Check the Door Buttons
  if (!BTN2press) {
    // No buttons have been pressed
    if ((millis() - t_btn) > BTN_INTERVAL) {
      
      newBtn2Read = digitalRead(PIN_BTN2);
      if (newBtn2Read && (newBtn2Read == prevBtn2Read)) {
        impSerial.write(0x32);                          // Send info to WiFi module
        impSerial.print('\n');
        impSerial.flush();
        delay(100);
        Serial.println("B2");
        BTN2press = true;
      }
      prevBtn2Read = newBtn2Read;
      
      if (!BTN1press) {
        newBtn1Read = !digitalRead(PIN_BTN1);
        if (newBtn1Read && (newBtn1Read == prevBtn1Read)) {
          BTN1press = true;
          impSerial.write(0x31);                          // Send info to WiFi module
          impSerial.print('\n');
          impSerial.flush();
          Serial.println("B1");
          t_btn1 = millis();
          // start the timer for led flashes
          CurieTimerOne.start(TIMERUSECS, &timerHandler);
        }
        
        prevBtn1Read = newBtn1Read;
      }
      t_btn = millis();
    }
    else if ((millis() - t_btn) < 0) t_btn = millis();      // reset timer
  }
  // A button has been pressed
  if (BTN2press) {
    // The indoor button takes precedence over the outdoor button
    if (BTN1press) {
      BTN1press = false;                    // Timeout the outside button
      Btn1Cntr = 0;                         // Reset the timer counter
      Serial.println(F("BTN1 Override"));       // Send info to debug
      Serial.flush();
      LEDtoggle = LOW;
      digitalWrite(PIN_STATUSLED, LEDtoggle);
      CurieTimerOne.kill();
      lcdEnd();
    }
    // Check to see if relay needs to activate
    // First check doormotion status
    if (canMONITORdoor) {
      if (!MONITORdoor && !t_relay) relayActivate();
      else {
        // The Door is open. We will use button 2 to stop the buzzer and allow door to remain open longer
        if (!t_relay && doorAngle>1) {
          // D still open - BTN2 override
          // APPLY MORE CODE HERE TO HANDLE THIS SCENARIO (NOT FULLY IMPLEMENTED IN DEMO)
          if (BUZZon) {
            BUZZon = false;
            t_buzz = 0;
            lcdEnd();
          }
        }
      }
    }
    else {
      // No door motion monitoring so just check door timer status
      if (!t_relay) relayActivate();        
    }
  }
  else {
    if (BTN1press) {
      if ((millis() - t_btn1) > TIMERCYCLETRIGGER) {
        CurieTimerOne.restart(TIMERUSECS);
        if (Btn1Cntr) {                         // Wait approx 40 seconds before timeout.
          BTN1press = false;                    // Timeout the outside button
          Btn1Cntr = 0;                         // Reset the timer counter
          impSerial.write(0x33);                          // Send info to WiFi module
          impSerial.print('\n');
          impSerial.flush();
          Serial.println(F("BTN1 Timeout"));       // Send info to debug
          Serial.flush();
          LEDtoggle = LOW;
          digitalWrite(PIN_STATUSLED, LEDtoggle);
          CurieTimerOne.kill();
          lcdEnd();
        }
        else Btn1Cntr++;
        t_btn1 = millis();
      }
      else if ((millis() - t_btn1) < 0) t_btn1 = millis();
    }
  }
  if (t_relay) {
    if ((millis() - t_relay) > RELAYTIMER) relayEndHandler();
    else if ((millis() - t_relay) < 0) t_relay = millis();
  }

  // Door Movement Monitoring
  if (canMONITORdoor && MONITORdoor) {
    if ((millis() - t_door) > DOORMOTION_INTERVAL) {
      // Monitor door motion
      prevAngleVal = AngleVal;
      getDoorAngleValue();
      //check value and compare againt previous value
      if (!AngleVal && !prevAngleVal) {
        // door closed
        Serial.println(F("D closed"));
        doorAngle = 1;               // A door angle grade (1=closed, 2=ajar, 3=open, 4=wide_open)
        doorMovmnt = 0;              // Door movement (1=opening, 2=closing)
        impSerial.write(doorAngle);                          // Send info to WiFi module
        impSerial.print('\n');
        impSerial.flush();
        delay(10);
        
        // If Relay no longer activated we can stop monitoring door movement
        if (!RELAYon) {
          MONITORdoor = false;
          if (BTN1press) BTN1press = false;
          if (BTN2press) BTN2press = false;
          LEDtoggle = LOW;
          digitalWrite(PIN_STATUSLED, LEDtoggle);
          CurieTimerOne.kill();
          lcdEnd();
          BUZZon = false;
          t_buzz = 0;
        }

      }
      else {
        if (abs(AngleVal - prevAngleVal) < 2) {
          if (!RELAYon) {
            impSerial.write(0x22);                          // Send info to WiFi module - Buzzer ON
            impSerial.print('\n');
            impSerial.flush();
            Serial.println(F("D still open"));
            if (!BUZZon) lcdDoorClose();
            BUZZon = true;
            
          }
          else {
            if (AngleVal < 15) {
              Serial.println(F("D ajar"));
              doorAngle = 2;              // A door angle grade (1=closed, 2=ajar, 3=open, 4=wide_open)
              doorMovmnt = 0;              // Door movement (11=opening, 12=closing)
            }
            else if (AngleVal > 90) {
              Serial.println(F("D wide open"));
              doorAngle = 4;              // A door angle grade (1=closed, 2=ajar, 3=open, 4=wide_open)
              doorMovmnt = 0;              // Door movement (11=opening, 12=closing)
            }
            else {
              Serial.println(F("D open"));
              doorAngle = 3;              // A door angle grade (1=closed, 2=ajar, 3=open, 4=wide_open)
              doorMovmnt = 0;              // Door movement (11=opening, 12=closing)
            }
            impSerial.write(doorAngle);                          // Send info to WiFi module
            impSerial.print('\n');
            impSerial.flush();
            delay(10);
          }
        }
         Serial.flush();
      }
      t_door = millis();
    }
    else if ((millis() - t_door) < 0) t_door = millis();
  }

  // Door Buzzer Handler
  if (BUZZon) {
    if (!t_buzz) t_buzz = millis();
    if ((millis() - t_buzz) > DOORMOTION_INTERVAL) {
      if (BUZZtoggle) noTone(PIN_BUZZ);
      else tone(PIN_BUZZ, 523, 300);
      BUZZtoggle = !BUZZtoggle;
      t_buzz = millis();
    }
    else if ((millis() - t_buzz) < 0) t_buzz = millis();
  }

  // Night Light
  if (NightLightOn) {
    if ((millis() - t_light) > NIGHTLIGHTTIMER) {
      digitalWrite(PIN_RELAY1, LOW);
      NightLightOn = false;
      t_light = 0;
    }
    else if ((millis() - t_light) < 0) t_light = millis();
  }
}

// ++++++++++++++++++++++++++
// 101 Timer Handlers
// ++++++++++++++++++++++++++

void timerHandler()   // callback function when interrupt is asserted
{
  if (!(CurieTimerOne.readTickCount()%20)) {
    if (!LEDtoggle) {
      LEDtoggle = !LEDtoggle;
      digitalWrite(PIN_STATUSLED, LEDtoggle);
    }
  }
  else {
    if (LEDtoggle) {
      LEDtoggle = !LEDtoggle;
      digitalWrite(PIN_STATUSLED, LEDtoggle);
    }
  }
}

// ++++++++++++++++++++++++++++++++++++++++++
// EEPROM integer read and write functions
// ++++++++++++++++++++++++++++++++++++++++++
// source: Arduino Forum - Topic: Implementation of an eeprom integer read / write

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address)
{
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);
  
  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
void EEPROMWriteInt(int p_address, int p_value)
{
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);
  
  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

// +++++++++++++++++++++++++++++++
// Door Motion / Relay Routines
// +++++++++++++++++++++++++++++++

void calibrateDoorAngleSensor()
{
  // reset min and max values to the default values
  calAngleMin = 1023;
  calAngleMax = 0;
  
  lcdCalibration();       // Display a message on the LCD screen for 3 seconds
  delay(3000);
  lcdEnd ();
  
  //read angle sensor vals a couple of times to ensure good readings from analog pin
  for (int i = 0; i < 4; i++) {
    AngleVal = analogRead(PIN_ANGLE);
    delay(2);
  }
  prevAngleVal = AngleVal;                // Set an initial prev value = current value;
  
  // Buzzer to indicate that calibration has started
  tone(PIN_BUZZ, 523, 500);
  delay(200);
  noTone(PIN_BUZZ);
  delay(300);

  t_now = millis();
  while (1) {
    AngleVal = analogRead(PIN_ANGLE);
    
    if (AngleVal > calAngleMax) calAngleMax = AngleVal;
    if (AngleVal < calAngleMin) calAngleMin = AngleVal;
    
    delay(20);
    
    if ((millis() - t_now) > CALIBTIME) break;
  }

  // Buzzer tone to indicate that calibration has finished
  for (byte i = 0; i < 3; i++) {
    tone(PIN_BUZZ, 523, 500);
    delay(200);
    noTone(PIN_BUZZ);
    delay(300);
  }

  // Check that we in fact got acceptable readings - if so then store in EEPROM
  Serial.println(F("Door Calibration is now over"));
  Serial.print("Min val: "); Serial.print(calAngleMin);
  Serial.print(" | Max val: "); Serial.println(calAngleMax);
  Serial.println(F("+++++++++++++++++++++++++++++++++++"));
  Serial.println("");
  Serial.flush();
  delay(1000);

  if (calAngleMax > calAngleMin && (calAngleMax - calAngleMin) > 100) {
    // There was no values stored in EEPROM hence default to calibration mode
    Serial.println(F("Storing Calibration Values"));
    EEPROMWriteInt(MIN_EEPADDR, calAngleMin);
    EEPROMWriteInt(MAX_EEPADDR, calAngleMax);
    canMONITORdoor = true;
  }
  else {
    // Can make another request to user to calibrate // or it prevents door movement from being used
    canMONITORdoor = false;
  }
}

void getDoorAngleValue() 
{
  // sample the angle sensor:
  AngleVal = 0;
  for (int i = 0; i < 3; i++) {
    AngleVal += analogRead(PIN_ANGLE);
    delay(2);
  }
  AngleVal /= 3;
  // apply the calibration to the sensor reading - use a scale of 1000 to get more sensitivity
  AngleVal = map(AngleVal, calAngleMin, calAngleMax-1, 0, 100);
  AngleVal = 100 - AngleVal;                                 // In our case the door rotation is counter clockwise (will need to factor in a clockwise option)

  // in case the sensor value is outside the range seen during calibration
  AngleVal = constrain(AngleVal, 0, 100);

}

// ++++++++++++++++++++++++++
// Relay Handlers
// ++++++++++++++++++++++++++

void relayActivate()
{
    digitalWrite(PIN_RELAY2, HIGH);
    impSerial.write(0x20);                          // Send info to WiFi module
    impSerial.print('\n');
    impSerial.flush();
    Serial.println("Relay on");
    lcdDoorOpen();
    t_relay = millis();
    t_door = t_relay;
    RELAYon = true;
    MONITORdoor = true;
    LEDtoggle = LOW;
    digitalWrite(PIN_STATUSLED, LEDtoggle);
    CurieTimerOne.kill();
}

void relayEndHandler()
{
  BTN1press = false;
  BTN2press = false;
  lcdEnd();
  t_relay = 0;
  Btn1Cntr = 0;
  digitalWrite(PIN_RELAY2, LOW);
  impSerial.write(0x21);                          // Send info to WiFi module
  impSerial.print('\n');
  impSerial.flush();
  Serial.println("Relay off");
  RELAYon = false;
}

// ++++++++++++++++++++++++++
// LCD Display Routines
// ++++++++++++++++++++++++++

void lcdCalibration()
{
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(colorR, 0, 0);
  lcd.setCursor(0, 0);
  lcd.print(" Grovey_Slocks: ");
  lcd.setCursor(0, 1);
  lcd.print("Door Calibration");
}

void lcdGreeting()
{
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, colorB);
  lcd.setCursor(0, 0);
  lcd.print(" Grovey_Slocks: ");
  lcd.setCursor(0, 1);
  lcd.print(" Access Control ");
}

void lcdBooking(char *Name)
{
  CurieTimerOne.pause();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(colorR, colorG, colorB);
  lcd.setCursor(0, 0);
  lcd.print(" Welcome to 101 ");
  lcd.setCursor(0, 1);
  lcd.print(Name);
  delay(100);
  CurieTimerOne.resume();
}

void lcdDoorOpen()
{
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(0, colorG, 0);
  lcd.setCursor(0, 0);
  lcd.print(" Door Unlocked  ");
  lcd.setCursor(0, 1);
  lcd.print("You can now open");
}

void lcdDoorClose()
{
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(colorR, 0, 0);
  lcd.setCursor(0, 0);
  lcd.print("Door still Open ");
  lcd.setCursor(0, 1);
  lcd.print("Please close it ");
}

void lcdEnd()
{
  lcd.setRGB(0, 0, 0);
  lcd.clear();
  lcd.noDisplay();
  delay(2);
}


// ++++++++++++++++++++++++++
// 101 BLE Beacon Handler
// ++++++++++++++++++++++++++

boolean initEddystoneUrlFrame(int8_t txPower, const char* url) 
{
  urlFrameLength = 0;

  // The frame starts with a type byte, then power byte.
  urlFrame[urlFrameLength++] = FRAME_TYPE_EDDYSTONE_URL;
  urlFrame[urlFrameLength++] = (uint8_t) txPower;

  if (url == 0 || url[0] == '\0') {
    return false;   // empty URL
  }

  const char *pNext = url;

  if (strncmp("http", pNext, 4) != 0) {
    return false;  // doesn't start with HTTP or HTTPS.
  }
  pNext += 4;
  
  bool isHttps = false; // that is, HTTP
  if (*pNext == 's') {
    pNext++;
    isHttps = true;
  }

  if (strncmp("://", pNext, 3) != 0) {
    return false; // malformed URL
  }
  pNext += 3;

  urlFrame[urlFrameLength] = URL_PREFIX_HTTP_COLON_SLASH_SLASH;
  if (isHttps) {
    urlFrame[urlFrameLength] = URL_PREFIX_HTTPS_COLON_SLASH_SLASH;
  }

  if (strncmp("www.", pNext, 4) == 0) {
    pNext += 4;

    urlFrame[urlFrameLength] = URL_PREFIX_HTTP_WWW_DOT;
    if (isHttps) {
      urlFrame[urlFrameLength] = URL_PREFIX_HTTPS_WWW_DOT;
    }
  }
  
  urlFrameLength++;

  // Encode the URL.

  while (urlFrameLength < MAX_URL_FRAME_LENGTH && *pNext != '\0') {
    if (strncmp(".com/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x00;
    } else if (strncmp(".org/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x01;
    } else if (strncmp(".edu/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x02;
    } else if (strncmp(".net/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x03;
    } else if (strncmp(".info/", pNext, 6) == 0) {
      pNext += 6;
      urlFrame[urlFrameLength++] = 0x04;
    } else if (strncmp(".biz/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x05;
    } else if (strncmp(".gov/", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x06;
    } else if (strncmp(".com", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x07;
    } else if (strncmp(".org", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x08;
    } else if (strncmp(".edu", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x09;
    } else if (strncmp(".net", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x0A;
    } else if (strncmp(".info", pNext, 5) == 0) {
      pNext += 5;
      urlFrame[urlFrameLength++] = 0x0B;
    } else if (strncmp(".biz", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x0C;
    } else if (strncmp(".gov", pNext, 4) == 0) {
      pNext += 4;
      urlFrame[urlFrameLength++] = 0x0D;
    } else {
      // It's not special.  Just copy the character
      urlFrame[urlFrameLength++] = (uint8_t) *pNext++;
    }
  }
  return true;
}

