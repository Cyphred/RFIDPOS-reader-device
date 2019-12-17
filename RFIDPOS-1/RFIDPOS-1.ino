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
    1 - ROW1 -> D0
    2 - ROW2 -> A2
    3 - COL2 -> D2
    4 - ROW3 -> D3
    5 - COL0 -> D4
    6 - ROW0 -> D5
    7 - COL1 -> D6

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
byte rowPins[keypadRows] = {5,0,A2,3}; // Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte colPins[keypadCols] = {4,6,2};  // Connect keypad COL0, COL1 and COL2 to these Arduino pins.
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, keypadRows, keypadCols); // Initializes the keypad. To get the char from the keypad, char key = keypad.getKey(); then if (key) to check for a valid key

// LCD declarations
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library

// Buzzer declarations
const int buzzer = A3;

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

void setup()
{
    SPI.begin();
    gsmSerial.begin(19200);
    Serial.begin(115200);
    pinMode(buzzer, OUTPUT);

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.noCursor();

    // Initialize RFID Module
    nfc.begin();
    version = nfc.getFirmwareVersion();
    
    sendByte(128); // Signals the POS that the device is ready to initiate a connection
}

int lastPrinted = 0; // An identifier for different LCD messages to prevent screen flickering
/*
    ID list
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
*/

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
            if (checkGSM()) {
                Serial.println(1);
            }
            else {
                Serial.println(0);
            }
            resetOperationState();
            break;

        case 4:
            Serial.print(getGSMSignal());
            resetOperationState();
            break;

        case 5:
            sendSMS();
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

            sendByte(180); // Tells POS to reestablish a connection
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
            sendByte(130);
        }
    }
}

// Waits for any data to arrive via serial and returns it as a String
String waitForSerialInput() {
    String input = "";
    while (input.length() == 0) {
        input = Serial.readStringUntil('\n');
    }
    return input;
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

boolean checkGSM()
{
    gsmSerial.println("AT");
    int returnValue = 2;
    int readBytes = 0;

    while (returnValue == 2) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            readBytes++;
            if (readBytes > 6) {
                if (readByte == 79) {
                    returnValue = 1;
                }
                else {
                    returnValue = 0;
                }
            }
        }
    }

    if (returnValue == 1) {
        return true;
    }
    return false;
}

// Gets GSM Signal Quality
int getGSMSignal() {
    gsmSerial.println("AT+CSQ\r");
    int returnValue = -1;
    int readBytes = 0;
    String temp = "";

    while (returnValue == -1) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            readBytes++;
            if (readBytes > 17) {
                if (readByte != 44) {
                    temp += (char)readByte;
                }
                else
                {
                    break;
                }
                
            }
        }
    }

    return temp.toInt();
}

// Sends an SMS with the GSM Module. Returns true if sending is successful
void sendSMS() {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Sending SMS");

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
                if (number.length() != 10) {
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

    // AT command to set gsmSerial to SMS mode
    gsmSerial.print("AT+CMGF=1\r"); 
    delay(100);
    gsmSerial.println("AT + CMGS = \"" + number + "\""); 
    delay(100);
    gsmSerial.println(message); 
    delay(100);
    // End AT command with a ^Z, ASCII code 26
    gsmSerial.println((char)26); 
    delay(100);
    gsmSerial.println();
    // Give module time to send SMS
    delay(5000); 
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

        sendByte(178); // Start a data stream
        // Send 4 bytes representing the card's unique ID
        for (int x = 0; x < 4; x++) {
            sendByte(TagSerialNumber[x]);
        }
        sendByte(179); // End the data stream
        operationState = 0;
    }
}

// Used for scanning new cards
// This process takes longer than a normal scan as it does multiple passes to ensure that the correct card information is read
// It will make mutiple scans and compare them to increase accuracy
void newScan() {
    // FIXME Fails after cancelling
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
            }
        }
    }

    // if there are enough stored IDs from RFID Tags
    else {
        
    }
}

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

