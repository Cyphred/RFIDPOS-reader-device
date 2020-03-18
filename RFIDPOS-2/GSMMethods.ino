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
byte recipientNumber[12];

// Toggles the power switch of the GSM module
void toggleGSMPower() {
    digitalWrite(gsmPowerPin,HIGH);
    delay(1000);
    digitalWrite(gsmPowerPin,LOW);
}

// Checks the GSM module's status
// Prints an integer with a corresponding state
boolean checkGSM(int timeoutDuration) {
    gsmSerial.print("AT\r"); // Sends AT command to check GSM status
    timeoutStart = millis(); // Mark the start of the timeout
    resetBufferEnding(); // Clear the current contents of the buffer ending
    
    // Wait for the set time in milliseconds before timing out
    while((millis() - timeoutStart) < timeoutDuration) {
        while (gsmSerial.available()) {
            addByteToBufferEnding(gsmSerial.read());
        }

        // If the buffer ending of "OK" has been received
        if (bufferEnding[1] == 'K') {
            return true;
        }
    }

    return false;
}

int checkSIM() {
    gsmSerial.print("AT+CPIN?\r");
    timeoutStart = millis(); // Mark the start of the timeout
    resetBufferEnding(); // Clear the current contents of the buffer ending

    // Wait for 100ms before timing out
    while ((millis() - timeoutStart) < 100) {
        
    }

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

// Check if the SMS mode is ready
boolean SMSMode() {
    gsmSerial.print("AT+CMGF=1\r"); // Sets the GSM Module to SMS Mode
    timeoutStart = millis(); // Mark the start of the timeout
    resetBufferEnding(); // Clear the current contents of the buffer ending

    // Wait for 100ms before timing out
    while((millis() - timeoutStart) < 100) {
        while (gsmSerial.available()) {
            addByteToBufferEnding(gsmSerial.read());
        }

        // If the buffer ending of "OK" has been received
        if (bufferEnding[1] == 'K') {
            return true;
        }
    }

    return false;
}

boolean SMSSend() {
    // Prompt the user that the SMS is sending
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Sending SMS");

    // If changing to SMS mode is successful
    if (SMSMode()) {
        // Set the recipient's number
        gsmSerial.print("AT+CMGS=\"+");
        gsmSerial.print(recipientNumber); // TODO recheck if this actually prints the whole thing
        gsmSerial.print("\"\r");

        timeoutStart = millis(); // Mark the start of the timeout
        resetBufferEnding(); // Clear the current contents of the buffer ending

        // Wait for 100ms before timing out
        while((millis() - timeoutStart) < 100) {
            while (gsmSerial.available()) {
                addByteToBufferEnding(gsmSerial.read());
            }

            // If the buffer ending contains the OK signal for sending an SMS
            if (bufferEnding[1] == '>') {
                flushGSMSerial(); // Flush the remaining data out of the GSM serial
                parseSMSTemplate(); // Start printing the SMS template to the GSM module
                flushGSMSerial(); // Flush the remaining data out of the GSM serial
                gsmSerial.write(26); // Start sending the SMS

                resetBufferEnding(); // Clear the current contents of the buffer ending

                // Wait for a response (This will either be OK or ERROR)
                // WARINING This will be stuck forever if the GSM module does not respond
                while(true) {
                    while (gsmSerial.available()) {
                        addByteToBufferEnding(gsmSerial.read());
                    }

                    // If the buffer ending of "OK" has been received
                    if (bufferEnding[1] == 'K') {
                        return true;
                    }
                    // If the buffer ending of "ERROR" has been received
                    else if (bufferEnding[1] == 'R') {
                        break;
                    }
                }

                // If this part is reached, this means the SMS could not be sent
                flushGSMSerial();
                break;
            }
            // If the GSM module prints ERROR
            else if (bufferEnding[1] == 'R') {
                flushGSMSerial(); // Flush the remaining data out of the GSM serial
                break; // Breaks out of the loop and prompts sending error
            }
        }

        // If the while loop above is exited, means the sending has failed, and will proceed with the prompt
    }
    
    // Prompt the user that the SMS could not be sent
    // This part would not be reached if the sending has succeeded
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Sending Failed");
    buzzerError();
    delay(1250);
    return false;
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

// Goes into a manual AT command debug mode
void ATDebugMode() {
    lcd.clear();
    lcd.print("AT Debug Mode");

    while (true) {
        byte readByte = Serial.read();

        // If the exit byte is received
        if (readByte == 128) {
            break;
        }

        if (readByte != 255) {
            gsmSerial.write(readByte);
        }

        if (gsmSerial.available()) {
            Serial.write(gsmSerial.read());
        }
    }
}

// Clears the remaining data in the GSM serial buffer
void flushGSMSerial() {
    while (gsmSerial.available()) {
        gsmSerial.read();
    }
}