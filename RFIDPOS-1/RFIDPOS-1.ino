/*
[PINOUTS]
  [RC522] - https://github.com/ljos/MFRC522
          - https://www.brainy-bits.com/card-reader-with-an-arduino-rfid-using-the-rc522-module/
    - 3.3V -> 3.3V
    - RST -> 9
    - GND -> GND
    - IRQ -> Nothing
    - SDA -> 10
    - MOSI -> 11
    - MISO -> 12
    - SCK -> 13

  [I2C LCD] - https://github.com/johnrickman/LiquidCrystal_I2C
            - https://www.makerguides.com/character-i2c-lcd-arduino-tutorial/
    - VCC -> 5V
    - GND -> GND
    - SDA -> A4
    - SCL -> A5

  [Keypad] - https://github.com/Chris--A/Keypad
           - https://playground.arduino.cc/Code/Keypad/
    Note: To get the pinouts of the keypad, rotate it counter-clockwise, from top to bottom pins:
    1 - ROW1 -> A2
    2 - ROW2 -> A1
    3 - COL2 -> D2
    4 - ROW3 -> D3
    5 - COL0 -> D4
    6 - ROW0 -> D5
    7 - COL1 -> A0

  [Buzzer]
    + -> A3
    - -> GND

  [GSM Module] - https://randomnerdtutorials.com/sim900-gsm-gprs-shield-arduino/
    GND -> Digital GND
    GND next to Vin -> Digital GND
    TXD -> D7
    RXD -> D8
*/

#include <Wire.h>              // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD
#include <MFRC522.h>           // RFID Module Library
#include <SPI.h>               // Library for communication via SPI with the RFID Module
#include <Keypad.h>            // Library for Keypad support
#include <SoftwareSerial.h>    // Library for Software Serial communication between the Arduino and the GSM Module

// GSM Module declarations
SoftwareSerial gsmSerial(7,8);

// RFID Reader declarations
#define SDAPIN 10  // RFID Module SDA Pin connected to digital pin
#define RESETPIN 9 // RFID Module RESET Pin connected to digital pin
MFRC522 nfc(SDAPIN, RESETPIN); // Initialization for RFID Reader with declared pinouts for SDA and RESET
byte version; // Variable to store Firmware version of the RFID Module

// Keypad declarations
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

// LCD declarations
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library

// Buzzer declarations
const int buzzer = A3;
boolean muteBuzzer = false;

// Runtime misc variables
const int challengeAttempts = 3;
String lastSentData = "";
byte lastSentByte;
boolean deviceConnected = false;
boolean startingState = true; // Is set to false once a connection is established with the POS System

// newScan() variables
byte newScan_scannedIDs[16][4];// will store 16 sets of 4 bytes each, representing the scanned card's UID
int newScan_storedIDs = 0; // counts the number of bytes stored in the array
byte newScan_uniqueIDs[16][4];// will store up to 16 sets of 4 bytes each, representing each scanned UID for comparison
int newScan_storedUniqueIDs = 0; // keeps track the actual number unique IDs scanned
int newScan_scores[16]; // keeps track of how many times each unique ID has appeared during the 16 passes of scanning
unsigned long newScan_lastScanTime = 0;

// challenge() and newPIN() misc variables
boolean pin_passcodeReceived = false;
String pin_passcode = "";
boolean pin_inputStreamActive = false;
char pin_inputCharacters[6];
char pin_inputCount = 0;
boolean pin_inputConfirmed = false;
int pin_retriesLeft = 3;
boolean pin_backConfirmDialogVisible = false;

// keypad
unsigned long lastKeyPress;
boolean enableKeypadSounds = false;
unsigned long beepStart;
int beepState = 0;
int keypadBeepTime = 50;

byte lastScannedID[4]; // Stores the unique ID of the last scanned RFID Tag
int gsmPower = 6;

int lastPrinted = 0; // An identifier for different LCD messages to prevent screen flickering
/*
    [lastPrinted Guide]
    1 - Waiting for connection
    2 - Connection Established
    3 - Disconnected
    4 - Splash screen
    5 - Place your card near the scanner
    6 - Card Scanned
    7 - Hold your card near the scanner
    8 - Scanning...
    9 - Scan Complete
    10 - Scan Failed
    11 - Fetching account information
    12 - PIN: (challenge)
    13 - Invalid PIN, X retries left
    14 - PIN: (newPIN)
    15 - CONFIRM:
    16 - PIN Confirmed
    17 - PIN does not match
    18 - 
    19 - Sending SMS
    20 - Powering on GSM
    21 - GSM Powered On!
    22 - GSM Failure!
    23 - Powering on GSM
*/