// Used for scanning new cards
// This process takes longer than a normal scan as it does multiple passes to ensure that the correct card information is read
// It will make mutiple scans and compare them to increase accuracy
void old_newCardScan() {
    boolean validIDAvailable = false;
    byte FoundTag;                                          // value to tell if a tag is found
    byte ReadTag;                                           // Anti-collision value to read tag information
    byte TagData[MAX_LEN];                                  // full tag data
    byte TagSerialNumber[5];                                // tag serial number

    String scannedIDs[16]; // will store 16 sets of 4 bytes each, representing the scanned card's UID
    int storedBytes = -1; // counts the number of bytes stored in the array

    while (!validIDAvailable) {
        // keep looping until the card has been scanned 16 times
        while (storedBytes != 16) {
            if (storedBytes == -1) {
                // Prompts user to scan their RFID card
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Hold your card");
                lcd.setCursor(0,1);
                lcd.print("near the scanner");
                storedBytes = 0;
            }

            // Check if a tag was detected
            // If yes, then variable FoundTag will contain "MI_OK"
            FoundTag = nfc.requestTag(MF1_REQIDL, TagData);

            if (FoundTag == MI_OK) {
                if (storedBytes == 0) {
                    lcd.clear();
                    lcd.setCursor(4,0);
                    lcd.print("Scanning");
                    lcd.setCursor(0,1);
                }

                delay(100);
                ReadTag = nfc.antiCollision(TagData); // Get anti-collision value to properly read information from the tag
                memcpy(TagSerialNumber, TagData, 4);  // Writes the tag info in TagSerialNumber

                // if tag data does not start with 0 and 32
                // I've noticed incompletely-read tag data tends to start with 0 and 32 as the firt 2 bytes
                if (!(TagSerialNumber[0] == 0 && TagSerialNumber[1] == 32)) {
                    String stringSerialNumber = "";
                    // iterates 4 times to get the first 4 decimal values representing the scanned card's unique ID
                    // and converts it into a hex value, then stores it as a string
                    for (int x = 0; x < 4; x++) {
                        if (TagSerialNumber[x] < 16) {
                            stringSerialNumber += 0;
                        }
                        stringSerialNumber += String(TagSerialNumber[x], HEX);
                    }

                    // Stores the new string to the list of 16 scanned IDs and prints to the progress bar
                    scannedIDs[storedBytes] = stringSerialNumber;
                    storedBytes++;
                    lcd.print(char(255));
                }
            }
        }

        String uniqueIDs[16]; // keeps track of all unique IDs scanned
        int storedUniqueIDs = 0; // keeps track the actual number unique IDs scanned

        // for each scanned ID...
        for (String sid: scannedIDs) {
            boolean duplicateFound = false;
            // checks if it matches an already recognized unique ID
            for (String s: uniqueIDs) {
                if (sid.equals(s)) {
                    duplicateFound = true;
                    break;
                }
            }
            // if no duplicate is found, add to list of unique IDs and increment Unique ID counter
            if (!duplicateFound) {
                uniqueIDs[storedUniqueIDs] = sid;
                storedUniqueIDs++;
            }
        }

        int scores[storedUniqueIDs]; // keeps track of how many times each unique ID has appeared during the 16 passes of scanning
        // for each stored unique ID, set the default score of zero
        for (int x = 0; x < storedUniqueIDs; x++) {
            scores[x] = 0;
        }

        // for each stored unique ID...
        for (int x = 0; x < storedUniqueIDs; x++) {
            // compare it with each unique ID and increment the score for the corresponding ID
            for (String sid: scannedIDs) {
                if (uniqueIDs[x].equals(sid)) {
                    scores[x]++;
                }
            }
        }

        // checks for ties in scoring
        boolean tieFound = false;
        for (int x = 0; x < storedUniqueIDs; x++) {
            for (int y = 0; y < storedUniqueIDs; y++) {
                if (x != y) {
                    if (scores[x] == scores[y] && (scores[x] + scores[y]) != 0) {
                        tieFound = true;
                        break; // breaks inner loop
                    }
                }
            }

            if (tieFound) {
                break; // breaks outer loop
            }
        }

        // if there are no ties found, start looking for the highest-scoring ID
        if (!tieFound) {
            int highestScore = 0; // keeps track of the highest score
            String bestMatch; // stores the actual ID with the highest score
            for (int x = 0; x < storedUniqueIDs; x++) {
                if (scores[x] > highestScore) {
                    highestScore = scores[x];
                    bestMatch = uniqueIDs[x];
                }
            }

            lcd.clear();
            lcd.setCursor(1,0);
            lcd.print("Scan Complete!");
            buzzerSuccess();
            delay(1500);

            bestMatch.toUpperCase();
            send(bestMatch);
            validIDAvailable = true;
        }
        // if there are ties found, retry the scan
        else {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("Scan Failed!");
            lcd.setCursor(0,1);
            lcd.print("Please try again");
            buzzerError();
            delay(1250);
            storedBytes = -1;
            storedUniqueIDs = 0;
        }
    }
}


