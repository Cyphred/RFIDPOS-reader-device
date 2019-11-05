/*
[PINOUTS]
  RC522 - https://www.brainy-bits.com/card-reader-with-an-arduino-rfid-using-the-rc522-module/
    - 3.3V -> 3.3V
    - RST -> 9
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
#define RESETPIN 9 // RFID Module RESET Pin connected to digital pin

byte FoundTag;                                          // value to tell if a tag is found
byte ReadTag;                                           // Anti-collision value to read tag information
byte TagData[MAX_LEN];                                  // full tag data
byte TagSerialNumber[5];                                // tag serial number
byte GoodTagSerialNumber[5] = {0x95, 0xEB, 0x17, 0x53}; // The Tag Serial number we are looking for
byte version;                                           // Variable to store Firmware version of the Module

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library
MFRC522 nfc(SDAPIN, RESETPIN);                          // Initialization for RFID Reader with declared pinouts for SDA and RESET

int menuState = 1;
const int buzzer = A3;

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
    lcd.clear();
    if (menuState == 1)
    {
        secureScan();
    }
}

String scan()
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
    // play tone in buzzer
    tone(buzzer, 2000);
    delay(500);
    noTone(buzzer);

    return stringSerialNumber;
}

void secureScan()
{
    Serial.println(scan()); // Prints card serial number to be read by Java program to be queried for the correct security code
    String passcode = "";

    // Wait for 6-digit PIN to arrive. Make sure that on the Java program will only send a 6-character long string of purely numbers
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Verifying...");
    while (passcode.length() != 6) {
        passcode = Serial.readStringUntil('\n');
    }

    // Java program will send "xxxxxx" if the scanned RFID tag does not exist in the database
    // othewise, proceed with the verification process
    if (passcode.equals("xxxxxx")) {
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Card Invalid");
        buzzerError();
        delay(1250);
    }
    else {
        String input = "";
        boolean passcodeMatch = false;
        
        for (int x = 3; x > 0; x--) {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("PIN :");
            lcd.setCursor(8,0);

            while (input.length() != 6) {
                input += keypad();
                lcd.print("*");
            }

            if (passcode.equals(input)) {
                passcodeMatch = true;
                lcd.clear();
                lcd.setCursor(2,0);
                lcd.print("Verification");
                lcd.setCursor(3,1);
                lcd.print("Successful");
                delay(2000);
                break;
            }
            else {
                lcd.clear();
                lcd.setCursor(1,0);
                lcd.print("Incorrect PIN.");
                lcd.setCursor(1,1);
                lcd.print(x - 1);
                lcd.setCursor(3,1);
                lcd.print("retries left");
                buzzerError();
                delay(1250);
                input = "";
            }
        }

        // if passcode matches, prints 1
        // else, prints 2
        // the java program should be listening for these values after sending the correct passcode
        if (passcodeMatch) {
            Serial.println(1);
        }
        else {
            Serial.println(2);
        }
    }
}

int keypad() {
    String input = "";
    while (input.length() != 1) {
        input = Serial.readStringUntil('\n');
    }
    return input.toInt();
}

void buzzerError() {
    tone(buzzer, 500);
    delay(250);
    noTone(buzzer);
    delay(250);
    tone(buzzer, 500);
    delay(250);
    noTone(buzzer);
}