void setup()
{
    SPI.begin();
    gsmSerial.begin(19200);
    Serial.begin(115200);
    pinMode(buzzer,OUTPUT);
    pinMode(gsmPower,OUTPUT);

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.noCursor();

    // Initialize RFID Module
    nfc.begin();
    version = nfc.getFirmwareVersion();

    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Please wait...");
    lcd.setCursor(0,1);
    lcd.print("Powering on GSM");
    lastPrinted = 23;

    if (toggleGSMPower()) {
        if (lastPrinted != 21) {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("GSM Powered On!");
            lastPrinted = 21;
            delay(1000);
        }
    }
    else {
        if (lastPrinted != 22) {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("GSM Failure!");
            lastPrinted = 22;
            delay(1000);
        }
    }
    
    sendByte(128); // Signals the POS that the device is ready to initiate a connection
}

int operationState = 0; // keeps track of what operation is currently being performed
byte lastReadByte;

void loop() {
    lastReadByte = Serial.read();

    // if device is connected
    if (deviceConnected) {
        // Print the splash screen
        if (lastPrinted != 4 && operationState == 0) {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("RFID POS SCANNER");
            lcd.setCursor(6,1);
            lcd.print("v1.0");
            lastPrinted = 4;
        }

        updateOperationState();
        
        switch (operationState) {
        case 1:
            scan();
            break;

        case 2:
            newScan();
            break;

        case 3:
            Serial.print(checkGSM());
            resetOperationState();
            break;

        case 4:
            Serial.print(getGSMSignal());
            resetOperationState();
            break;

        case 5:
            if (sendSMS()) {
                Serial.print(1);
            }
            else {
                Serial.print(0);
            }
            resetOperationState();
            break;
        
        case 6:
            challenge();
            break;

        case 7:
            newPIN();
            break;

        case 8:
            testConnection();
            break;

        case 9:
            Serial.print(checkSIM());
            resetOperationState();
            break;

        case 10:
            if (toggleGSMPower()) {
                Serial.print(1);
            }
            else {
                Serial.print(0);
            }
            resetOperationState();
            break;

        case 11:
            if (gsmPowerOff()) {
                Serial.print(1);
            }
            else {
                Serial.print(0);
            }
            resetOperationState();
            break;

        default:
            break;
        }
    }

    // if device is not connected
    else {
        // if device is not connected and a connection hasn't been made prior
        if (startingState) {
            if (lastPrinted != 1) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Waiting for");
                lcd.setCursor(0,1);
                lcd.print("connection...");
                lastPrinted = 1;
            }
        }
        // if device is not connected and a connection has been made prior
        else {
            if (lastPrinted != 3) {
                lcd.clear();
                lcd.setCursor(5,0);
                lcd.print("Device");
                lcd.setCursor(2,1);
                lcd.print("Disconnected");
                buzzerError();
                delay(1250);
                lastPrinted = 3;
            }
        }

        // if byte 128 is received, set status as connected
        if (lastReadByte == 129) {
            lcd.clear();
            lcd.setCursor(3,0);
            lcd.print("Connection");
            lcd.setCursor(2,1);
            lcd.print("Established!");
            buzzerSuccess();
            delay(1500);
            lastPrinted = 2;
            startingState = false;
            deviceConnected = true;
            sendByte(49);
        }
    }
}

// Checks if the RFID Module if connected
boolean checkNFC()
{
    if (version)
    {
        return true;
    }
    return false;
}


int checkGSM()
{
    // Sends AT command to check GSM status
    gsmSerial.print("AT\r");
    // by default, return value will be '2' for timed out
    int returnValue = 2;
    // temp to temporarily store the bytes received as responses from the gsm module
    String temp = "";
    // The start of timeouts
    unsigned long timeoutStart;

    // Starts keeping track of time to wait for a response before timing out
    timeoutStart = millis();
    while (returnValue == 2 && (millis() - timeoutStart) < 5000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 10 || readByte != 13) {
                temp += (char)readByte;
            }
        }

        if (temp.indexOf("OK") >= 0) {
            returnValue = 1;
            break;
        }
        else if (temp.indexOf("ERROR") >= 0) {
            returnValue = 0;
            break;
        }
    }

    return returnValue;
}