// TODO Document this method
void challenge(String passcodeString) {
    char passcode[6];
    for (int x = 0; x < 6; x++) {
        passcode[x] = passcodeString.charAt(x);
    }

    boolean result = false;

    for (int x = challengeAttempts; x > 0; x--) {
        char input[6];
        int storedInputs = 0;
        unsigned long lastKeyPress;
        boolean timedOut = false;

        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("PIN :");
        lcd.setCursor(3, 1);
        lcd.print("[#]DELETE");
        lcd.setCursor(8, 0);

        while (!timedOut) {
            char key = keypad.getKey(); // Get key from keypad
            // if a key is pressed
            if (key) {
                if (key == '#') {
                    if (storedInputs > 0) {
                        storedInputs--;
                        lcd.setCursor(8, 0);
                        lcd.print("      ");
                        lcd.setCursor(8, 0);
                        for (int y = 0; y < storedInputs; y++) {
                            lcd.print("*");
                        }
                    }
                    else {
                        buzzerQuickError();
                    }
                }
                else if (key == '*') {
                    buzzerQuickError();
                }
                // if a number is pressed
                else {
                    lastKeyPress = millis();
                    // if the number of stored inputs is not 6 yet
                    if (storedInputs != 6) {
                        input[storedInputs] = key;
                        storedInputs++;
                        lcd.print("*");
                    }
                    else {
                        buzzerQuickError();
                    }
                }
            }

            // if there are 6 digits in input and 2 seconds have passed since the last keypress
            if (storedInputs == 6 && (millis() - lastKeyPress) == 2000) {
                timedOut = true;
            }
        }
        
        boolean match = true;
        for (int y = 0; y < 6; y++) {
            if (input[y] != passcode[y]) {
                match = false;
                break;
            }
        }

        if (match) {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("Verification");
            lcd.setCursor(3,1);
            lcd.print("Successful");
            buzzerSuccess();
            delay(1500);

            result = true;
            break;
        }
        else {
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("Invalid PIN");
            lcd.setCursor(1,1);
            lcd.print(x-1);
            lcd.setCursor(3,1);
            lcd.print("retries left");
            buzzerError();
            delay(1250);
        }
    }

    if (result) {
        send(1);
    }
    else {
        send(0);
    }
}

// Waits for a key on the keypad to be pressed before returning the pressed number
String keypadInput() {
    String returnValue = "";
    while (returnValue.length() != 1) {
        char key = keypad.getKey();
        if (key) {
            returnValue += key;
        }
    }
    return returnValue;
}

