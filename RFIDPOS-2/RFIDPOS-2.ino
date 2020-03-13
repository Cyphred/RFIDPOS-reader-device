#include <Wire.h>                   // for I2C communication with the LCD
#include <LiquidCrystal_I2C.h>      // main library for the LCD functionality
#include <SoftwareSerial.h>         // software Serial for communication with the GSM Module
#include <EEPROM.h>                 // for allowing read/write operations to the EEPROM of the Arduino UNO
#include <TimeLib.h>                // for time conversion functionality
#include <SPI.h>                    // for SPI communication with the RFID scanner
#include <MFRC522.h>                // main library for RFID scanner functionality

// GSM Module variables and declarations
SoftwareSerial gsmSerial(8,7); // Create SoftwareSerial object
int gsmPowerPin = 6; // Digital pin connected to the GSM module's power toggle switch

// RFID reader variables and declarations
#define SDAPIN 10  // RFID Module SDA Pin connected to digital pin
#define RESETPIN 9 // RFID Module RESET Pin connected to digital pin
MFRC522 nfc(SDAPIN, RESETPIN); // Initialization for RFID Reader with declared pinouts for SDA and RESET
byte version; // Variable to store Firmware version of the RFID Module
byte lastScannedID[4]; // Stores the unique ID of the last scanned RFID Tag

// Keypad declarations and declarations
const byte keypadRows = 4; // Keypad Rows
const byte keypadCols = 3; // Keypad Columns
// Keymap for the keypad
char keys[keypadRows][keypadCols] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[keypadRows] = {5,A2,A1,3}; // Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte colPins[keypadCols] = {4,A0,2};  // Connect keypad COL0, COL1 and COL2 to these Arduino pins.
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, keypadRows, keypadCols); // Initializes the keypad. To get the char from the keypad, char key = keypad.getKey(); then if (key) to check for a valid key


// LCD variables and declarations
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library
int lastPrintedMenu = 0; // An identifier for different LCD messages to prevent screen flickering

// Buzzer declarations
const int buzzer = A3;
boolean muteBuzzer = false;

void setup() {

}

void loop() {

}
