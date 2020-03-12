#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

SoftwareSerial gsmSerial(8,7);
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2); // Initialization for LCD Library

String gsmSerialTemp = "";

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
}

void loop() {
    byte readByte;
    if (Serial.available()) {
        readByte = Serial.read();

        if (readByte == 136) {
            if (checkSMS()) {
                Serial.print(1);
                streamSMSContent();
            }
            else {
                Serial.print(0);
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

boolean checkSMS() {
    unsigned long timeoutStart; // Will keep track of when the timer started

    gsmSerial.print("AT+CMGF=1\r"); // AT command to set gsmSerial to SMS mode
    boolean responseReceived = false;

    // waits 1 second for a response before timing out
    timeoutStart = millis();
    while ((millis() - timeoutStart) < 1000) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 13 && readByte != 10) {
                gsmSerialTemp += (char)readByte;
            }
        }

        if (gsmSerialTemp.length() > 2) {
            char lastChar[] = {gsmSerialTemp.charAt(gsmSerialTemp.length() - 2),gsmSerialTemp.charAt(gsmSerialTemp.length() - 1)};
            if (lastChar[0] == 'O') {
                if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                    responseReceived = true;
                    break;
                }
            }
        }
    }

    // if the response is received and it contains "OK"
    if (responseReceived && gsmSerialTemp.indexOf("OK") >= 0) {
        return true;
    }

    return false;
}

byte streamSMSContent() {
    lcd.clear(); // TEMP
    lcd.print("Sending SMS to");
    lcd.setCursor(0,1);
    lcd.print('+');
    // With a timeout, wait for Serial data to arrive and immediately push said data to the GSM Module
    unsigned long timeoutStart; // Will keep track of timeout points
    byte streamStatus = 0; // Will keep track of the byte Stream's status
    boolean timedOut = true;

    gsmSerial.print("AT+CMGS=\"+");

    // Timeout after 1 second
    while ((millis() - timeoutStart) < 1000) {
        if (Serial.available()) {
            byte readByte = Serial.read();

            // If the byte stream for receiving the number is inactive, and the stream start marker has been receiced,
            // set the stream status to "running"
            if (streamStatus == 0 && readByte == 2) {
                streamStatus = 1;
            }
            // If the byte stream for receiving the number is active...
            else if (streamStatus == 1) {
                // And the end stream marker is received
                if (readByte == 3) {
                    timedOut = false; // Indicate that the loop did not time out
                    break; // End the current loop
                }
                else {
                    lcd.write(readByte); // TEMP
                    gsmSerial.write(readByte);
                }
            }
        }
    }

    gsmSerial.print('"');
    gsmSerialTemp = ""; // Clear the temp GSM Serial data variable

    delay(100);

    while(gsmSerial.available()) {
        gsmSerialTemp += (char)gsmSerial.read();
    }

    // If the angle bracket indicator that the SMS data receiving is ready, proceed
    if (gsmSerialTemp.indexOf(">") >= 0) {
        // TEMP
        lcd.clear();
        lcd.print("Waiting for message");
    }
    else {
        lcd.clear();
        lcd.print("SMS Error");
        lcd.setCursor(0,1);
        lcd.print("No message");
        return 2;
    }

    // End the method the previous stream timed out
    if (timedOut) {
        lcd.clear();
        lcd.print("SMS Error");
        lcd.setCursor(0,1);
        lcd.print("Timed out");
        return 2;
    }

    // Prepare variables for next byte stream
    timeoutStart = millis();
    streamStatus = 0;
    timedOut = true;

    // Timeout after 5 seconds
    while ((millis() - timeoutStart) < 5000) {
        if (Serial.available()) {
            byte readByte = Serial.read();

            // If the byte stream for receiving the number is inactive, and the stream start marker has been receiced,
            // set the stream status to "running"
            if (streamStatus == 0 && readByte == 2) {
                streamStatus = 1;
            }
            // If the byte stream for receiving the number is active...
            else if (streamStatus == 1) {
                // And the end stream marker is received
                if (readByte == 3) {
                    timedOut = false; // Indicate that the loop did not time out
                    break; // End the current loop
                }
                else {
                    gsmSerial.write(readByte);
                }
            }
        }
    }

    gsmSerial.write(26);
    streamStatus = 0; // Re-purpose variable to indicate if a reply from the module has been received
    gsmSerialTemp = "";

    lcd.clear();
    lcd.print("Sending SMS");

    while (true) {
        if (gsmSerial.available()) {
            byte readByte = gsmSerial.read();
            if (readByte != 13 && readByte != 10) {
                gsmSerialTemp += (char)readByte;
            }
        }

        if (gsmSerialTemp.length() > 2) {
            char lastChar[] = {gsmSerialTemp.charAt(gsmSerialTemp.length() - 2),gsmSerialTemp.charAt(gsmSerialTemp.length() - 1)};
            if (lastChar[0] == 'O') {
                if (lastChar[1] == 'K' || lastChar[1] == 'R') {
                    break;
                }
            }
        }
    }

    if (gsmSerialTemp.indexOf("ERROR") >= 0) {
        lcd.clear();
        lcd.print("SMS Sent");
        return 0;
    }
    else if (gsmSerialTemp.indexOf("OK") >= 0) {
        lcd.clear();
        lcd.print("SMS not sent");
        return 1;
    }

    lcd.clear();
    lcd.print("SMS error");
    lcd.setCursor(0,1);
    lcd.print("Generic");
    return 2;
}