// Asks user to input a new PIN twice, for confirmation
void newPINInput() {
    String inputs[2] = {"",""};
    boolean redo = false;
    do {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter PIN:");
        lcd.setCursor(0, 1);
        lcd.print("[*]OK");
        lcd.setCursor(9, 1);
        lcd.print("[#]BACK");
        lcd.setCursor(10,0);

        boolean done = false;

        // keep looping until input length is 6 characters, and the "OK" button is pressed
        while (inputs[0].length() != 6 || !done)
        {
            String tempInput = keypadInput(); // waits until the keypad is pressed and returns a character

            // if pressed button is not the "OK" or "backspace" button, add returned charater to the current input and display an asterisk on the LCD
            if (!tempInput.equals("*") && !tempInput.equals("#")) {
                // if input is not 6 characters long, keep adding to input
                // otherwise, play short error tone
                if (inputs[0].length() < 6) {
                    inputs[0] += tempInput;
                    lcd.print("*");
                }
                else {
                    buzzerQuickError();
                }
            }

            // if "backspace" button is pressed, remove the last character from the current input
            else if (tempInput.equals("#")) {
                // if input is not 0 characters long, keep deleting the last input
                // otherwise, play short error tone
                if (inputs[0].length() >= 1) {
                    char inputCharArray[6];
                    inputs[0].toCharArray(inputCharArray, inputs[0].length());
                    inputs[0] = String(inputCharArray);

                    lcd.setCursor(10,0);
                    lcd.print("      ");
                    lcd.setCursor(10,0);
                    for (int x = 0; x < inputs[0].length(); x++) {
                        lcd.print("*");
                    }
                }
                else {
                    buzzerQuickError();
                }
            }

            // if "ok" button is pressed, check if PIN length is satisfied.
            // if length is good, finish the first input of PIN and move on to re-entering for verification
            // Otherwise, play short error tone
            else if (tempInput.equals("*")) {
                if (inputs[0].length() == 6) {
                    done = true;
                    buzzerSuccess();
                }
                else {
                    buzzerQuickError();
                }
                
            }
        }

        done = false;
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("Confirm:");
        lcd.setCursor(0, 1);
        lcd.print("[*]OK");
        lcd.setCursor(9, 1);
        lcd.print("[#]BACK");
        lcd.setCursor(10,0);

        // keep looping until input length is 6 characters, and the "OK" button is pressed
        while (inputs[1].length() != 6 || !done)
        {
            String tempInput = keypadInput(); // waits until the keypad is pressed and returns a character

            // if pressed button is not the "OK" or "backspace" button, add returned charater to the current input and display an asterisk on the LCD
            if (!tempInput.equals("*") && !tempInput.equals("#")) {
                // if input is not 6 characters long, keep adding to input
                // otherwise, play short error tone
                if (inputs[1].length() < 6) {
                    inputs[1] += tempInput;
                    lcd.print("*");
                }
                else {
                    buzzerQuickError();
                }
            }

            // if "backspace" button is pressed, remove the last character from the current input
            else if (tempInput.equals("#")) {
                // if input is not 0 characters long, keep deleting the last input
                // otherwise, play short error tone
                if (inputs[1].length() >= 1) {
                    char inputCharArray[6];
                    inputs[1].toCharArray(inputCharArray, inputs[1].length());
                    inputs[1] = String(inputCharArray);

                    lcd.setCursor(10,0);
                    lcd.print("      ");
                    lcd.setCursor(10,0);
                    for (int x = 0; x < inputs[1].length(); x++) {
                        lcd.print("*");
                    }
                }
                else {
                    // Asks the user if they want to go back and change the PIN before confirming
                    lcd.clear();
                    lcd.setCursor(4,0);
                    lcd.print("Go back?");
                    lcd.setCursor(1,1);
                    lcd.print("[*]Yes");
                    lcd.setCursor(10,1);
                    lcd.print("[#]No");

                    // Response will be 1 for yes, 0 for no
                    // if any other key is pressed, play a short error tone
                    int response = -1;
                    while (response == -1) {
                        String tempInput = keypadInput(); // Wait for an input
                        if (tempInput.equals("#")) {
                            response = 0;
                        }
                        else if (tempInput.equals("*")) {
                            response = 1;
                        }
                        else {
                            buzzerQuickError();
                        }
                    }

                    if (response == 1) {
                        inputs[1] = "xxxxxx";
                        done = true;
                        redo = true;
                    }
                }
            }

            // if "ok" button is pressed, check if PIN length is satisfied.
            // Proceed to checking if length is good.
            // Otherwise, play short error tone
            else if (tempInput.equals("*")) {
                if (inputs[1].length() == 6) {
                    // if re-entered PIN matches
                    if (inputs[0].equals(inputs[1])) {
                        done = true;
                        redo = false;
                        lcd.clear();
                        lcd.setCursor(1,0);
                        lcd.print("PIN Confirmed!");
                        buzzerSuccess();
                        delay(1500);
                    }
                    // if re-entered PIN DOES NOT match
                    else {
                        inputs[1] = "";
                        lcd.clear();
                        lcd.setCursor(2,0);
                        lcd.print("PIN Mismatch");
                        lcd.setCursor(0,1);
                        lcd.print("please try again");
                        buzzerError();
                        delay(1250);
                        lcd.clear();
                        lcd.setCursor(2,0);
                        lcd.print("Confirm:");
                        lcd.setCursor(0,1);
                        lcd.print("[*]OK");
                        lcd.setCursor(9, 1);
                        lcd.print("[#]BACK");
                        lcd.setCursor(10,0);
                    }
                }
                else {
                    buzzerQuickError();
                }
            }
        }

        if (redo) {
            inputs[0] = "";
            inputs[1] = "";
            done = false;
        }
    } while (redo);
    send(inputs[0]);
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

// Plays a brief error tone for 250ms so I don't have to write these couple lines down every single time
void buzzerQuickError()
{
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

// TODO Remove this and its references
// saves data before sending it to the Serial monitor in case it is requsted again
void send(String data) {
    Serial.println(data);
    lastSentData = data;
}

// TODO Remove this and its references
// saves data before sending it to the Serial monitor in case it is requsted again
void send(int data) {
    Serial.println(data);
    lastSentData = data;
}

void sendByte(byte data) {
    Serial.write(data);
    lastSentByte = data;
}

void updateOperationState() {
    switch (lastReadByte) {
    case 131:
        operationState = 0;
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
        
    
    default:
        break;
    }
}

void resetOperationState() {
    operationState = 0;
}

boolean matchID(byte id1[4], byte id2[4]) {
    for (int x = 0; x < 4; x++) {
        if (id1[x] != id2[x]) {
            return false;
        }
    }
    return true;
}

String waitForDataStream(byte startMarker, byte endMarker, int length) {

}