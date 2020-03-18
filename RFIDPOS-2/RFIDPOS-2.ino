#include <Wire.h>                   // for I2C communication with the LCD
#include <LiquidCrystal_I2C.h>      // main library for the LCD functionality
#include <SoftwareSerial.h>         // software Serial for communication with the GSM Module
#include <EEPROM.h>                 // for allowing read/write operations to the EEPROM of the Arduino UNO
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

// Misc declarations
byte arrivingByte; // Stores the arriving byte each iteration of the loop
boolean newByte; // Keeps track if the last arriving byte has not been interpreted yet
boolean deviceConnected; // Keeps track if
uint32_t timeoutStart; // Global variable for storing the start of timeouts

// Splash screen scrolling
uint32_t lastStep; // The last millis() time that the text was scrolled
byte welcomeMessage[11] = {87,101,108,99,111,109,101,32,116,111,32}; // The message to be attached to the store name ("Welcome to ")
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
    
    Serial.write(5); // Communicate with the POS that the device is ready to initiate a connection
    // Wait for the handshake byte to be received
    while (true) {
        if (Serial.read() == 6) {
            break;
        }
    }

    deviceConnected = true;
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

            case 133:
                printStoreName();
                newByte = false;
                break;

            case 134: // Check GSM status
                Serial.print(checkGSM(100));
                newByte = false;
                break;

            case 135: //TODO Get GSM Signal Quality

                break;

            case 136: // Set store name
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

            case 140: // SMS Mode
                Serial.print(SMSSend());
                newByte = false;
                break;

            case 141: // Create new PIN
                menuID = 4;
                newByte = false;
                break;

            
        }
    }

    // Shows the splash screen
    if (menuID == 1) {
        // Check if the last menu ID was not the splash screen
        if (lastMenuID != 1) {
            lcd.clear();
            lcd.setCursor(0,0);
            // Write the welcome message template
            for (int x = 0; x < 11; x++) {
                lcd.write(welcomeMessage[x]);
            }

            // Write the store name
            //lcd.setCursor(0,1);
            for (int x = 0; x < 25; x++) {
                if (EEPROM.read(x) == 3) {
                    break;
                }
                lcd.write(EEPROM.read(x));
            }
        }
        
        // Check if it is time to scroll the splash text
        if ((millis() - lastStep) >= scrollSpeed) {
            lastStep = millis();
            lcd.scrollDisplayLeft();
        }
    }
    // Scan an RFID tag
    else if (menuID == 2) {
        RFIDRead();
    }
    // PIN Challenge
    else if (menuID == 3) {
        PINChallenge();
    }
    // Create new PIN
    else if (menuID == 4) {
        PINCreate();
    }
    // Set store name
    else if (menuID == 5) {
        setStoreName();
    }
    // AT Debug Mode
    else if (menuID == 6) {
        ATDebugMode();
    }

    lastMenuID = menuID; // Save the current menu ID to be remembered as the last menu ID
    menuID = 1; // Set the next menu ID to the splash screen
}

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
        // TODO also add a call for device status and gsm status here

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

void writeSerialDataToEEPROM(int startAddress) {
    // Keeps track of what EEPROM address to write to
    int writeToAddress = startAddress;
    boolean endMarkerReceived = false;

    // Wait for the end marker to arrive before proceeding
    while (!endMarkerReceived) {
        // While there is data coming in from the Serial monitor
        while(Serial.available()) {
            byte readByte = Serial.read(); // Read arriving Serial data
            // If an end marker is received
            if (readByte == 3) {
                smartEEPROMWrite(writeToAddress,3); // Write a END TEXT marker
                endMarkerReceived = true;
                break;
            }
            // If the read byte is part of the data to be stored
            else {
                smartEEPROMWrite(writeToAddress,readByte); // Write the arriving byte into storage
                writeToAddress++; // Increment target EEPROM address to write to
            }
        }
    }
}

// Checks the current adress if it has the same value as the candidate byte to reduce the need
// to overwrite an address with the same data
boolean smartEEPROMWrite(int address, byte input) {
    // Check if the candidate byte is different from the currently held byte
    if (EEPROM.read(address) != input) {
        EEPROM.write(address, input); // Write the candidate byte to the selected address to overwrite the old data
        return true; // Indicate that data has been overwritten
    }
    // If the candidate byte is the same with the currently held byte
    else {
        return false; // Indicate that no writing has been done
    }
}

// Clears a row on the LCD
void clearLCDRow(int row) {
    lcd.setCursor(0,row);
    for (int x = 0; x < 16; x++) {
        lcd.write(32);
    }
    lcd.setCursor(0,row);
}

// Plays an error tone for 750ms so I don't have to write these couple lines down every single time
void buzzerError() {
    if (!muteBuzzer) {
        tone(buzzerPin, 500);
        delay(250);
        noTone(buzzerPin);
        delay(250);
        tone(buzzerPin, 500);
        delay(250);
        noTone(buzzerPin);
    }
}

// Plays success tone for 500ms so I don't have to write these couple lines down every single time
void buzzerSuccess() {
    if (!muteBuzzer) {
        tone(buzzerPin, 2000);
        delay(500);
        noTone(buzzerPin);
    }
}

// TODO Make the POS verify with the device what name should it use
// Prints the store name
void printStoreName() {
    Serial.write(2);
    for (int x = 0; x < 24; x++) {
        byte readByte = EEPROM.read(x);
        // If the end marker is found
        if (readByte == 3) {
            break;
        }

        Serial.write(readByte);
    }
    Serial.write(3);
}

// Changes the stored store name
void setStoreName() {
    writeSerialDataToEEPROM(0);
}