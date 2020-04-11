#include <SoftwareSerial.h>         // software Serial for communication with the GSM Module

// GSM Module variables and declarations
SoftwareSerial gsmSerial(8,7); // Create SoftwareSerial object
int gsmPowerPin = 6; // Digital pin connected to the GSM module's power toggle switch
byte serialBufferDelay = 10; // The delay in milliseconds to wait in between each character being sent to the GSM module via software serial

void setup() {
    Serial.begin(115200); // Initialize hardware serial communication with 
    gsmSerial.begin(19200); // Initialize software serial communication with the GSM module
}

void loop() {
    byte readByte;

    while (Serial.available()) {
        readByte = Serial.read();

        if (readByte == 97) {
            gsmSerial.print("AT+CMGF=1\r");
        }

        while (gsmSerial.available()) {
            Serial.write(gsmSerial.read());
        }
    }
}

void sendStringToGSMSerial(String inputString) {
    // Iterate for the number of characters that the input string has
    for (int x = 0; x < inputString.length(); x++) {
        gsmSerial.print(inputString.charAt(x));
        delay(serialBufferDelay); // Wait for the specified buffer delay in ms in between each character before sending to the GSM serial
    }
}