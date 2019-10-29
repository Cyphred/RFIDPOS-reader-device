/*
[PINOUTS]
  RC522 - https://www.brainy-bits.com/card-reader-with-an-arduino-rfid-using-the-rc522-module/
    - 3.3V -> 3.3V
    - RST -> 8
    - GND -> GND
    - IRQ -> Nothing
    - SDA -> 10
    - MOSI -> 11
    - MISO -> 12
    - SCK -> 13

  I2C LCD - https://www.makerguides.com/character-i2c-lcd-arduino-tutorial/
    - VCC -> 5V
    - GND -> GND
    - SDA -> A4
    - SCL -> A5
*/

#include <Wire.h> // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD
#include <MFRC522.h> // RFID Module Library
#include <SPI.h> // Used for communication via SPI with the RFID Module

#define SDAPIN 10 // RFID Module SDA Pin connected to digital pin
#define RESETPIN 8 // RFID Module RESET Pin connected to digital pin

byte FoundTag; // value to tell if a tag is found
byte ReadTag; // Anti-collision value to read tag information
byte TagData[MAX_LEN]; // full tag data
byte TagSerialNumber[5]; // tag serial number
byte GoodTagSerialNumber[5] = {0x95, 0xEB, 0x17, 0x53}; // The Tag Serial number we are looking for

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library
MFRC522 nfc(SDAPIN, RESETPIN); // Initialization for RFID Reader with declared pinouts for SDA and RESET

int menuState = 1;

void setup() {
  SPI.begin();
  Serial.begin(115200);

  // Initiate the LCD:
  lcd.init();
  lcd.backlight();
  lcd.noCursor();
  
  // Start to find an RFID Module
  lcd.setCursor(0,0);
  lcd.print("Looking for RFID");
  lcd.setCursor(0,1);
  lcd.print("Reader");
  nfc.begin();
  byte version = nfc.getFirmwareVersion(); // Variable to store Firmware version of the Module
  
  // If can't find an RFID Module 
  if (! version) { 
    lcd.setCursor(0,0);
    lcd.print("RFID Reader not");
    lcd.setCursor(0,1);
    lcd.print("found");
  while(1); //Wait until a RFID Module is found
  }
  else {
    // If found, print the information about the RFID Module
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RC522 found!");
    lcd.setCursor(0,1);
    lcd.print("0x");
    lcd.setCursor(2,1);
    lcd.print(version, HEX);
  }
  delay(3000);
  lcd.clear();
}

void loop()
{
  if (menuState == 1) {
      // Check if a tag was detected
      // If yes, then variable FoundTag will contain "MI_OK"
      FoundTag = nfc.requestTag(MF1_REQIDL, TagData);

      if (FoundTag == MI_OK) {
          delay(200);

          // Get anti-collision value to properly read information from the tag
          ReadTag = nfc.antiCollision(TagData);
          memcpy(TagSerialNumber, TagData, 4); // Writes the tag info in TagSerialNumber

          String stringSerialNumber = "";
          for (int i = 0; i < 4; i++) { // Loop to print serial number to serial monitor
            stringSerialNumber += String(TagSerialNumber[i], HEX);
            if (i != 3) {
              stringSerialNumber += " ";
            }
          }

          Serial.println("SerialNumber=" + stringSerialNumber);
          lcd.setCursor(0,0);
          lcd.print("SN:");
          lcd.print(stringSerialNumber);
        }
    }
  /*
  // when characters arrive over the serial port...
  if (Serial.available()) {
    // wait a bit for the entire message to arrive
    delay(100);
    // clear the screen
    lcd.clear();
    // read all the available characters
    while (Serial.available() > 0) {
      // display each character to the LCD
      if (Serial.read() == 'o') {
          lcd.print("LED is ON");
          digitalWrite(13, HIGH);
        }

      else if (Serial.read() == 'x') {
          lcd.print("LED is OFF");
          digitalWrite(13, LOW);
        }
    }
  }
  */

  
}
