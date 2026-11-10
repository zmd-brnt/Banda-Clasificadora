#include "nfc.h"

#include <MFRC522.h>
#include <SPI.h>

#include "config.h"

static MFRC522 lector(PIN_RC522_SS, PIN_RC522_RST);

bool LectorNfc::begin() {
  SPI.begin();
  lector.PCD_Init();
  delay(100);
  version_ = lector.PCD_ReadRegister(MFRC522::VersionReg);
  ok_ = !(version_ == 0x00 || version_ == 0xFF);
  if (ok_) lector.PCD_SetAntennaGain(MFRC522::RxGain_max);
  return ok_;
}

bool LectorNfc::leerUid(char* destino) {
  if (!ok_) return false;

  byte atqa[2];
  byte largo = sizeof(atqa);
  MFRC522::StatusCode s = lector.PICC_WakeupA(atqa, &largo);
  if (s != MFRC522::STATUS_OK && s != MFRC522::STATUS_COLLISION) return false;
  if (!lector.PICC_ReadCardSerial()) return false;

  destino[0] = '\0';
  for (byte i = 0; i < lector.uid.size && (size_t)(i * 2 + 2) <= MAX_UID; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02X", lector.uid.uidByte[i]);
    strcat(destino, hex);
  }
  lector.PICC_HaltA();
  lector.PCD_StopCrypto1();
  return destino[0] != '\0';
}
