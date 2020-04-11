/**
 * GSMMethods
 * Will contain all GSM-related methods and variables
 */

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

// Checks if the SIM card is detected
boolean checkSIM() {
    gsmSerial.print("AT+CPIN?\r");
    String temp = "";
    timeoutStart = millis();
    boolean responseReceived = false;
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
        if (temp.indexOf("READY") >= 0) {
            return true;
        }
    }

    return false;
}

// Gets GSM Signal Quality
int getGSMSignalQuality() {
    gsmSerial.print("AT+CSQ\r");
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

// Goes into a manual AT command mode
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

        delay(10);
    }
}

// Clears the remaining data in the GSM serial buffer
void flushGSMSerial() {
    while (gsmSerial.available()) {
        gsmSerial.read();
    }
}