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

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.noCursor();

    // Initialize RFID Module
    nfc.begin();
    version = nfc.getFirmwareVersion();
}

void loop()
{
    splash();
    String command = waitForSerialInput(); // Listening for commands sent over Serial monitor
    // valid commands will result in a '1' being sent to Serial
    // otherwise, a '2' is sent

    // check commands for testing specific components and connectivity for easier troubleshooting
    if (command.equals("check")) {
        Serial.println(1);
        command = waitForSerialInput();

        // testing RFID Module
        // returns '1' if OK
        // returns '2' if not
        if (command.equals("nfc")) {
            if (checkNFC()) {
                Serial.println(1);
            }
            else {
                Serial.println(2);
            }
        }
        // testing GSM Module
        else if (command.equals("gsm")) {
            Serial.println(1);
            command = waitForSerialInput();

            // testing GSM Module
            // returns '1' if OK
            // returns '2' if not
            if (command.equals("status")) {
                if (checkGSM()) {
                    Serial.println(1);
                }
                else {
                    Serial.println(2);
                }
            }

            // testing GSM Module signal
            // prints integer value to Serial
            else if (command.equals("signal")) {
                Serial.println(getGSMSignal());
            }
        }
    }

    // Scan RFID Card and send its Serial number
    else if (command.equals("scan")) {
        Serial.println(scan());
    }

    else if (command.equals("challenge")) {
        challenge();
    }

    // GSM commands for sending SMS
    else if (command.equals("gsm")) {
        sendSMS();
    }
    else {
        Serial.println(2);
    }
    
}

// Waits for any input to arrive via serial
String waitForSerialInput() {
    String input = "";
    while (input.length() == 0) {
        input = Serial.readStringUntil('\n');
    }
    return input;
}

// Displays splash screen on LCD
void splash() {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RFID POS SCANNER");
    lcd.setCursor(6,1);
    lcd.print("v1.0");
}

// Checks if the RFID Module if functional
boolean checkNFC()
{
    if (version)
    {
        return true;
    }
    return false;
}

// TODO Checks if the GSM Module is functional
boolean checkGSM() {
    return false;
}

// TODO Gets GSM Signal Quality
int getGSMSignal() {
    return 0;
}

// TODO Sends an SMS with the GSM Module
void sendSMS() {
    buzzerError();
}

// Waits for an RFID card to be scanned
// Returns the unique ID of RFID card as 8-character String
String scan()
{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Place your card");
    lcd.setCursor(0,1);
    lcd.print("near the scanner");

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

    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Card Scanned");
    buzzerSuccess();
    return stringSerialNumber;
}

// Waits for an RFID to be scanned
// then waits for a corresponding passcode to be fetched from DB
// if a valid
void challenge()
{
    // Wait for 6-digit PIN to arrive. Make sure that on the Java program will only send a 6-character long string of purely numbers
    String passcode = "";
    while (passcode.length() != 6)
    {
        passcode = Serial.readStringUntil('\n');
    }

    String input = "";
    boolean passcodeMatch = false;

    for (int x = 3; x > 0; x--)
    {
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("PIN :");
        lcd.setCursor(8, 0);

        while (input.length() != 6)
        {
            input += keypad();
            lcd.print("*");
        }

        if (passcode.equals(input))
        {
            passcodeMatch = true;
            lcd.clear();
            lcd.setCursor(2, 0);
            lcd.print("Verification");
            lcd.setCursor(3, 1);
            lcd.print("Successful");
            delay(2000);
            break;
        }
        else
        {
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Incorrect PIN.");
            lcd.setCursor(1, 1);
            lcd.print(x - 1);
            lcd.setCursor(3, 1);
            lcd.print("retries left");
            buzzerError();
            delay(1250);
            input = "";
        }
    }

    // if passcode matches, prints "ok"
    // else, prints "no"
    // the java program should be listening for these values after sending the correct passcode
    if (passcodeMatch)
    {
        Serial.println("ok");
    }
    else
    {
        Serial.println("no");
    }
    
}

// Temporary keypad that works through single-character inputs through the serial monitor
// Will get rid of this when the actual keypad component arrives
int keypad()
{
    String input = "";
    while (input.length() != 1)
    {
        input = Serial.readStringUntil('\n');
    }
    return input.toInt();
}

// Plays an error tone for 750ms so I don't have to write these couple lines down every single time
void buzzerError()
{
    tone(buzzer, 500);
    delay(250);
    noTone(buzzer);
    delay(250);
    tone(buzzer, 500);
    delay(250);
    noTone(buzzer);
}

// Plays success tone for 500ms so I don't have to write these couple lines down every single time
void buzzerSuccess() {
    tone(buzzer, 2000);
    delay(500);
    noTone(buzzer);
}