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
String lastReadIDSerialNumber = "";

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
    splash(); // Show splash screen when device is idle
    String command = waitForSerialInput(); // Listening for commands sent over Serial monitor
    // valid commands will result in a '1' being sent to Serial. Otherwise, a '100' is sent

    // 'check' commands for testing specific components and connectivity for easier troubleshooting
    // Valid Commands: 'check nfc', 'check gsm'
    // returns 100 if command is invalid
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
        else {
            Serial.println(100);
        }
    }

    // Scan RFID Card and send its Serial number
    else if (command.equals("scan")) {
        scan();
        Serial.println("scannedID=" + lastReadIDSerialNumber);
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

    // Prints '100' when received command is not recognized
    else {
        Serial.println(100);
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
    byte GoodTagSerialNumber[5] = {0x95, 0xEB, 0x17, 0x53}; // The Tag Serial number we are looking for

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

            // it tag data does not start with 0 and 32
            // I've noticed incompletely-read tag data tends to start with 0 and 32 as the firt 2 bytes
            if (!(TagSerialNumber[0] == 0 && TagSerialNumber[1] == 32)) {
                // iterates 4 times to get the first 4 decimal values representing the scanned card's unique ID
                // and converts it into a hex value, then stores it as a string
                for (int i = 0; i < 4; i++)
                {
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
    lastReadIDSerialNumber = stringSerialNumber;
}

// Challenges customer to match a given PIN number
void challenge()
{
    // Wait for 6-digit PIN to arrive in Serial. Make sure that on the Java program will only send a 6-character long string of purely numbers
    String passcode = "";
    while (passcode.length() != 6)
    {
        passcode = Serial.readStringUntil('\n');
    }

    String input = ""; // Temporarily stores the input from the customer
    boolean passcodeMatch = false;

    for (int x = challengeAttempts; x > 0; x--)
    {
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("PIN :");
        lcd.setCursor(8, 0);

        // While the entered PIN length is not complete, wait for more inputs
        // TODO Make an 'OK' and 'BACKSPACE' button
        while (input.length() != 6)
        {
            String tempInput = keypadInput();
            if (!tempInput.equals("*") && !tempInput.equals("#")) {
                input += tempInput;
                lcd.print("*");
            }
        }

        // if entered PIN matches the PIN retrieved from the database
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
        }
    }

    // if passcode matches, prints "ok" to Serial
    // else, prints "no" to Serial
    // the java program should be listening for these values after sending the correct passcode
    Serial.print("challenge=");
    if (passcodeMatch)
    {
        Serial.println(1);
    }
    else
    {
        Serial.println(0);
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
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter PIN:");
    lcd.setCursor(10,0);

    String input1 = ""; // first input
    String input2 = ""; // second input, for verification purposes
    boolean done = false;

    // keep looping until input length is 6 characters, and the "OK" button is pressed
    while (input1.length() != 6 || !done)
    {
        String tempInput = keypadInput(); // waits until the keypad is pressed and returns a character

        // if pressed button is not the "OK" or "backspace" button, add returned charater to the current input and display an asterisk on the LCD
        if (!tempInput.equals("*") && !tempInput.equals("#")) {
            input1 += tempInput;
            lcd.print("*");
        }

        // if "backspace" button is pressed, remove the last character from the current input
        else if (tempInput.equals("#") && tempInput.length() >= 1) {
            char inputCharArray[6];
            input1.toCharArray(inputCharArray, input1.length());
            input1 = String(inputCharArray);

            lcd.setCursor(10,0);
            lcd.print("      ");
            lcd.setCursor(10,0);
            for (int x = 0; x < input1.length(); x++) {
                lcd.print("*");
            }
        }

        // if "ok" button is pressed and PIN length is satisfied, finish the first input of PIN and move on to re-entering for verification
        else if (tempInput.equals("*") && input1.length() == 6) {
            done = true;
            buzzerSuccess();
        }
    }

    done = false;
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Confirm:");
    lcd.setCursor(10,0);

    // keep looping until input length is 6 characters, and the "OK" button is pressed
    while (input2.length() != 6 || !done)
    {
        String tempInput = keypadInput(); // waits until the keypad is pressed and returns a character

        // if pressed button is not the "OK" or "backspace" button, add returned charater to the current input and display an asterisk on the LCD
        if (!tempInput.equals("*") && !tempInput.equals("#")) {
            input2 += tempInput;
            lcd.print("*");
        }

        // if "backspace" button is pressed, remove the last character from the current input
        else if (tempInput.equals("#") && tempInput.length() >= 1) {
            char inputCharArray[6];
            input2.toCharArray(inputCharArray, input2.length());
            input2 = String(inputCharArray);

            lcd.setCursor(10,0);
            lcd.print("      ");
            lcd.setCursor(10,0);
            for (int x = 0; x < input2.length(); x++) {
                lcd.print("*");
            }
        }

        // if "ok" button is pressed and PIN length is satisfied
        else if (tempInput.equals("*") && input2.length() == 6) {
            // if re-entered PIN matches
            if (input1.equals(input2)) {
                done = true;
                lcd.clear();
                lcd.setCursor(1,0);
                lcd.print("PIN Confirmed!");
                buzzerSuccess();
                delay(1500);
            }
            // if re-entered PIN DOES NOT match
            else {
                input2 = "";
                lcd.clear();
                lcd.setCursor(2,0);
                lcd.print("PIN Mismatch");
                lcd.setCursor(0,1);
                lcd.print("please try again");
                buzzerError();
                delay(1250);
                lcd.clear();
                lcd.setCursor(2, 0);
                lcd.print("Confirm:");
                lcd.setCursor(10,0);
            }
        }
    }

    Serial.println("newPIN=" + input1);
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