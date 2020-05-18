/**
 * PINCommands
 * Will contain all PIN entry-related methods and variables
 */

/**
 * Challenges the user to match the PIN fetched from the database
 * @return corresponds the completion state
 */
byte PINChallenge() {
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
        if (readByte == 6) {
            return 2;
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
                if (readByte == 6) {
                    return 2; // Cancels the method
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
                    // Check if there are already 6 digits entered
                    else if (readKey == '*' && cursorPosition == 6) {
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
                            lcd.clear(); // Clear the LCD before finishing
                            return 1; // Ends the method
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
                                lcd.clear(); // Clear the LCD before finishing
                                return 0; // Ends the method
                            }

                            break;
                        }
                    }
                }
            }
        }
    }

    lcd.clear(); // Clear the LCD before finishing
}

/**
 * Creates a new PIN by asking for a combination and verifying it
 */
void PINCreate() {
    char initialPIN[6]; // Will store the first entered PIN
    char confirmPIN[6]; // Will store the second entered PIN for verification
    byte cursorPositionA = 0; // Will keep track of the current position of the first "cursor"
    byte verifyAttempts = 0;

    // An infinite loop encompassing the initial input and the verification
    while (true) {
        if (verifyAttempts == 0) {
            // Prompts the user to enter a new PIN
            lcd.clear();
            lcd.setCursor(1,0);
            lcd.print("New PIN:");
            lcd.setCursor(0,1);
            lcd.print("[*]OK  [#]DELETE");
            lcd.setCursor(10,0);

            // In case the user goes back from the verification to change the initial entered PIN
            // populate the lcd with the asterisks to show the enterd digits prior to entering the
            // verification screen
            if (cursorPositionA > 0) {
                for (int x = 0; x < 6; x++) {
                    lcd.print('*');
                }
            }

            // Another infinite inner loop that will catch the user's first inputs
            while (true) {
                byte readByte = Serial.read(); // Reads the serial in case a cancel byte or status inquiry is issued
                if (readByte == 6) {
                    return; // Cancels the method
                }

                char readKey = keypad.getKey(); // Grab data from keypad
                // If the keypad was pressed
                if (readKey) {
                    // If the pressed key is a digit
                    if (readKey > 47 && readKey < 58) {
                        // If there are already 6 digits entered
                        if (cursorPositionA < 6) {
                            lcd.print('*');
                            initialPIN[cursorPositionA] = readKey;
                            cursorPositionA++;
                        }
                    }
                    // If the pound key is pressed (Delete Key)
                    else if (readKey == '#') {
                        // Check if the current cursor position is not at zero
                        if (cursorPositionA > 0) {
                            lcd.setCursor((9 + cursorPositionA),0);
                            lcd.write(32); // Overwrite with space
                            lcd.setCursor((9 + cursorPositionA),0);
                            cursorPositionA--;
                        }
                    }
                    // If the asterisk key is pressed (OK Key)
                    else if (readKey == '*') {
                        // Check if there are already 6 digits entered
                        if (cursorPositionA == 6) {
                            break;
                        }
                    }
                }
            }
        }

        byte cursorPositionB = 0; // Will keep track of the current position of the second "cursor"
        // Prompts the user to enter the new PIN a second time for verification
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("Re-enter:");
        lcd.setCursor(0,1);
        lcd.print("[*]OK  [#]BACK");
        lcd.setCursor(10,0);

        boolean backButtonVisibleLastIteration = true; // Keeps track if the back button was visible during the last iteration

        // Another infinite inner loop that will catch the user's second input
        while (true) {
            byte readByte = Serial.read(); // Reads the serial in case a cancel byte or status inquiry is issued
            if (readByte == 6) {
                return; // Cancels the method
            }

            char readKey = keypad.getKey(); // Grab data from keypad
            // If the keypad was pressed
            if (readKey) {
                // If the pressed key is a digit
                if (readKey > 47 && readKey < 58) {
                    // If there are already 6 digits entered
                    if (cursorPositionB < 6) {
                        lcd.print('*');
                        confirmPIN[cursorPositionB] = readKey;
                        cursorPositionB++;
                    }
                }
                // If the pound key is pressed (Delete Key)
                else if (readKey == '#') {
                    // Check if the current cursor position is not at zero
                    if (cursorPositionB > 0) {
                        lcd.setCursor((9 + cursorPositionB),0);
                        lcd.write(32); // Overwrite with space
                        lcd.setCursor((9 + cursorPositionB),0);
                        cursorPositionB--;
                    }
                    // If the current cursor position is at zero, that means go back to the first PIN input
                    else if (cursorPositionB == 0) {
                        verifyAttempts = 0;
                        break;
                    }
                }
                // If the asterisk key is pressed (OK Key)
                // Check if there are already 6 digits entered
                else if (readKey == '*' && cursorPositionB == 6) {
                    boolean validPIN = true;
                    // Check every digit in the second input and the first input to see if there is a mismatch
                    for (int x = 0; x < 6; x++) {
                        // If a mistmatch is found
                        if (confirmPIN[x] != initialPIN[x]) {
                            validPIN = false;
                            break;
                        }
                    }

                    // If no mismatches were found
                    if (validPIN) {
                        // Prompt PIN match
                        lcd.clear();
                        lcd.print("New PIN Verified");
                        buzzerSuccess();
                        delay(1500);

                        // Print the newly-created PIN to serial
                        Serial.write(2);
                        for (int x = 0; x < 6; x++) {
                            Serial.print(initialPIN[x]);
                        }
                        Serial.write(3);
                        lcd.clear(); // Clear the LCD before finishing
                        return; // Ends the method
                    }
                    // If a mismatch is found
                    else {
                        // Prompt PIN mismatch
                        lcd.clear();
                        lcd.setCursor(2,0);
                        lcd.print("PIN mismatch");
                        lcd.setCursor(0,1);
                        lcd.print("Please try again");
                        buzzerError();
                        delay(1250);
                        verifyAttempts++;

                        break;
                    }
                }

                // Check if the back or delete key should be shown
                // If the cursor position B is at zero and the back button was not visible during the last iteration
                if (cursorPositionB == 0 && !backButtonVisibleLastIteration) {
                    backButtonVisibleLastIteration = true;
                    lcd.setCursor(14,1);
                    lcd.write(32);
                    lcd.write(32);
                    lcd.setCursor(10,1);
                    lcd.print("BACK");
                    lcd.setCursor(10,0);
                }
                // If the cursor position B is not at zero
                else if (cursorPositionB > 0 && backButtonVisibleLastIteration) {
                    backButtonVisibleLastIteration = false;
                    lcd.setCursor(10,1);
                    lcd.print("DELETE");
                    lcd.setCursor((10 + cursorPositionB),0);
                }
            }
        }
    }
}