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
boolean gsmReady; // Keeps track if the GSM module was active the last time it was checked
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

// SMS fields
uint32_t purchaseAmount_whole;
uint32_t purchaseAmount_decimal;
uint32_t availableBalance_whole;
uint32_t availableBalance_decimal;
uint32_t transactionID;
uint32_t purchaseUnixtime;

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
    if (checkGSM() != 1) {
        toggleGSMPower(); // Turn on GSM module
         delay(3000); // Give the GSM module time to initialize
        // If the GSM module still won't respond
        if (checkGSM() != 1) {
            clearLCDRow(1);
            lcd.print("GSM Error");
            gsmReady = false;
            buzzerError();
        }
        // If the GSM module has responded
        else {
            gsmReady = true;
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

            case 134: // Check GSM status
                Serial.write(3);
                Serial.print(checkGSM());
                Serial.write(3);
                newByte = false;
                break;

            case 135: //TODO Get GSM Signal Quality

                break;

            case 139: // Challenge
                menuID = 3;
                newByte = false;
                break;
        }
    }

    // Shows the splash screen
    if (menuID == 1) {
        // Check if the last menu ID was not the splash screen
        if (lastMenuID != 1) {
            lcd.clear();
            lcd.setCursor(16,0);
            // Write the welcome message template
            for (int x = 0; x < 11; x++) {
                lcd.write(welcomeMessage[x]);
            }

            // Write the store name
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

// Checks the GSM module's status
// returns an integer with a corresponding state
// 0 = Error
// 1 = Ok
// 2 = Timeout - This means the GSM module did not respond within the allocated time
// It also updates gsmReady with true or false, depending on the result every time it is called
int checkGSM() {
    gsmSerial.print("AT\r"); // Sends AT command to check GSM status
    timeoutStart = millis(); // Mark the start of the timeout
    resetBufferEnding(); // Clear the current contents of the buffer ending
    
    // Wait for 100ms before timing out
    while((millis() - timeoutStart) < 100) {
        while (gsmSerial.available()) {
            addByteToBufferEnding(gsmSerial.read());
        }

        // If the buffer ending of "OK" or "ERROR" has been received
        if (bufferEnding[0] == 'O' && (bufferEnding[1] == 'K' || bufferEnding[1] == 'R')) {
            break;
        }
    }

    if (bufferEnding[0] == 0) {
        gsmReady = false;
        return 2;
    }
    else if (bufferEnding[1] == 'K') {
        gsmReady = true;
        return 1;
    }
    else if (bufferEnding[1] == 'R') {
        gsmReady = false;
        return 0;
    }
}

// TODO Queries for the signal quality of the SIM in db
int getGSMSignalQuality() {
    gsmSerial.print("AT+CSQ\r"); // AT command for checking GSM signal quality
}

void PINChallenge() {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Fetching PIN");
    lcd.setCursor(2,1);
    lcd.print("from database");

    char targetPIN[6]; // Will store the correct PIN
    byte PINCharactersReceived = 0; // Keeps track of the number of digits received as the target PIN

    // Infinite loop to wait for the operation to complete or be cancelled
    while (true) {
        byte readByte = Serial.read(); // Read serial data
        // Cancels the operation
        if (readByte == 131) {
            break;
        }
        // The end marker for the PIN
        else if (readByte == 3) {
            break;
        }
        // If the received byte is a digit, and the number of stored digits have not been satisfied yet
        else if (readByte > 47 && readByte < 58 && PINCharactersReceived < 6) {
            targetPIN[PINCharactersReceived] = readByte; // Add the digit to the target PIN
            PINCharactersReceived++; // Increment the counter for the received digits
        }
        // TODO also add a call for device status and gsm status here
    }

    byte attemptsLeft = 3; // Keeps track of the attempts left

    // Only runs if the number of target PIN characters has been satisfied
    // Otherwise, assumes that the operation has been cancelled
    if (PINCharactersReceived == 6) {
        // Another infinite loop that will only end when it is cancelled, failed or completed
        while (true) {
            char inputDigits[6]; // Will hold the entered digits from the keypad
            byte cursorPosition = 0; // Will keep track of the current position of the "cursor"

            // Prompts the user to enter their PIN
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("PIN:");
            lcd.setCursor(0,1);
            lcd.print("[*]OK  [#]DELETE");
            lcd.setCursor(7,0);

            // Another infinite inner loop that will catch the user's inputs
            while (true) {
                byte readByte = Serial.read(); // Reads the serial in case a cancel byte or status inquiry is issued
                if (readByte == 131) {
                    return; // Cancels the method
                }
                else {
                    // TODO Add status inquries here
                }

                char readKey = keypad.getKey(); // Grab data from keypad
                // If the keypad was pressed
                if (readKey) {
                    // If the pressed key is a digit
                    if (readKey > 47 && readKey < 58) {
                        // If there are already 6 digits entered
                        if (cursorPosition < 6) {
                            lcd.print('*');
                            inputDigits[cursorPosition] = readKey;
                            cursorPosition++;
                        }
                    }
                    // If the pound key is pressed (Delete Key)
                    else if (readKey == '#') {
                        // Check if the current cursor position is not at zero
                        if (cursorPosition > 0) {
                            lcd.setCursor((6 + cursorPosition),0);
                            lcd.write(32); // Overwrite with space
                            lcd.setCursor((6 + cursorPosition),0);
                            cursorPosition--;
                        }
                    }
                    // If the asterisk key is pressed (OK Key)
                    else if (readKey == '*') {
                        // Check if there are already 6 digits entered
                        if (cursorPosition == 6) {
                            boolean validPIN = true;
                            // Check every digit in the input and the target PINs to see if there is a mismatch
                            for (int x = 0; x < 6; x++) {
                                // If a mistmatch is found
                                if (inputDigits[x] != targetPIN[x]) {
                                    validPIN = false;
                                    break;
                                }
                            }

                            // If no mismatches were found
                            if (validPIN) {
                                // Prompt PIN match
                                lcd.clear();
                                lcd.setCursor(2,0);
                                lcd.print("Verification");
                                lcd.setCursor(4,1);
                                lcd.print("Success");
                                buzzerSuccess();
                                delay(1500);

                                Serial.print(1); // Tell the POS that verification was a Success
                                lcd.clear(); // Clear the LCD before finishing
                                return; // Ends the method
                            }
                            // If a mismatch is found
                            else {
                                attemptsLeft--; // Deduct the number of attempts left
                                // Prompt PIN mismatch
                                lcd.clear();
                                lcd.setCursor(2,0);
                                lcd.print("Invalid PIN");
                                lcd.setCursor(1,1);
                                lcd.print(attemptsLeft);
                                lcd.setCursor(3,1);
                                lcd.print("retries left");
                                buzzerError();
                                delay(1250);

                                // If there are no more attempts left
                                if (attemptsLeft == 0) {
                                    Serial.print(0);
                                    lcd.clear(); // Clear the LCD before finishing
                                    return; // Ends the method
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    lcd.clear(); // Clear the LCD before finishing
}

// Saves an SMS template to the EEPROM
// This will have fields that will be parsed and included in the SMS
void setSMSTemplate() {
    // Keeps track of what EEPROM address to write to
    // Starts with address 25 to accomodate for the 24-character store name (address 0-23)
    // and the end marker which will be at address 24 if the store name reaches the 24-character limit
    writeSerialDataToEEPROM(25);
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

// Adds a byte to the 2-byte buffer ending
void addByteToBufferEnding(byte input) {
    if (input != 10 && input != 13) {
        bufferEnding[0] = bufferEnding[1];
        bufferEnding[1] = input;
    }
}

// Resets the buffer ending with byte 0
void resetBufferEnding() {
    bufferEnding[0] = 0;
    bufferEnding[1] = 0;
}

// Toggles the power switch of the GSM module
void toggleGSMPower() {
    digitalWrite(gsmPowerPin,HIGH);
    delay(1000);
    digitalWrite(gsmPowerPin,LOW);
}

// Parse SMS template and pass it to GSM module
void parseSMSTemplate() {
    // Prints date and time
    char dateBuffer[32];
    // Parse unixtime into readable date
    sprintf(
        dateBuffer,"%02d-%02d-%02d %02d:%02d:%02d",
        year(purchaseUnixtime),
        month(purchaseUnixtime),
        day(purchaseUnixtime),
        hour(purchaseUnixtime),
        minute(purchaseUnixtime),
        second(purchaseUnixtime)
    );
    gsmSerial.println(dateBuffer); // Print the date and time to the SMS

    byte readByte, nextByte; // Stores the currently read bytes
    boolean ignoreNextByte = false; // Will keep track if the next read byte is supposed to be ignored
    int readPosition = 25; // The starting position to read from the EEPROM
    // Will iterate until the maximum size of the EEPROM available on the Arduino UNO
    while (readPosition < 512) {
        readByte = EEPROM.read(readPosition); // Read the byte at the current address

        // Only attempt to read the next byte when the currently selected address is not the last one
        if (readPosition < 511) {
            nextByte = EEPROM.read(readPosition + 1);
        }

        readPosition++; // Increment read position to move onto the next address after this iteration

        // If the current byte is NOT supposed to be ignored
        if (!ignoreNextByte) {
            // If the end marker has been reached, break out of the while loop
            if (readByte == 3) {
                break;
            }
            // If a STX marker is encountered, this marks a field with a corresponding code
            else if (readByte == 2) {
                ignoreNextByte = true; // The next byte should be ignored because it is a character that represents the field's code
                switch (nextByte) {
                    // Purchase Amount (Total)
                    case 65:
                        gsmSerial.print(purchaseAmount_whole);
                        gsmSerial.print('.');
                        // Prints the a zero before the actual decimal if it is smaller than 10
                        if (purchaseAmount_decimal < 10) {
                            gsmSerial.print('0');
                        }
                        gsmSerial.print(purchaseAmount_decimal);
                        break;

                    // Store Name
                    case 66:
                        for (int x = 0; x < 24; x++) {
                            byte readName = EEPROM.read(x);
                            if (readName == 3) {
                                break;
                            }
                            else {
                                gsmSerial.write(readName);
                            }
                        }
                        break;

                    // Available Balance
                    case 67:
                        gsmSerial.print(availableBalance_whole);
                        gsmSerial.print('.');
                        // Prints a zero beforet he actual decimal if it is smaller than 10
                        if (availableBalance_decimal < 10) {
                            gsmSerial.print('0');
                        }
                        gsmSerial.print(availableBalance_decimal);
                        break;

                    // Transaction ID
                    case 68:
                        gsmSerial.print(transactionID);
                        break;
                }
            }
            // If a regular character is received, send it to the GSM
            else {
                gsmSerial.write(readByte);
            }
        }
        // If the current byte is supposed to be ignored
        else {
            ignoreNextByte = false; // Set ignoreNextByte to false so that the next byte will not be ignored
        }
    }
}

// Sets the unix time of the last purchase
// This will be parsed and included in the SMS
void setPurchaseUnixTime() {
    purchaseUnixtime = catchLong();
}

void setPurchaseAmount() {
    catchDouble(0);
}

void setAvailableBalance() {
    catchDouble(1);
}

void setTransactionID() {
    transactionID = catchLong();
}

// Catches a byte stream containing a double value
/* Possible parameters
    0 - Purchase Amount
    1 - Available Balance
*/
double catchDouble(int targetVariable) {
    uint32_t wholeNumber = 0;
    uint32_t decimal = 0;
    boolean periodReceived = false;
    boolean endMarkerReceived = false;

    // Wait for the end marker to arrive before proceeding
    while (!endMarkerReceived) {
        // While there is data coming in from the Serial monitor
        while(Serial.available()) {
            byte readByte = Serial.read(); // Read arriving Serial data
            // If an end marker is received
            if (readByte == 3) {
                endMarkerReceived = true;
                break;
            }
            // If the read byte is a period
            else if (readByte == 46) {
                periodReceived = true;
            }
            // If the read byte is a valid digit
            else if (readByte > 47 && readByte < 58) {
                // If a period hasn't been received yet, incoming values are to be added to
                // the "whole number" variable
                if (!periodReceived) {
                    // Multiply the whole number by 10 to accomodate the adding of another digit
                    wholeNumber = (wholeNumber * 10) + (readByte - 48);
                }
                // If a period has already been received, incoming values are to be added to
                // the "decimal" variable
                else {
                    // Multiply the decimal by 10 to accomodate the adding of another digit
                    decimal = (decimal * 10) + (readByte - 48);
                }
            }
        }
    }

    switch (targetVariable) {
        case 0:
            purchaseAmount_whole = wholeNumber;
            purchaseAmount_decimal = decimal;
            break;
        case 1:
            availableBalance_whole = wholeNumber;
            availableBalance_decimal = decimal;
            break;
    }
}

// Catches a byte stream containing a long value
uint32_t catchLong() {
    uint32_t returnValue = 0;
    boolean endMarkerReceived = false;

    // Wait for the end marker to arrive before proceeding
    while (!endMarkerReceived) {
        // While there is data coming in from the Serial monitor
        while(Serial.available()) {
            byte readByte = Serial.read(); // Read arriving Serial data
            // If an end marker is received
            if (readByte == 3) {
                endMarkerReceived = true;
                break;
            }
            // If the read byte is a digit
            else {
                returnValue *= 10;
                returnValue += (readByte - 48);
            }
        }
    }

    return returnValue;
}