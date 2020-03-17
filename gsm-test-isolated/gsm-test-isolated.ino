#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <TimeLib.h>

SoftwareSerial gsmSerial(8,7);
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library

uint32_t purchaseAmount_whole;
uint32_t purchaseAmount_decimal;
uint32_t availableBalance_whole;
uint32_t availableBalance_decimal;
uint32_t transactionID;
uint32_t purchaseUnixtime;

String gsmSerialTemp = "";

/*
    NOTES
    - Addresses 0 to 23 are reserved for the store name

*/

void setup() {
    Serial.begin(9600);
    gsmSerial.begin(19200);

    lcd.init();
    lcd.backlight();
    lcd.noCursor();

    lcd.clear();
    int gsmState = checkGSM();
    if (gsmState == 1) {
        lcd.print("GSM Ready");
    }
    else {
        lcd.print("GSM not ready");
    }
    delay(2000);
    lcd.clear();
}

byte namePrint = 0;

void loop() {
    while (Serial.available()) {
        byte readByte = Serial.read();

        if (readByte != 255 && readByte != 13 && readByte != 10) {
            switch (readByte) {
                case 147:
                    setStoreName();
                    namePrint = 0;
                    break;
                case 148:
                    setPurchaseUnixTime();
                    break;
                case 149:
                    setSMSTemplate();
                    break;
                case 150:
                    setPurchaseAmount();
                    break;
                case 151:
                    setAvailableBalance();
                    break;
                case 152:
                    setTransactionID();
                    break;
                case 64:
                    parseSMSTemplate_debug();
                    break;
            }
        }
    }
}

int checkGSM() {
    // Sends AT command to check GSM status
    gsmSerial.print("AT\r");
    // by default, return value will be '2' for timed out
    int returnValue = 2;
    // temp to temporarily store the bytes received as responses from the gsm module
    gsmSerialTemp = "";
    // The start of timeouts
    unsigned long timeoutStart;

    // Starts keeping track of time to wait for a response before timing out
    timeoutStart = millis();
    while (returnValue == 2 && (millis() - timeoutStart) < 5000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 10 || readByte != 13) {
                gsmSerialTemp += (char)readByte;
            }
        }

        if (gsmSerialTemp.indexOf("OK") >= 0) {
            returnValue = 1;
            break;
        }
        else if (gsmSerialTemp.indexOf("ERROR") >= 0) {
            returnValue = 0;
            break;
        }
    }

    gsmSerialTemp = ""; // Clears memory used by gsmSerialTemp
    return returnValue;
}

// Changes the stored store name
void setStoreName() {
    writeSerialDataToEEPROM(0);
}

// Queries the length of the store's name
int getStoreNameLength() {
    int length = 0; // Keeps track of the counted bytes in the allocated space for the store's name

    // Iterate 24 times MAX
    for (int x = 0; x < 24; x++) {
        byte readByte = EEPROM.read(x); // Read the current EEPROM address
        
        // If the store name end marker is reached (a forward slash) end the loop
        if (readByte == 3) {
            break;
        }
        // If the end marker is not reached yet, increment the length
        else {
            length++;
        }
    }

    return length;
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

// Parse SMS template and pass it to GSM module
void parseSMSTemplate_debug() {
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
    Serial.println(dateBuffer); // Print the date and time to the SMS

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
                        Serial.print(purchaseAmount_whole);
                        Serial.print('.');
                        Serial.print(purchaseAmount_decimal);
                        break;

                    // Store Name
                    case 66:
                        for (int x = 0; x < 24; x++) {
                            byte readName = EEPROM.read(x);
                            if (readName == 3) {
                                break;
                            }
                            else {
                                Serial.write(readName);
                            }
                        }
                        break;

                    // Available Balance
                    case 67:
                        Serial.print(availableBalance_whole);
                        Serial.print('.');
                        Serial.print(availableBalance_decimal);
                        break;

                    // Transaction ID
                    case 68:
                        Serial.print(transactionID);
                        break;
                }
            }
            // If a regular character is received, send it to the GSM
            else {
                Serial.write(readByte);
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