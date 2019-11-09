# RFIDPOS-reader-device
RFID Card Reader device to complement POS Implementation in Java

Parts List:
1. Arduino Uno
2. RFID RC522 Reader
3. LCM1602 IIC
4. LCD SCreen
5. SIM800L
6. Keypad
7. Buzzer

Functions:
[GENERAL]
1. Cancel an operation
2. Ask for PIN input (for creating new accounts)

[RFID]
1. Test for RFID Scanner Status - OK
2. Read RFID Cards and send the unique identifier over Serial - OK
3. Read RFID Cards and wait for a 6-digit PIN for verification - [Updated, see #4]
4. Challenge user for correct PIN/Paasscode - OK

[GSM]
1. Test for GSM Module Status
2. Ask for GSM Signal Strength
3. Send SMS

[Keypad]
1. Input functionality

