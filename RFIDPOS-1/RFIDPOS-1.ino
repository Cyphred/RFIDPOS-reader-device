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

  [GSM Module] - https://www.ayomaonline.com/programming/quickstart-sim800-sim800l-with-arduino/
    VCC -> 5V
    GND -> GND
    SIM_TXD -> D8
    SIM_RXD -> D7
*/

#include <Wire.h>              // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD
#include <MFRC522.h>           // RFID Module Library
#include <SPI.h>               // Library for communication via SPI with the RFID Module
#include <Keypad.h>            // Library for Keypad support

#define SDAPIN 10  // RFID Module SDA Pin connected to digital pin
#define RESETPIN 9 // RFID Module RESET Pin connected to digital pin

byte version; // Variable to store Firmware version of the RFID Module
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
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library
MFRC522 nfc(SDAPIN, RESETPIN);                          // Initialization for RFID Reader with declared pinouts for SDA and RESET
const int buzzer = A3;
const int challengeAttempts = 3;
String lastSentData = "";
boolean connectedToJava = false;

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

    send(1);
}

int timesAsked = 0;

void loop()
{
    // if not connected to POS Software
    if (!connectedToJava) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Waiting for");
        lcd.setCursor(0,1);
        lcd.print("connection...");
        String startSignal = waitForSerialInput();
        if  (startSignal.equals("start")) {
            connectedToJava = true;
            lcd.clear();
            lcd.setCursor(3,0);
            lcd.print("Connection");
            lcd.setCursor(2,1);
            lcd.print("Established!");
            buzzerSuccess();
            delay(1500);
            send(2);
        }
        else if (startSignal.equals("sendAgain")) {
            Serial.println(lastSentData);
        }
    }

    // if connected to POS Software
    else {
        splash(); // Show splash screen when device is idle

        // Listening for commands sent over Serial monitor
        // invalid commands will result in a '9' being sent to Serial
        String command = waitForSerialInput();

        // 'check' commands for testing specific components and connectivity for easier troubleshooting
        // Valid Commands: 'check nfc', 'check gsm'
        // returns 100 if command is invalid
        if (command.equals("check")) {
            command = waitForSerialInput();

            // testing RFID Module
            // returns '1' if OK
            // returns '2' if not
            if (command.equals("nfc")) {
                if (checkNFC()) {
                    send(1);
                }
                else {
                    send(0);
                }
            }
            // testing GSM Module
            else if (command.equals("gsm")) {
                command = waitForSerialInput();

                // testing GSM Module
                // returns '1' if OK
                // returns '2' if not
                if (command.equals("status")) {
                    if (checkGSM()) {
                        send(1);
                    }
                    else {
                        send(0);
                    }
                }

                // testing GSM Module signal
                // prints integer value to Serial
                else if (command.equals("signal")) {
                    send(getGSMSignal());
                }
            }
            else {
                send(100);
            }
        }

        // Respond to Java inquiry for connection status
        else if (command.equals("test")) {
            Serial.println(1);
        }

        // Scan RFID Card and send its Serial number
        else if (command.equals("scan")) {
            scan();
        }

        // Use this for scanning new cards into the database for higher read accuracy
        else if (command.equals("newscan")) {
            newCardScan();
        }

        else if (command.equals("challenge")) {
            challenge();
        }

        // GSM commands for sending SMS
        else if (command.equals("gsm")) {
            sendSMS();
        }

        else if (command.equals("newpass")) {
            newPINInput();
        }

        // Prints the last scanned ID to serial monitor for future retrieval in case Java program does not receive complete data
        else if (command.equals("sendAgain")) {
            Serial.println(lastSentData);
        }

        // Prints '9' when received command is not recognized
        else {
            Serial.println(9); // Note: DO NOT use send() for sending error code 9
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

// Prints splash screen to LCD
void splash() {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RFID POS SCANNER");
    lcd.setCursor(6,1);
    lcd.print("v1.0");
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
void scan()
{
    byte FoundTag;                                          // value to tell if a tag is found
    byte ReadTag;                                           // Anti-collision value to read tag information
    byte TagData[MAX_LEN];                                  // full tag data
    byte TagSerialNumber[5];                                // tag serial number

    // Prompts customer to scan their RFID card
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Place your card");
    lcd.setCursor(0,1);
    lcd.print("near the scanner");

    String stringSerialNumber = ""; // String to temporarily store converted tag data
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
            stringSerialNumber = "";

            // if tag data does not start with 0 and 32
            // I've noticed incompletely-read tag data tends to start with 0 and 32 as the firt 2 bytes
            if (!(TagSerialNumber[0] == 0 && TagSerialNumber[1] == 32)) {
                // iterates 4 times to get the first 4 decimal values representing the scanned card's unique ID
                // and converts it into a hex value, then stores it as a string
                for (int i = 0; i < 4; i++)
                {
                    // FIXME returning incorrect card IDs
                    // if the decimal value is lesser than 16, it is going to start with a zero
                    // Casting into a String does not retain that zero, so it must be added manually
                    if (TagSerialNumber[i] < 16) {
                        stringSerialNumber += 0;
                    }
                    stringSerialNumber += String(TagSerialNumber[i], HEX);
                }

                stringSerialNumber.toUpperCase(); // Convert retrieved Serial Number to all uppercase
            }
        }
    }

    // Notifies user of successful scan through displayed text on the LCD and a beep
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Card Scanned");
    buzzerSuccess();
    send(stringSerialNumber);
}

// Used for scanning new cards
// This process takes longer than a normal scan as it does multiple passes to ensure that the correct card information is read
// It will make mutiple scans and compare them to increase accuracy
void newCardScan() {
    byte FoundTag;                                          // value to tell if a tag is found
    byte ReadTag;                                           // Anti-collision value to read tag information
    byte TagData[MAX_LEN];                                  // full tag data
    byte TagSerialNumber[5];                                // tag serial number

    // Prompts customer to scan their RFID card
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Scan Progress:");
    lcd.setCursor(0,1);

    String scannedIDs[16]; // will store 16 sets of 4 bytes each, representing the scanned card's UID
    int storedBytes = 0; // counts the number of bytes stored in the array

    // keep looping until the card has been scanned 16 times
    while (storedBytes != 16) {
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
                lcd.print("*");
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
}

// Challenges customer to match a given PIN number
void challenge()
{
    // Wait for 6-digit PIN to arrive in Serial. Make sure that on the Java program will only send a 6-character long string of purely numbers
    String passcode = "";
    while (passcode.length() != 6)
    {
        passcode = Serial.readStringUntil('\n');
        if (passcode.length() != 6) {
            Serial.println(9);
        }
    }

    String input = ""; // Temporarily stores the input from the customer
    boolean done = false;

    // loop for a maximum of the specified times in challengeAttempts
    for (int x = challengeAttempts; x > 0; x--)
    {
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("PIN :");
        lcd.setCursor(0, 1);
        lcd.print("[*]OK");
        lcd.setCursor(9, 1);
        lcd.print("[#]BACK");
        lcd.setCursor(8, 0);
        
        // While the entered PIN length is not complete, wait for more inputs
        while (input.length() != 6 || !done)
        {
            String tempInput = keypadInput(); // waits until the keypad is pressed and returns a character

            // if the pressed button is not the "OK" or "Backspace" button, add returned charater to the current input and display an asterisk on the LCD
            if (!tempInput.equals("*") && !tempInput.equals("#")) {
                // if input is not 6 characters long, keep adding to input
                // otherwise, play short error tone
                if (input.length() < 6) {
                    input += tempInput;
                    lcd.print("*");
                }
                else {
                    buzzerQuickError();
                }
            }

            // if "Backspace" button is pressed, remove the last character from the current input
            else if (tempInput.equals("#")) {
                // if input is not 0 characters long, keep deleting the last input
                // otherwise, play short error tone
                if (input.length() >= 1) {
                    char inputCharArray[6];
                    input.toCharArray(inputCharArray, input.length());
                    input = String(inputCharArray);

                    lcd.setCursor(8,0);
                    lcd.print("      ");
                    lcd.setCursor(8,0);
                    for (int x = 0; x < input.length(); x++) {
                        lcd.print("*");
                    }
                }
                else {
                    buzzerQuickError();
                }
            }

            // if "ok" button is pressed, check if PIN length is satisfied.
            // Proceed to checking if length is good.
            // Otherwise, play short error tone
            else if (tempInput.equals("*")) {
                if (input.length() == 6) {
                    done = true;
                }
                else {
                    buzzerQuickError();
                }
                
            }
        }

        // if entered PIN matches the PIN retrieved from the database
        if (passcode.equals(input))
        {
            lcd.clear();
            lcd.setCursor(2, 0);
            lcd.print("Verification");
            lcd.setCursor(3, 1);
            lcd.print("Successful");
            delay(2000);
            send(1);
            break;
        }

        // if entered PIN DOES NOT match the PIN retrieved from the database
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
            done = false;
        }
    }

    // if passcode matches, prints "1" to Serial
    // else, prints "0" to Serial
    // the java program should be listening for these values after sending the correct passcode
    if (!passcode.equals(input))
    {
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

// Asks customer to input a new PIN twice, for confirmation
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

// saves data before sending it to the Serial monitor in case it is requsted again
void send(String data) {
    Serial.println(data);
    lastSentData = data;
}

// saves data before sending it to the Serial monitor in case it is requsted again
void send(int data) {
    Serial.println(data);
    lastSentData = data;
}