int checkSIM() {
    gsmSerial.print("AT+CPIN?\r");
    int returnValue = 2;
    String temp = "";
    unsigned long timeoutStart = millis();
    boolean responseReceived = false;
    while (!responseReceived && (millis() - timeoutStart) < 3000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 13 && readByte != 10) {
                temp += (char)readByte;
            }
        }

        if (temp.length() > 2) {
            char lastChar[] = {temp.charAt(temp.length() - 2),temp.charAt(temp.length() - 1)};
            if (lastChar[0] == 'O') {
                if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                    responseReceived = true;
                    break;
                }
            }
        }
    }

    if (responseReceived) {
        if (temp.indexOf("READY") >= 0) {
            returnValue = 1;
        }
        else if (temp.indexOf("ERROR") >= 0) {
            returnValue = 0;
        }
    }

    return returnValue;
}

// Gets GSM Signal Quality
int getGSMSignal() {
    gsmSerial.print("AT+CSQ\r");
    int returnValue = -1;
    String temp = "";
    boolean responseReceived = false;

    unsigned long timeoutStart = millis();
    while (!responseReceived && (millis() - timeoutStart) < 1000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 13 && readByte != 10) {
                temp += (char)readByte;
            }
        }

        if (temp.length() > 2) {
            char lastChar[] = {temp.charAt(temp.length() - 2),temp.charAt(temp.length() - 1)};
            if (lastChar[0] == 'O') {
                if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                    responseReceived = true;
                }
            }
        }
    }

    if (responseReceived && temp.indexOf("OK") >= 0) {
        boolean spaceFound = false;
        String temp2 = "";
        for (int x = 0; x < temp.length(); x++) {
            if (spaceFound) {
                if (temp.charAt(x) != ',') {
                    temp2 += temp.charAt(x);
                }
                else {
                    break;
                }
            } else if (!spaceFound && temp.charAt(x) == ' ') {
                spaceFound = true;
            }
        }
        returnValue = temp2.toInt();
    }

    return returnValue;
}

