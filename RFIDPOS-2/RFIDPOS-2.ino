#include <Wire.h>                   // for I2C communication with the LCD
#include <LiquidCrystal_I2C.h>      // main library for the LCD functionality
#include <SoftwareSerial.h>         // software Serial for communication with the GSM Module
#include <TimeLib.h>                // for time conversion functionality
#include <SPI.h>                    // for SPI communication with the RFID scanner
#include <MFRC522.h>                // main library for RFID scanner functionality
#include <Keypad.h>                 // for keypad support

// GSM Module variables and declarations
SoftwareSerial gsmSerial(8,7); // Create SoftwareSerial object
int gsmPowerPin = 6; // Digital pin connected to the GSM module's power toggle switch
boolean gsmReady = false; // Keeps track if the GSM module was active the last time it was checked
byte bufferEnding[2] = {0,0}; // Will hold the last 3 bytes of a byte stream coming in through the software serial

// RFID reader variables and declarations
#define SDAPIN 10  // RFID Module SDA Pin connected to digital pin
#define RESETPIN 9 // RFID Module RESET Pin connected to digital pin
MFRC522 nfc(SDAPIN, RESETPIN); // Initialization for RFID Reader with declared pinouts for SDA and RESET
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
boolean backlightOn;

// Buzzer declarations
const int buzzerPin = A3;
boolean muteBuzzer = false;

// Menu operation variables
byte menuID = 1;
byte lastMenuID = 0;

// Misc variables
byte arrivingByte; // Stores the arriving byte each iteration of the loop
boolean newByte; // Keeps track if the last arriving byte has not been interpreted yet
uint32_t timeoutStart; // Global variable for storing the start of timeouts
long lastStatusPing; // Keeps track of the last time a status ping has been sent to the system

// Splash screen scrolling
uint32_t lastStep; // The last millis() time that the text was scrolled
byte splashText[2][40]; // Will store the two-row splash screen messages
byte splashTextLength[2] = {0,0}; // Will keep track of the length of the splash text lines
int currentIndex = 0; // The current starting index to print the message on
uint32_t scrollSpeed = 500; // The time intervals in ms between each scroll step

void setup() {
    Serial.begin(115200); // Initialize hardware serial communication with 
    gsmSerial.begin(19200); // Initialize software serial communication with the GSM module

    pinMode(buzzerPin,OUTPUT); // Set the pin of the buzzer as a digital output
    pinMode(gsmPowerPin,OUTPUT); // Set the pin for the GSM power switch as a digital output

    // LCD
    lcd.init(); // Initialization
    lcd.backlight(); // Enable the backlight
    backlightOn = true; // Set the backlight's status to ON
    lcd.noCursor(); // Hides the LCD cursor

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Initializing");

    // RFID Reader
    lcd.setCursor(0,1);
    lcd.print("RFID Reader");
    SPI.begin(); // Begin SPI communication
    nfc.PCD_Init(); // Initialize the MFRC522

    // GSM Module
    clearLCDRow(1);
    lcd.print("GSM Module");
    // Check the GSM module first to see if it is not turned on yet
    if (!checkGSM(100)) {
        toggleGSMPower(); // Turn on GSM module

        // Query GSM until it responds with OK
        timeoutStart = millis();
        while ((millis() - timeoutStart) < 3000) {
            if (checkGSM(100)) {
                gsmReady = true;
                break;
            }
        }

        //delay(3000); // Give the GSM module time to initialize
        // If the GSM module still won't respond
        if (!gsmReady) {
            clearLCDRow(1);
            lcd.print("GSM Error");
            gsmReady = false;
            buzzerError();
            delay(1250);
        }
    }
    // If the GSM module is already turned on
    else {
        gsmReady = true;
    }

    // Prompt waiting for connection
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Waiting for");
    lcd.setCursor(0,1);
    lcd.print("connection");

    Serial.write(5); // Sends the signal asking for a reply to the POS

    // Wait for the splash screen text to arrive
    // Iterate twice for each row of splash screen text
    for (int x = 0; x < 2; x++) {
        // Wait for the splash screen text data to arrive
        // NOTE Splash screen can only be a maximum of 40 characters per line
        byte splashTextCharactersReceived = 0;
        while (true) {
            byte readByte = Serial.read();

            if (readByte == 3) { // If the read byte is an end marker
                splashText[x][splashTextCharactersReceived] = 3;
                break; // End the loop
            }
            else if (readByte != 255) { // If the read byte is part of the store name
                splashText[x][splashTextCharactersReceived] = readByte;
                splashTextCharactersReceived++; // Increment the number of characters received
            }

            if (splashTextCharactersReceived == 32) { // If the store name character limit has been reached
                break; // end the loop
            }
        }
        splashTextLength[x]  = splashTextCharactersReceived;
    }

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connection");
    lcd.setCursor(0,1);
    lcd.print("Established");
    buzzerSuccess();
    delay(1500);

    lcd.clear();
}

