/**
 * GSMMethods
 * @author Jeremy Andrews "Cyphred" Zantua
 * Will contain all GSM-related methods and variables
 */

/**
 * Toggles the power switch of the GSM module
 */
void toggleGSMPower() {
    digitalWrite(gsmPowerPin,HIGH);
    delay(1000);
    digitalWrite(gsmPowerPin,LOW);
}

/**
 * Checks the GSM module's status
 * @return is true if the GSM module is responsive
 */
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

String messageData = "";
byte MessageDataBytes = 0;
/**
 * Waits for the recipient number and message data, then sends an SMS
 * @return is the completion status of sending the SMS
 */
// Returns 0 if sending failed
// Returns 1 if sending succeeded
// Returns 2 if an error occurs
byte sendSMS() {
    boolean SMSDebugMode = false; // Debugs to LCD
    // The SMS-sending routine will be broken up into several stages which I will label
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Sending SMS...");
    lcd.setCursor(0,1);
    lcd.print("Enabling SMS");

    // Stage 1
    // Set the GSM module to SMS sending mode
    gsmSerial.print("AT+CMGF=1\r");

    timeoutStart = millis(); // Mark the start of the timeout
    resetBufferEnding(); // Clear the current contents of the buffer ending
    boolean readyState = false; // Multipurpose variable to indicate the success of a step

    // Wait for 100ms before timing out
    while ((millis() - timeoutStart) < 1000) {
        while (gsmSerial.available()) {
            addByteToBufferEnding(gsmSerial.read());

            // If the response is 'OK'
            if (bufferEnding[0] == 'O' && bufferEnding[1] == 'K') {
                flushGSMSerial();
                readyState = true;
                break;
            }
            // If the response is 'ERROR'
            else if (bufferEnding[0] == 'O' && bufferEnding[1] == 'R') {
                flushGSMSerial();
                // TEMP
                lcd.clear();
                lcd.print("SMS Mode Fail");
                return 2;
            }
        }

    }

    // Stage 2
    // Set the mobile number of the recipient
    if (readyState) {
        readyState = false;
        clearLCDRow(1);
        lcd.print("Query recipient");
        Serial.write(19); // Queries the POS for the recipient's number
        String phoneNumber = "";

        byte readByte;
        // Wait for the entire phone number to arrive
        while (true) {
            readByte = Serial.read();
            
            // If the read byte is a valid digit
            if (readByte > 47 && readByte < 58) {
                phoneNumber += (char)readByte;
            }
            // If the read byte is the end marker
            else if (readByte == 3) {
                break;
            }
        }
        gsmSerial.print("AT+CMGS=\"+");
        gsmSerial.print(phoneNumber);
        gsmSerial.print("\"\r");

        if (SMSDebugMode) {
            clearLCDRow(1);
            lcd.print(phoneNumber);
        }

        // Wait for the message line marker to be sent
        timeoutStart = millis(); // Mark the start of the timeout
        resetBufferEnding(); // Clear the current contents of the buffer ending

        // Wait 100 ms before timing out
        while ((millis() - timeoutStart) < 100) {
            while(gsmSerial.available()) {
                addByteToBufferEnding(gsmSerial.read());
                // If the response is 'ERROR'
                if (bufferEnding[0] == 'O' && bufferEnding[1] == 'R') {
                    flushGSMSerial();
                    readyState = false;
                    // TEMP
                    lcd.clear();
                    lcd.print("SMS Number Fail");
                    buzzerError();
                    delay(1250);
                    return 2;
                }
                // If the response is the line marker
                else if (bufferEnding[1] == '>') {
                    flushGSMSerial();
                    readyState = true;
                    break;
                }
            }

        }

    }

    // Stage 3
    // Send the message data to the GSM module
    if (readyState) {
        readyState = false;
        byte readByte;
        clearLCDRow(1);
        lcd.print("Sending");
        Serial.write(20); // Queries the POS for the message contents

        // Keep iterating until the end marker has been received
        while (true) {
            readByte = Serial.read();
            
            // If the end marker is received
            if (readByte == 3) {
                if (messageData != 0) {
                    gsmSerial.print(messageData);
                    messageData = "";
                    MessageDataBytes = 0;
                }
                break; // End the loop
            }
            // If any other character is received
            else if (readByte != 255) {
                messageData += (char)readByte;
                MessageDataBytes++;
                if (MessageDataBytes == 160) {
                    gsmSerial.print(messageData);
                    messageData = "";
                    MessageDataBytes = 0;
                }
            }
        }

        gsmSerial.write(26);

        // Wait for the GSM module to reply. This is the part without a timeout.
        resetBufferEnding();
        while (true) {
            while(gsmSerial.available()) {
                addByteToBufferEnding(gsmSerial.read());

                // If the response includes an 'OK'
                if (bufferEnding[0] == 'O' && bufferEnding[1] == 'K') {
                    flushGSMSerial();
                    lcd.clear();
                    lcd.setCursor(4,0);
                    lcd.print("SMS Sent");
                    buzzerSuccess();
                    delay(1500);
                    return 1;
                }
                // If the response is 'ERROR'
                else if (bufferEnding[0] == 'O' && bufferEnding[1] == 'R') {
                    flushGSMSerial();
                    lcd.clear();
                    lcd.setCursor(2,0);
                    lcd.print("SMS not sent");
                    buzzerError();
                    delay(1250);
                    return 0;
                }
            }

        }
    }

    // If one step fails, it will automatically skip all the stages and fail
    lcd.clear();
    lcd.print("Generic Error");
    buzzerError();
    delay(1250);
    return 2;
}

/**
 * Gets GSM Signal Quality
 * @return Integer indicating the signal quality
 */
int getGSMSignalQuality() {
    sendATCommand("AT+CSQ",true);
    int returnValue = -1;
    String temp = "";
    boolean responseReceived = false;

    timeoutStart = millis();
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

/**
 * Adds a byte to the 2-byte buffer ending
 * @param input is the byte that will be added to the end of bufferEnding
 */
void addByteToBufferEnding(byte input) {
    if (input != 10 && input != 13) {
        bufferEnding[0] = bufferEnding[1];
        bufferEnding[1] = input;
    }
}

/**
 * Resets the buffer ending with byte 0
 */
void resetBufferEnding() {
    bufferEnding[0] = 0;
    bufferEnding[1] = 0;
}

/**
 * Goes into a manual AT command mode
 */
void ATCommandMode() {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Sending SMS");

    while (true) {
        byte readByte = Serial.read();

        // If the exit byte is received
        if (readByte == 128) {
            break;
        }
        else if (readByte != 255) {
            gsmSerial.write(readByte);
        }

        while (gsmSerial.available()) {
            Serial.write(gsmSerial.read());
        }
    }
}

/**
 * Clears the remaining data in the GSM serial buffer
 */
void flushGSMSerial() {
    while (gsmSerial.available()) {
        gsmSerial.read();
    }
}

/**
 * Sends an AT command to the GSM module using the fix with breaks in between bytes
 * @param command is the String form of the AT command that will be sent to the GSM
 * @param sendCarriageReturn will determine if a carriage return character will be sent after the command
 */
void sendATCommand(String command, boolean sendCarriageReturn) {
    for (int x = 0; x < command.length(); x++) {
        gsmSerial.print(command.charAt(x));
    }

    if (sendCarriageReturn) {
        gsmSerial.write(13); // Sends a carriage return
    }
}