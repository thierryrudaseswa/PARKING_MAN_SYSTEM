#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

void setup() {
    Serial.begin(9600);
    SPI.begin();
    mfrc522.PCD_Init();
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

String readBlock(byte block) {
    byte buffer[18], size = sizeof(buffer);
    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &mfrc522.uid) != MFRC522::STATUS_OK)
        return "";
    if (mfrc522.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK)
        return "";
    String s;
    for (byte i = 0; i < 16; i++) {
        if (buffer[i] != 0) s += (char)buffer[i];
    }
    s.trim();
    return s;
}

int readIntBlock(byte block) {
    byte buffer[18], size = sizeof(buffer);
    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &mfrc522.uid) != MFRC522::STATUS_OK)
        return -1;
    if (mfrc522.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK)
        return -1;
    int value;
    memcpy(&value, buffer, sizeof(value));
    return value;
}

void writeIntBlock(byte block, int value) {
    byte buf[16] = {0};
    memcpy(buf, &value, sizeof(value));
    mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &mfrc522.uid);
    mfrc522.MIFARE_Write(block, buf, 16);
}

void loop() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
        return;

    String plate = readBlock(2);
    int balance = readIntBlock(4);
    Serial.print(plate);
    Serial.print(",");
    Serial.println(balance);

    unsigned long startTime = millis();
    while (!Serial.available()) {
        if (millis() - startTime > 5000) {
            Serial.println("timeout");
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
            delay(500);
            return;
        }
    }

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("topup,")) {
        int amount = command.substring(6).toInt();
        if (amount <= 0) {
            Serial.println("invalid_topup");
        } else {
            int newBalance = balance + amount;
            writeIntBlock(4, newBalance);
            int verifyBalance = readIntBlock(4);
            if (verifyBalance == newBalance) {
                Serial.print("topped,");
                Serial.println(newBalance);
            } else {
                Serial.println("write_error");
            }
        }
    } else {
        int due = command.toInt();
        if (due == -1) {
            Serial.println("insufficient");
        } else {
            int newBalance = balance - due;
            if (newBalance < 0) {
                Serial.println("insufficient");
            } else {
                writeIntBlock(4, newBalance);
                Serial.println("done");
            }
        }
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
}