// Sends an SMS with the GSM Module. Returns true if sending is successful
boolean sendSMS() {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Sending SMS");
    lastPrinted = 19;

    String number = ""; // Variable to store the customer's phone number
    int readState = 0; // Variable to keep track of the waiting status for the customer's number to arrive over serial
    // loops and stores the arriving data into String number until ETX or end of text character arrives
    while (readState != 2) {
        byte readByte = Serial.read();
        if ((readByte > 47 && readByte < 58) || (readByte == 2 || readByte == 3)) {
            if (readByte == 2 || readByte == 3) {
                readState++;
            }
            else if (readState == 1) {
                number += (char)readByte;
            }
            
            if (readState == 2) {
                if (number.length() != 12) {
                    readState = 0;
                    number = "";
                    sendByte(137); // Tells the POS that the received data is incomplete, and asks to resend
                }
                else {
                    sendByte(138); // Tells the POS that the received data is complete
                }
            }
        }
    }

    String message = "";
    readState = 0;
    while (readState != 2) {
        byte readByte = Serial.read();
        if (readByte != 255) {
            if (readByte == 2 || readByte == 3) {
                readState++;
            }
            else if (readState == 1) {
                message += (char)readByte;
            }
        }
    }

    message += (char)26;

    unsigned long timeoutStart;
    String temp = "";
    boolean smsAvailable = false;
    boolean responseReceived = false;

    // AT command to set gsmSerial to SMS mode
    gsmSerial.print("AT+CMGF=1\r");
    timeoutStart = millis();
    while ((millis() - timeoutStart) < 1000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 13 && readByte != 10) {
                temp += (char)readByte;
            }
        }

        if (temp.length() > 2) {
            char lastChar[] = {temp.charAt(temp.length() - 2),temp.charAt(temp.length() - 1)};
            if (lastChar[0] == 'O') {
                if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                    responseReceived = true;
                    break;
                }
            }
        }
    }
    
    if (responseReceived && temp.indexOf("OK") >= 0) {
        smsAvailable = true;
    }

    if (smsAvailable) {
        gsmSerial.print("AT+CMGS=\"+" + number + "\"\r"); 
        
        timeoutStart = millis();
        temp = "";
        responseReceived = false;

        while (!responseReceived && (millis() - timeoutStart) < 100) {
            if (gsmSerial.available()) {
                byte readByte = gsmSerial.read();
                if (readByte != 13 && readByte != 10) {
                    temp += (char)readByte;
                }
            }

            if (temp.length() > 2) {
                char lastChar[] = {temp.charAt(temp.length() - 2),temp.charAt(temp.length() - 1)};
                if (lastChar[0] == 'O') {
                    if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                        responseReceived = true;
                        break;
                    }
                }
            }
        }

        if (responseReceived) {
            if (temp.indexOf("ERROR") >= 0) {
                smsAvailable = false;
            }
        }

        if (smsAvailable) {
            gsmSerial.print(message);

            timeoutStart = millis();
            temp = "";
            responseReceived = false;

            while (!responseReceived && (millis() - timeoutStart) < 5000) {
                if (gsmSerial.available()) {
                    byte readByte = gsmSerial.read();
                    if (readByte != 13 && readByte != 10) {
                        temp += (char)readByte;
                    }
                }

                if (temp.length() > 2) {
                    char lastChar[] = {temp.charAt(temp.length() - 2),temp.charAt(temp.length() - 1)};
                    if (lastChar[0] == 'O') {
                        if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                            responseReceived = true;
                            break;
                        }
                    }
                }
            }

            if (responseReceived) {
                if (temp.indexOf("OK") >= 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Resets the GSM Module
boolean gsmPowerOff() {
    int status = checkGSM();
    if (status == 1) {
        digitalWrite(gsmPower,HIGH);
        delay(1000);
        digitalWrite(gsmPower,LOW);
        return true;
    }
    return false;
}

// Checks if an RFID tag is scanned
// Sends the unique ID of RFID card as a 4-byte stream
void scan() {
    byte FoundTag;           // value to tell if a tag is found
    byte ReadTag;            // Anti-collision value to read tag information
    byte TagData[MAX_LEN];   // full tag data
    byte TagSerialNumber[5]; // tag serial number
    boolean validTag = false;

    // Prompts user to scan their RFID card
    if (lastPrinted != 5) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Place your card");
        lcd.setCursor(0,1);
        lcd.print("near the scanner");
        lastPrinted = 5;
    }
    
    // Check if a tag was detected
    // If yes, then variable FoundTag will contain "MI_OK"
    FoundTag = nfc.requestTag(MF1_REQIDL, TagData);

    if (FoundTag == MI_OK) {
        delay(200);
        ReadTag = nfc.antiCollision(TagData); // Get anti-collision value to properly read information from the tag
        memcpy(TagSerialNumber, TagData, 4);  // Writes the tag info in TagSerialNumber

        // if tag data does not start with 0 and 32
        // I've noticed incompletely-read tag data tends to start with 0 and 32 as the firt 2 bytes
        if (!(TagSerialNumber[0] == 0 && TagSerialNumber[1] == 32)) {
            validTag = true;
        }
    }

    // if the scanned bytes are valid
    if (validTag) {
        // Notifies user of successful scan through displayed text on the LCD and a beep
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Card Scanned");
        lastPrinted = 6;
        buzzerSuccess();

        // Send 4 bytes representing the card's unique ID
        for (int x = 0; x < 4; x++) {
            lastScannedID[x] = TagSerialNumber[x];
        }

        printLastScannedID();
        resetOperationState();
    }
}

// Used for scanning new cards
// This process takes longer than a normal scan as it does multiple passes to ensure that the correct card information is read
// It will make mutiple scans and compare them to increase accuracy
void newScan() {
    byte FoundTag;           // value to tell if a tag is found
    byte ReadTag;            // Anti-collision value to read tag information
    byte TagData[MAX_LEN];   // full tag data
    byte TagSerialNumber[5]; // tag serial number
    boolean validTag = false;

    if (lastPrinted != 7 && newScan_storedIDs == 0) {
        // Prompts user to scan their RFID card
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Hold your card");
        lcd.setCursor(0,1);
        lcd.print("near the scanner");
        lastPrinted = 7;
    }

    // if there are not enough stored IDs from RFID Tags
    if (newScan_storedIDs < 16) {
        // Check if a tag was detected
        // If yes, then variable FoundTag will contain "MI_OK"
        FoundTag = nfc.requestTag(MF1_REQIDL, TagData);

        if (FoundTag == MI_OK) {
            if (lastPrinted != 8) {
                lcd.clear();
                lcd.setCursor(4,0);
                lcd.print("Scanning");
                lcd.setCursor(0,1);
                lastPrinted = 8;
            }

            delay(100);
            ReadTag = nfc.antiCollision(TagData); // Get anti-collision value to properly read information from the tag
            memcpy(TagSerialNumber, TagData, 4);  // Writes the tag info in TagSerialNumber

            // if tag data does not start with 0 and 32
            // I've noticed incompletely-read tag data tends to start with 0 and 32 as the firt 2 bytes
            if (!(TagSerialNumber[0] == 0 && TagSerialNumber[1] == 32)) {
                for (int x = 0; x < 4; x++) {
                    newScan_scannedIDs[newScan_storedIDs][x] = TagSerialNumber[x];
                }
                newScan_storedIDs++;
                newScan_lastScanTime = millis();
                lcd.print(char(255));
            }
        }
        // if no card is read
        else {
            // if scan is incomplete and no new tag is read 5 seconds after the last read,
            // abort the scan operation
            if ((millis() - newScan_lastScanTime) > 5000 && newScan_storedIDs > 0) {
                failedNewScan(0);
                resetNewScanVariables();
            }
        }
    }

    // if there are enough stored IDs from RFID Tags
    else {
        // Start checking the scanned IDs for unique entries
        // for each item in newScan_scannedIDs
        for (int scanned = 0; scanned < 16; scanned++) {
            boolean duplicateFound = false;
            // for each item in newScan_uniqueIDs
            for (int unique = 0; unique < newScan_storedUniqueIDs; unique++) {
                int similarity = 0;
                // checks if each character matches and increments similarity
                for (int x = 0; x < 4; x++) {
                    if (newScan_scannedIDs[scanned][x] == newScan_uniqueIDs[unique][x]) {
                        similarity++;
                    }
                }
                // similarity will be 4 when all 4 bytes match, meaning a duplicate is found
                if (similarity == 4) {
                    duplicateFound = true;
                    break;
                }
            }
            // if no duplicate is found, the current scanned ID is a unique entry, and will be stored in newScan_uniqueIDs
            if (!duplicateFound) {
                for (int x = 0; x < 4; x++) {
                    newScan_uniqueIDs[newScan_storedUniqueIDs][x] = newScan_scannedIDs[scanned][x];
                }
                newScan_storedUniqueIDs++;
            }
        }

        // If there's only 1 unique ID, automatically save that as the scanned ID
        if (newScan_storedUniqueIDs == 1) {
            for (int x = 0; x < 4; x++) {
                lastScannedID[x] = newScan_uniqueIDs[0][x];
            }
        }
        // if there's more than 1 unique ID, start checking which one appears more times during the scan
        else {
            // test each unique ID against the stored scanned IDs
            // increments the score of the current unique ID if a match is found
            for (int unique = 0; unique < newScan_storedUniqueIDs; unique++) {
                for (int stored = 0; stored < 16; stored++) {
                    boolean match = true;
                    for (int x = 0; x < 4; x++) {
                        if (newScan_uniqueIDs[unique][x] != newScan_scannedIDs[stored][x]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        newScan_scores[unique]++;
                    }
                }
            }

            // Start checking which item has a higher score
            int highestScore = 0;
            int highestAddress = 0;
            for (int unique = 0; unique < newScan_storedUniqueIDs; unique++) {
                if (newScan_scores[unique] > highestScore) {
                    highestScore = newScan_scores[unique];
                    highestAddress = unique;
                }
            }

            // Save result
            for (int x = 0; x < 4; x++) {
                lastScannedID[x] = newScan_uniqueIDs[highestAddress][x];
            }
        }

        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("Scan Complete");
        buzzerSuccess();
        delay(2500);
        lastPrinted = 9;

        // Sends card data to serial in between Text Start and Text End bytes
        printLastScannedID();
        resetNewScanVariables();
        resetOperationState();
    }
}

// Clears the variables associated with newScan() so that it is reset and ready for another operation
void resetNewScanVariables() {
    newScan_storedIDs = 0;
    newScan_storedUniqueIDs = 0;
    newScan_lastScanTime = 0;

    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 4; y++) {
            newScan_scannedIDs[x][y] = 0;
            newScan_uniqueIDs[x][y] = 0;
        }
        newScan_scores[x] = 0;
    }
}

