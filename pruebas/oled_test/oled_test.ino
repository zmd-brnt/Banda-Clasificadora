// Prueba OLED 0.96" I2C (SSD1306)
//
// Conexiones (docs/conexiones.md §5.2):
//   OLED GND -> GND    VCC -> 3V3    SCL -> GPIO22    SDA -> GPIO21
//   Revisa el orden de los pines en TU modulo: algunos traen VCC y GND invertidos.
//
// Bibliotecas: "Adafruit SSD1306" (instala tambien "Adafruit GFX Library" y "Adafruit BusIO").
//   Arduino IDE: Herramientas -> Administrar bibliotecas -> buscar "Adafruit SSD1306".
//   arduino-cli: arduino-cli lib install "Adafruit SSD1306"
//
// Que hace:
//   1. Escanea el bus I2C e imprime todas las direcciones que responden.
//   2. Si encuentra 0x3C o 0x3D, inicializa la pantalla y muestra texto, un contador y
//      una pantalla de ejemplo con el formato del firmware.
//
// Criterios de exito:
//   - El escaner muestra 0x3C (o 0x3D). Anota cual: va en OLED_DIRECCION de config.h.
//   - Se lee el texto completo y el contador aumenta cada segundo sin congelarse.
//   - Ningun otro dispositivo aparece en direcciones inesperadas.

#include <Adafruit_SSD1306.h>
#include <Wire.h>

Adafruit_SSD1306* oled = nullptr;
uint8_t direccion = 0;
uint32_t contador = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== Prueba OLED I2C ===");

  Wire.begin();   // SDA 21, SCL 22
  Serial.println("Escaneando bus I2C...");
  int encontrados = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Dispositivo en 0x%02X\n", a);
      encontrados++;
      if (a == 0x3C || a == 0x3D) direccion = a;
    }
  }
  if (encontrados == 0) {
    Serial.println("Ningun dispositivo responde. Revisa VCC/GND (orden), SDA=21, SCL=22 y tierra comun.");
    return;
  }
  if (!direccion) {
    Serial.println("Hay dispositivos pero ninguno en 0x3C/0x3D: puede que no sea una OLED SSD1306.");
    return;
  }

  oled = new Adafruit_SSD1306(128, 64, &Wire, -1);
  if (!oled->begin(SSD1306_SWITCHCAPVCC, direccion)) {
    Serial.println("La OLED responde pero no se pudo inicializar (memoria o controlador distinto).");
    oled = nullptr;
    return;
  }
  Serial.printf("OLED inicializada en 0x%02X. Revisa la pantalla.\n", direccion);
}

void loop() {
  if (!oled) {
    delay(1000);
    return;
  }
  oled->clearDisplay();
  oled->setTextColor(SSD1306_WHITE);
  oled->setTextSize(1);
  oled->setCursor(0, 0);
  oled->println("ESPERA");
  oled->drawFastHLine(0, 9, 128, SSD1306_WHITE);
  oled->setCursor(0, 12);
  oled->println("A:12 B:7 R:3");
  oled->println("Sin leer: 1");
  oled->println("C 12/15 7/15 3/15");
  oled->println("Pi: OK  pend: 0");
  oled->println();
  oled->printf("Contador: %lu\n", (unsigned long)contador++);
  oled->display();
  Serial.printf("Contador en pantalla: %lu\n", (unsigned long)contador - 1);
  delay(1000);
}
