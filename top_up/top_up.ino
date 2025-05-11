#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9 
#define SS_PIN          10         
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Place your new card on the reader...");
}

void loop() {
  // Look for new card
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Print UUID
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Data to write
  byte plateBlock[16] = {0};  // Initialize array with zeroes
  String plate = "RAH972U";  // Plate to write to block 2
  
  // Copy plate string into plateBlock
  plate.getBytes(plateBlock, plate.length() + 1);  

  int balance = 10000;
  byte balanceBlock[16] = {0};
  memcpy(balanceBlock, &balance, sizeof(int)); // Write integer balance as bytes

  // Authenticate and write plate number to block 2
  if (!writeBlock(2, plateBlock)) {
    Serial.println("Failed to write plate number to block 2.");
  } else {
    Serial.println("Plate number written to block 2.");
  }

  // Authenticate and write balance to block 4
  if (!writeBlock(4, balanceBlock)) {
    Serial.println("Failed to write balance to block 4.");
  } else {
    Serial.println("Balance written to block 4.");
  }

  delay(2000); // Pause before looking for next card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

bool writeBlock(byte blockAddr, byte* blockData) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  // Authenticate
  byte status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    blockAddr,
    &key,
    &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Write block
  status = mfrc522.MIFARE_Write(blockAddr, blockData, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Write failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}