// Prompt user if the scan has failed
void failedNewScan(int mode) {
    if (lastPrinted != 10) {
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Scan Failed!");
        if (mode == 1) {
            lcd.setCursor(0,1);
            lcd.print("Please try again");
        }
        buzzerError();
        delay(1250);
        lastPrinted = 10;
        resetOperationState();
    }
}

// Challenges the customer with a PIN retrieved from the database
void challenge() {
    // if the passcode is available
    if (pin_passcodeReceived) {
        if (lastPrinted != 12) {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("PIN:");
            lcd.setCursor(7,1);
            lcd.print("[#]DELETE");
            lcd.setCursor(7,0);
            lastPrinted = 12;
        }

        // Stops any keypad sounds
        keypadBeepStop(100);

        // if the current input has not been completed yet
        if (!pin_inputConfirmed) {
            char key = keypad.getKey();
            // if a key is pressed
            if (key) {
                lastKeyPress = millis(); // keeps track of the last time a button is pressed
                // if a number key is pressed
                if (key > 47 && key < 58) {
                    // check if there are adding another digit is still possible
                    // if input length is less than 6, add another digit
                    if (pin_inputCount < 6) {
                        pin_inputCharacters[pin_inputCount] = key;
                        pin_inputCount++;
                        lcd.write(42); // prints an asterisk to the LCD
                        keypadBeepStart(2000);
                    }
                    // if input length is 6, play a buzz and do not add digit to input
                    else {
                        keypadBeepStart(500);
                    }
                }
                // if '#' key is pressed
                else if (key == 35) {
                    // if input is not empty, remove the last character
                    if (pin_inputCount > 0) {
                        lcd.setCursor((pin_inputCount + 6),0);
                        lcd.write(32);
                        lcd.setCursor((pin_inputCount + 6),0);
                        pin_inputCount--;
                        keypadBeepStart(2000);
                    }
                    // if input is not empty
                    else {
                        keypadBeepStart(500);
                    }
                }
                // if '*' is pressed
                else {
                    keypadBeepStart(500);
                }
            }

            if (pin_inputCount == 6 && (millis() - lastKeyPress) > 2000) {
                pin_inputConfirmed = true;
            }
        }
        // if the input has been completed
        else {
            if (pin_retriesLeft > 0) {
                boolean match = true;
                for (int x = 0; x < 6; x++) {
                    if (pin_inputCharacters[x] != pin_passcode.charAt(x)) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    lcd.clear();
                    lcd.setCursor(2,0);
                    lcd.print("Verification");
                    lcd.setCursor(3,1);
                    lcd.print("Succesful");
                    buzzerSuccess();
                    delay(2500);
                    resetPINVariables();
                    resetOperationState();
                    Serial.print(1);
                }
                else {
                    pin_retriesLeft--;
                    lcd.clear();
                    lcd.setCursor(2,0);
                    lcd.print("Invalid PIN");
                    lcd.setCursor(1,1);
                    lcd.print(pin_retriesLeft);
                    lcd.setCursor(3,1);
                    lcd.print("retries left");
                    lastPrinted = 13;

                    pin_inputCount = 0;
                    pin_inputConfirmed = false;

                    buzzerError();
                    delay(2250);

                    if (pin_retriesLeft == 0) {
                        resetPINVariables();
                        resetOperationState();
                        Serial.print(0);
                    }
                }
            }
        }
    }
    // if the passcode is not available
    else {
        if (lastPrinted != 11) {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Fetching account");
            lcd.setCursor(0,1);
            lcd.print("information...");
            lastPrinted = 11;
        }
        
        // if input stream is inactive
        if (!pin_inputStreamActive) {
            // if the stream start character is received
            if (lastReadByte == 2) {
                pin_inputStreamActive = true;
            }
        }
        // if input stream is active
        else {
            // if the last read byte is valid
            if (lastReadByte > 47 && lastReadByte < 58) {
                pin_passcode += (char)lastReadByte;
            }
            else {
                if (lastReadByte == 3 && pin_passcode.length() == 6) {
                    pin_inputStreamActive = false;
                    pin_passcodeReceived = true;
                }
                else if (lastReadByte != 255) {
                    resetPINVariables();
                    resetOperationState();
                    sendByte(140);
                }
            }
            
        }
    }
}

