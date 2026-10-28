// Prueba NFC: Lectura de UID con RC522
//
// Conexiones ESP32 <-> RC522:
//   ESP32 GPIO5  (SS)   ----> RC522 SDA
//   ESP32 GPIO18 (SCK)  ----> RC522 SCK
//   ESP32 GPIO23 (MOSI) ----> RC522 MOSI
//   ESP32 GPIO19 (MISO) ----> RC522 MISO
//   ESP32 GPIO22         ----> RC522 RST
//   ESP32 3.3V          ----> RC522 3.3V  (NO usar 5V)
//   ESP32 GND           ----> RC522 GND
//
// Biblioteca requerida:
//   En Arduino IDE: Sketch -> Incluir Libreria -> Administrar Librerias
//   Buscar "MFRC522" de GithubCommunity e instalar.
//
// Funcionamiento:
//   Espera que acerques una etiqueta NFC al lector.
//   Lee el UID y lo muestra en el Monitor Serial.
//   Detecta si la etiqueta es NTAG213 (la que usa nuestro proyecto).
//
// Criterio de exito:
//   - Al acercar una etiqueta, el Monitor Serial muestra su UID en hexadecimal.
//   - El UID debe ser consistente: la misma etiqueta siempre muestra el mismo UID.
//   - Etiquetas diferentes muestran UIDs diferentes.

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5
#define RST_PIN 22

MFRC522 lector(SS_PIN, RST_PIN);

uint32_t lecturas_totales = 0;
uint32_t lecturas_exitosas = 0;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  lector.PCD_Init();

  delay(100);

  Serial.println("=== Prueba NFC - Lector RC522 ===");
  Serial.println();

  byte version = lector.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("Firmware del RC522: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println("ERROR: No se detecta el RC522.");
    Serial.println("Revisa las conexiones SPI y la alimentacion.");
    Serial.println("Recuerda: el RC522 se alimenta a 3.3V, NO a 5V.");
    while (true) { delay(1000); }
  }

  if (version == 0x92) {
    Serial.println("RC522 version 2.0 detectado - OK");
  } else if (version == 0x91) {
    Serial.println("RC522 version 1.0 detectado - OK");
  } else {
    Serial.print("Version no comun, pero se intentara usar: 0x");
    Serial.println(version, HEX);
  }

  Serial.println();
  Serial.println("Acerca una etiqueta NFC al lector...");
  Serial.println();
}

void loop() {
  if (!lector.PICC_IsNewCardPresent()) {
    return;
  }

  if (!lector.PICC_ReadCardSerial()) {
    return;
  }

  lecturas_totales++;
  lecturas_exitosas++;

  Serial.print("[Lectura #");
  Serial.print(lecturas_exitosas);
  Serial.println("]");

  // Mostrar UID
  Serial.print("  UID: ");
  String uid_str = "";
  for (byte i = 0; i < lector.uid.size; i++) {
    if (lector.uid.uidByte[i] < 0x10) {
      Serial.print("0");
      uid_str += "0";
    }
    Serial.print(lector.uid.uidByte[i], HEX);
    uid_str += String(lector.uid.uidByte[i], HEX);
    if (i < lector.uid.size - 1) {
      Serial.print(":");
      uid_str += ":";
    }
  }
  Serial.println();

  Serial.print("  Bytes del UID: ");
  Serial.println(lector.uid.size);

  // Identificar tipo de etiqueta
  MFRC522::PICC_Type tipo = lector.PICC_GetType(lector.uid.sak);
  Serial.print("  Tipo: ");
  Serial.println(lector.PICC_GetTypeName(tipo));

  if (lector.uid.size == 7) {
    Serial.println("  >> UID de 7 bytes: compatible con NTAG213");
  } else if (lector.uid.size == 4) {
    Serial.println("  >> UID de 4 bytes: etiqueta MIFARE Classic u otra");
  }

  Serial.print("  Lecturas exitosas: ");
  Serial.print(lecturas_exitosas);
  Serial.println();
  Serial.println("---");
  Serial.println("Acerca otra etiqueta o retira y vuelve a acercar la misma...");
  Serial.println();

  lector.PICC_HaltA();
  lector.PCD_StopCrypto1();

  delay(500);
}
