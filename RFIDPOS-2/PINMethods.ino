/**
 * PINCommands
 * Will contain all PIN entry-related methods and variables
 */

void PINChallenge() {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Fetching PIN");
    lcd.setCursor(2,1);
    lcd.print("from database");

    char targetPIN[6]; // Will store the correct PIN
    byte PINCharactersReceived = 0; // Keeps track of the number of digits received as the target PIN

    // Infinite loop to wait for the operation to complete or be cancelled
    while (true) {
        byte readByte = Serial.read(); // Read serial data
        // Cancels the operation
        if (readByte == 131) {
            break;
        }
        // The end marker for the PIN
        else if (readByte == 3) {
            break;
        }
        // If the received byte is a digit, and the number of stored digits have not been satisfied yet
        else if (readByte > 47 && readByte < 58 && PINCharactersReceived < 6) {
            targetPIN[PINCharactersReceived] = readByte; // Add the digit to the target PIN
            PINCharactersReceived++; // Increment the counter for the received digits
        }
        // TODO also add a call for device status and gsm status here
    }

    byte attemptsLeft = 3; // Keeps track of the attempts left

    // Only runs if the number of target PIN characters has been satisfied
    // Otherwise, assumes that the operation has been cancelled
    if (PINCharactersReceived == 6) {
        // Another infinite loop that will only end when it is cancelled, failed or completed
        while (true) {
            char inputDigits[6]; // Will hold the entered digits from the keypad
            byte cursorPosition = 0; // Will keep track of the current position of the "cursor"

            // Prompts the user to enter their PIN
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("PIN:");
            lcd.setCursor(0,1);
            lcd.print("[*]OK  [#]DELETE");
            lcd.setCursor(7,0);

            // Another infinite inner loop that will catch the user's inputs
            while (true) {
                byte readByte = Serial.read(); // Reads the serial in case a cancel byte or status inquiry is issued
                if (readByte == 131) {
                    return; // Cancels the method
                }
                else {
                    // TODO Add status inquries here
                }

                char readKey = keypad.getKey(); // Grab data from keypad
                // If the keypad was pressed
                if (readKey) {
                    // If the pressed key is a digit
                    if (readKey > 47 && readKey < 58) {
                        // If there are already 6 digits entered
                        if (cursorPosition < 6) {
                            lcd.print('*');
                            inputDigits[cursorPosition] = readKey;
                            cursorPosition++;
                        }
                    }
                    // If the pound key is pressed (Delete Key)
                    else if (readKey == '#') {
                        // Check if the current cursor position is not at zero
                        if (cursorPosition > 0) {
                            lcd.setCursor((6 + cursorPosition),0);
                            lcd.write(32); // Overwrite with space
                            lcd.setCursor((6 + cursorPosition),0);
                            cursorPosition--;
                        }
                    }
                    // If the asterisk key is pressed (OK Key)
                    else if (readKey == '*') {
                        // Check if there are already 6 digits entered
                        if (cursorPosition == 6) {
                            boolean validPIN = true;
                            // Check every digit in the input and the target PINs to see if there is a mismatch
                            for (int x = 0; x < 6; x++) {
                                // If a mistmatch is found
                                if (inputDigits[x] != targetPIN[x]) {
                                    validPIN = false;
                                    break;
                                }
                            }

                            // If no mismatches were found
                            if (validPIN) {
                                // Prompt PIN match
                                lcd.clear();
                                lcd.setCursor(2,0);
                                lcd.print("Verification");
                                lcd.setCursor(4,1);
                                lcd.print("Success");
                                buzzerSuccess();
                                delay(1500);

                                Serial.print(1); // Tell the POS that verification was a Success
                                lcd.clear(); // Clear the LCD before finishing
                                return; // Ends the method
                            }
                            // If a mismatch is found
                            else {
                                attemptsLeft--; // Deduct the number of attempts left
                                // Prompt PIN mismatch
                                lcd.clear();
                                lcd.setCursor(2,0);
                                lcd.print("Invalid PIN");
                                lcd.setCursor(1,1);
                                lcd.print(attemptsLeft);
                                lcd.setCursor(3,1);
                                lcd.print("retries left");
                                buzzerError();
                                delay(1250);

                                // If there are no more attempts left
                                if (attemptsLeft == 0) {
                                    Serial.print(0);
                                    lcd.clear(); // Clear the LCD before finishing
                                    return; // Ends the method
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    lcd.clear(); // Clear the LCD before finishing
}