// Clears the variables associated with challenge() and newPIN() so that it is reset and ready for another operation
void resetPINVariables() {
    pin_passcodeReceived = false;
    pin_passcode = "";
    pin_inputStreamActive = false;
    pin_inputCount = 0;
    pin_inputConfirmed = false;
    pin_retriesLeft = 3;
    pin_backConfirmDialogVisible = false;
}

// Asks user to input a new PIN twice, for confirmation
void newPIN() {
    // Stops any keypad sounds
    keypadBeepStop(100);

    if (!pin_passcodeReceived) {
        if (lastPrinted != 14) {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("PIN:");
            lcd.setCursor(0,1);
            lcd.print("[*]OK");
            lcd.setCursor(7,1);
            lcd.print("[#]DELETE");
            lcd.setCursor(7,0);
            lastPrinted = 14;
        }

        // if the current input has not been completed yet
        char key = keypad.getKey();
        // if a key is pressed
        if (key) {
            lastKeyPress = millis(); // keeps track of the last time a button is pressed
            // if a number key is pressed
            if (key > 47 && key < 58) {
                // check if there are adding another digit is still possible
                // if input length is less than 6, add another digit
                if (pin_inputCount < 6) {
                    pin_inputCharacters[pin_inputCount] = key;
                    pin_inputCount++;
                    lcd.write(42); // prints an asterisk to the LCD
                    keypadBeepStart(2000);
                }
                // if input length is 6, play a buzz and do not add digit to input
                else {
                    keypadBeepStart(500);
                }
            }
            // if '#' key is pressed
            else if (key == 35) {
                // if input is not empty, remove the last character
                if (pin_inputCount > 0) {
                    lcd.setCursor((pin_inputCount + 6),0);
                    lcd.write(32);
                    lcd.setCursor((pin_inputCount + 6),0);
                    pin_inputCount--;
                    keypadBeepStart(2000);
                }
                // if input is empty
                else {
                    keypadBeepStart(500);
                }
            }
            // if '*' is pressed
            else {
                // if input length is 6
                if (pin_inputCount == 6) {
                    // Store input as the passcode to be matched
                    for (int x = 0; x < 6; x++) {
                        pin_passcode += pin_inputCharacters[x];
                    }
                    pin_passcodeReceived = true;
                    pin_inputCount = 0;
                    buzzerSuccess();
                }
                // if input length is not 6
                else {
                    keypadBeepStart(500);
                }
            }
        }
    }
    // if passcode is available
    else {
        // if confirm dialog for going back is not visible
        if (!pin_backConfirmDialogVisible) {
            if (lastPrinted != 15) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("CONFIRM:");
                lcd.setCursor(0,1);
                lcd.print("[*]OK");
                lcd.setCursor(7,1);
                lcd.print("[#]DELETE");
                lcd.setCursor(9,0);
                lastPrinted = 15;
            }

            char key = keypad.getKey();
            if (key) {
                lastKeyPress = millis(); // keeps track of the last time a button is pressed
                // if a number key is pressed
                if (key > 47 && key < 58) {
                    // check if there are adding another digit is still possible
                    // if input length is less than 6, add another digit
                    if (pin_inputCount < 6) {
                        pin_inputCharacters[pin_inputCount] = key;
                        pin_inputCount++;
                        lcd.write(42); // prints an asterisk to the LCD
                        keypadBeepStart(2000);
                    }
                    // if input length is 6, play a buzz and do not add digit to input
                    else {
                        keypadBeepStart(500);
                    }
                }
                // if '#' key is pressed
                else if (key == 35) {
                    // if input is not empty, remove the last character
                    if (pin_inputCount > 0) {
                        lcd.setCursor((pin_inputCount + 8),0);
                        lcd.write(32);
                        lcd.setCursor((pin_inputCount + 8),0);
                        pin_inputCount--;
                        keypadBeepStart(2000);
                    }
                    // if input is empty
                    else {
                        pin_backConfirmDialogVisible = true;
                    }
                }
                // if '*' is pressed
                else {
                    // if input length is 6
                    if (pin_inputCount == 6) {
                        boolean match = true;
                        for (int x = 0; x < 6; x++) {
                            if (pin_inputCharacters[x] != pin_passcode.charAt(x)) {
                                match = false;
                                break;
                            }
                        }

                        if (match) {
                            lcd.clear();
                            lcd.setCursor(1,0);
                            lcd.print("PIN Confirmed");
                            lastPrinted = 16;
                            buzzerSuccess();
                            delay(2500);
                            Serial.print(pin_passcode);
                            resetPINVariables();
                            resetOperationState();
                        }
                        else {
                            lcd.clear();
                            lcd.setCursor(2,0);
                            lcd.print("PIN does not");
                            lcd.setCursor(5,1);
                            lcd.print("match");
                            lastPrinted = 17;

                            pin_inputCount = 0;

                            buzzerError();
                            delay(2250);
                        }
                    }
                    // if input length is not 6
                    else {
                        keypadBeepStart(500);
                    }
                }
            }
        }
        // if confirm dialog for going back is visible
        else {
            if (lastPrinted != 18) {
                lcd.clear();
                lcd.setCursor(2,0);
                lcd.print("Change PIN?");
                lcd.setCursor(0,1);
                lcd.print("[*]YES");
                lcd.setCursor(11,1);
                lcd.print("[#]NO");
                lastPrinted = 18;
            }

            char key = keypad.getKey();
            if (key) {
                // if '*' key is pressed
                if (key == 42) {
                    pin_backConfirmDialogVisible = false;
                    pin_passcodeReceived = false;
                    pin_passcode = "";
                    pin_inputCount = 0;
                    keypadBeepStart(2000);
                }
                // if '#' key is pressed
                else if (key == 35) {
                    pin_backConfirmDialogVisible = false;
                    keypadBeepStart(2000);
                }
                // if a number is pressed
                else {
                    keypadBeepStart(500);
                }
            }
        }
        
    }
}