void loop() {
    /*
    if ((millis() - lastStatusPing) > 1000) { // Sends a status ping to the device every second
        lastStatusPing = millis();
        // TODO create pinging routine
    }
    */

    // If a byte has arrived via serial
    if (Serial.available()) {
        arrivingByte = Serial.read();
        newByte = true;
    }

    // Interpret the arrived byte to see if it is a general command byte
    // First, check if the arrived byte has not been read yet to avoid a repeat read
    // If the arrived byte is interpreted in the switch statement, set its state to already read
    if (newByte) {
        switch (arrivingByte) {
            case 132: // Scan an RFID tag
                menuID = 2;
                newByte = false;
                break;

            case 134: // Check GSM status
                Serial.print(checkGSM(100));
                newByte = false;
                break;

            case 135:
                Serial.write(2);
                Serial.write(135); // nextbyte identifier
                Serial.print(getGSMSignalQuality());
                Serial.write(3);
                newByte = false;
                break;

            case 136: // Send SMS
                menuID = 5;
                newByte = false;
                break;

            case 137: // Toggle GSM power
                toggleGSMPower();
                newByte = false;
                break;

            case 138: // AT Debug Mode
                menuID = 6;
                newByte = false;
                break;

            case 139: // Challenge
                menuID = 3;
                newByte = false;
                break;

            case 141: // Create new PIN
                menuID = 4;
                newByte = false;
                break;

            case 142: // Device status inquiry
                Serial.write(2);
                Serial.write(142); // nextbyte identifier
                Serial.print(1);
                Serial.write(3);
                break;

            case 143: // Check SIM
                Serial.print(checkSIM());
                newByte = false;
                break;
        }
    }

    // Shows the splash screen
    if (menuID == 1) {
        // Check if the last menu ID was not the splash screen
        if (lastMenuID != 1) {
            lcd.clear();

            // Iterate twice for each row
            for (int x = 0; x < 2; x++) {
                lcd.setCursor(0,x);

                // Write the store name
                for (int y = 0; y < 40; y++) {
                    if (splashText[x][y] == 3) {
                        break;
                    }
                    lcd.write(splashText[x][y]);
                }
            }
        }
        
        // Check if the splash screen needs to be scrolled
        if (splashTextLength[0] > 16 || splashTextLength[1] > 16) {
            // Check if it is time to scroll the splash text
            if ((millis() - lastStep) >= scrollSpeed) {
                lastStep = millis();
                lcd.scrollDisplayLeft();
            }
        }
    }
    // Scan an RFID tag
    else if (menuID == 2) {
        RFIDRead();
    }
    // PIN Challenge
    else if (menuID == 3) {
        byte result = PINChallenge();
        if (result != 2) { // If the method was not cancelled
            Serial.print(result);
        }
    }
    // Create new PIN
    else if (menuID == 4) {
        PINCreate();
    }
    // Send an SMS
    else if (menuID == 5) {
        Serial.print(sendSMS());
    }
    // AT Command Mode
    else if (menuID == 6) {
        ATCommandMode();
    }

    lastMenuID = menuID; // Save the current menu ID to be remembered as the last menu ID
    menuID = 1; // Set the next menu ID to the splash screen
}

/**
 * Prompts the user to scan an RFID tag and prints the UID to Serial
 */
void RFIDRead() {
    lcd.clear();
    lcd.print("Place your card");
    lcd.setCursor(0,1);
    lcd.print("near the scanner");

    // Indefinite loop while waiting for a card to be scanned
    while (true) {
        byte readByte = Serial.read(); // Read serial data
        if (readByte == 131) { // Cancels the operation
            break; // Break out of the indefinite loop
        }

        // If a card has been detected and read
        if (nfc.PICC_IsNewCardPresent() && nfc.PICC_ReadCardSerial()) {
            // Send tag data via Serial with start and end markers
            Serial.write(2);
            nfc.PICC_DumpUIDToSerial(&(nfc.uid));
            Serial.write(3);
            
            // Prompt the user that a card has been scanned
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("Card Scanned");
            buzzerSuccess();
            delay(1500);

            break; // Break out of the indefinite loop
        }
    }

    lcd.clear(); // Clear the LCD before finishing
}

/**
 * Clears a row on the LCD
 */
void clearLCDRow(int row) {
    lcd.setCursor(0,row);
    for (int x = 0; x < 16; x++) {
        lcd.write(32);
    }
    lcd.setCursor(0,row);
}

/**
 * Plays an error tone for 750ms so I don't have to write these couple lines down every single time
 */
void buzzerError() {
    if (!muteBuzzer) {
        tone(buzzerPin, 150);
        delay(250);
        noTone(buzzerPin);
        delay(250);
        tone(buzzerPin, 150);
        delay(250);
        noTone(buzzerPin);
    }
}

/**
 * Plays success tone for 500ms so I don't have to write these couple lines down every single time
 */
void buzzerSuccess() {
    if (!muteBuzzer) {
        tone(buzzerPin, 2000);
        delay(500);
        noTone(buzzerPin);
    }
}