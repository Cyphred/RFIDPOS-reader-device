/*
[PINOUTS]
  RC522 - https://www.brainy-bits.com/card-reader-with-an-arduino-rfid-using-the-rc522-module/
    - 3.3V -> 3.3V
    - RST -> 6
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

#include <Wire.h>              // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD
#include <MFRC522.h>           // RFID Module Library
#include <SPI.h>               // Used for communication via SPI with the RFID Module

#define SDAPIN 10  // RFID Module SDA Pin connected to digital pin
#define RESETPIN 6 // RFID Module RESET Pin connected to digital pin

byte FoundTag;                                          // value to tell if a tag is found
byte ReadTag;                                           // Anti-collision value to read tag information
byte TagData[MAX_LEN];                                  // full tag data
byte TagSerialNumber[5];                                // tag serial number
byte GoodTagSerialNumber[5] = {0x95, 0xEB, 0x17, 0x53}; // The Tag Serial number we are looking for
byte version;                                           // Variable to store Firmware version of the Module

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library
MFRC522 nfc(SDAPIN, RESETPIN);                          // Initialization for RFID Reader with declared pinouts for SDA and RESET

int menuState = 1;
const int buzzer = 9;

void setup()
{
    SPI.begin();
    Serial.begin(115200);
    pinMode(buzzer, OUTPUT);

    // Initiate the LCD:
    lcd.init();
    lcd.backlight();
    lcd.noCursor();

    // Start to find an RFID Module
    lcd.setCursor(0, 0);
    lcd.print("Looking for RFID");
    lcd.setCursor(0, 1);
    lcd.print("Reader");
    nfc.begin();
    version = nfc.getFirmwareVersion();

    // If can't find an RFID Module
    if (!version)
    {
        lcd.setCursor(0, 0);
        lcd.print("RFID Reader not");
        lcd.setCursor(0, 1);
        lcd.print("found");
        while (1)
            ; // Wait until a RFID Module is found
    }
    else
    {
        // If found, print the information about the RFID Module
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RC522 found!");
        lcd.setCursor(0, 1);
        lcd.print("0x");
        lcd.setCursor(2, 1);
        lcd.print(version, HEX);
    }
    tone(buzzer, 2000);
    delay(250);
    noTone(buzzer);
    delay(250);
    tone(buzzer, 2000);
    delay(1000);
    noTone(buzzer);
    lcd.clear();

    Serial.println('g');
}

void loop()
{
    if (menuState == 1)
    {
        secureScan();
    }
}

void scan()
{
    String stringSerialNumber = "";
    // repeat until the retrieved serial number is 8 characters long
    while (stringSerialNumber.length() != 8)
    {
        // Check if a tag was detected
        // If yes, then variable FoundTag will contain "MI_OK"
        FoundTag = nfc.requestTag(MF1_REQIDL, TagData);

        if (FoundTag == MI_OK)
        {
            delay(200);
            ReadTag = nfc.antiCollision(TagData); // Get anti-collision value to properly read information from the tag
            memcpy(TagSerialNumber, TagData, 4);  // Writes the tag info in TagSerialNumber
            // Loop to print serial number to serial monitor
            for (int i = 0; i < 4; i++)
            {
                stringSerialNumber += String(TagSerialNumber[i], HEX);
            }
        }
    }
    Serial.println(stringSerialNumber);
    // play tone in buzzer
    tone(buzzer, 2000);
    delay(500);
    noTone(buzzer);
}

void secureScan()
{
    scan();
    String input = "";
    char pin[6];
    boolean validPIN;

    do
    {
        validPIN = true;
        while (input.length() != 6)
        {
            input = Serial.readStringUntil('\n');
        }
        Serial.println("received");
        input.toCharArray(pin, 6);
        for (char c : pin)
        {
            if (!isDigit(c))
            {
                validPIN = false;
            }
        }

        if (!validPIN)
        {
            validPIN = false;
            Serial.println("PIN Invalid:" + input);
            input = "";
        }
    } while (!validPIN);

    Serial.println("PIN Received");
}