// Tests the connection between the device and the POS
void testConnection() {
    Serial.print(1);
    resetOperationState();
}

// Plays an error tone for 750ms so I don't have to write these couple lines down every single time
void buzzerError()
{
    if (!muteBuzzer) {
        tone(buzzer, 500);
        delay(250);
        noTone(buzzer);
        delay(250);
        tone(buzzer, 500);
        delay(250);
        noTone(buzzer);
    }
}

// Plays success tone for 500ms so I don't have to write these couple lines down every single time
void buzzerSuccess() {
    if (!muteBuzzer) {
        tone(buzzer, 2000);
        delay(500);
        noTone(buzzer);
    }
}

// sends a byte to the Serial monitor
void sendByte(byte data) {
    Serial.write(data);
    lastSentByte = data;
}

// updates the operation state to tell the arduino what function to perform
void updateOperationState() {
    switch (lastReadByte) {
    case 131: // Cancels an operation
        // If the cancelled operation is "newScan()", resets the variables associated with it
        if (operationState == 2) {
            resetNewScanVariables();
        }
        // If the cancelled operation is "challenge()" or "newPIN()", resets the variables associated with it
        else if (operationState == 6 || operationState == 7) {
            resetPINVariables();
        }
        resetOperationState();
        break;
    case 132:
        operationState = 1; // Sets the current task to "scan()"
        break;
    case 133:
        operationState = 2; // Sets the current task to "newScan()"
        break;
    case 134:
        operationState = 3; // Sets the current task to "checkGSM()"
        break;
    case 135:
        operationState = 4; // Sets the current task to "getGSMSignal()"
        break;
    case 136:
        operationState = 5; // Sets the current task to "sendSMS()"
        break;
    case 139:
        operationState = 6; // Sets the current task to "challenge()"
        break;
    case 141:
        operationState = 7; // Sets the current task to "newPIN()"
        break;
    case 142:
        operationState = 8; // Sets the current task to "testConnection()"
        break;
    case 143:
        operationState = 9; // Sets the current task to "checkSIM()"
        break;
    case 144:
        operationState = 10; // Sets the current task to "toggleGSMPower()"
        break;
    case 145:
        operationState = 11; // Sets the current task to "gsmPowerOff()"
        break;
    
    default:
        break;
    }
}

