/**
 * GSMMethods
 * Will contain all GSM-related methods and variables
 */

// SMS fields
uint32_t purchaseAmount_whole;
uint32_t purchaseAmount_decimal;
uint32_t availableBalance_whole;
uint32_t availableBalance_decimal;
uint32_t transactionID;
uint32_t purchaseUnixtime;

// Toggles the power switch of the GSM module
void toggleGSMPower() {
    digitalWrite(gsmPowerPin,HIGH);
    delay(1000);
    digitalWrite(gsmPowerPin,LOW);
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

// Saves an SMS template to the EEPROM
// This will have fields that will be parsed and included in the SMS
void setSMSTemplate() {
    // Keeps track of what EEPROM address to write to
    // Starts with address 25 to accomodate for the 24-character store name (address 0-23)
    // and the end marker which will be at address 24 if the store name reaches the 24-character limit
    writeSerialDataToEEPROM(25);
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