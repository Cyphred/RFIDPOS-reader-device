# RFIDPOS-reader-device
RFID Card Reader device to complement POS Implementation in Java

## I. Parts List
1. Arduino Uno
2. RFID RC522 Reader
3. LCM1602 IIC
4. LCD SCreen
5. SIM800L
6. Keypad
7. Buzzer

## II. Functions
##### A. GENERAL
- [ ] Cancel an operation

##### B. RFID
- [x] Test for RFID Scanner Status
- [x] Read RFID Cards and send the unique identifier over Serial
- [x] ~~Read RFID Cards and wait for a 6-digit PIN for verification~~
- [x] Challenge user for correct PIN/Paasscode
- [x] Make the device hold the scanned serial number until the Java program has requested it

##### C. GSM
- [ ] Test for GSM Module Status
- [ ] Ask for GSM Signal Strength
- [ ] Send SMS

##### D. KEYPAD
- [x] Input functionality
- [x] Ask for PIN input (for creating new accounts)
- [x] Challenge customer for PIN
- [x] Similar behavior of input between `challenge()` and `newPass()`