// Resets the operation state to revert back to the splash screen
void resetOperationState() {
    operationState = 0;
}

// Starts the keypad beep
void keypadBeepStart(int frequency) {
    if (beepState == 0 && enableKeypadSounds && !muteBuzzer) {
        beepState = 1;
        tone(buzzer, frequency);
        beepStart = millis();
    }
}

// Stops the keypad beep
void keypadBeepStop(int time) {
    // Stops the keypad beep
    if (enableKeypadSounds && beepState == 1 && (millis() - beepStart) > time) {
        beepState = 0;
        noTone(buzzer);
    }
}

// Prints the last scanned ID
void printLastScannedID() {
    for (int x = 0; x < 4; x++) {
        Serial.write(lastScannedID[x]);
    }
}

boolean toggleGSMPower() {
    //Serial.println("[toggleGSMPower()] checking gsm status..."); // TEMP
    int result = checkGSM();
    if (result == 0 || result == 2) {
        //Serial.println("[toggleGSMPower()] gsm is off. Powering on..."); // TEMP
        digitalWrite(gsmPower,HIGH);
        delay(1500);
        digitalWrite(gsmPower,LOW);
        //Serial.println("[toggleGSMPower()] checking gsm status..."); // TEMP

        result = 0;
        while (result < 2) {
            if (checkGSM() == 1) {
                //Serial.println("[toggleGSMPower()] gsm is now on."); // TEMP
                return true;
            }
            result++;
        }
    }
    else if (result == 1) {
        //Serial.println("[toggleGSMPower()] gsm is already on. Doing nothing..."); // TEMP
        return true;
    }
    //Serial.println("[toggleGSMPower()] could not power on gsm"); // TEMP
    return false;
    
}