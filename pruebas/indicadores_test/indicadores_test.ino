// Prueba de LEDs, buzzer activo y boton Restablecer
//
// Conexiones (docs/conexiones.md §5.6 y §5.8):
//   GPIO32 -> 330 Ω -> LED verde -> GND
//   GPIO33 -> 330 Ω -> LED amarillo -> GND
//   GPIO2  -> 330 Ω -> LED rojo -> GND        (tambien enciende el LED azul de la placa)
//   GPIO15 -> 1 kΩ -> base del NPN; buzzer entre +5 V y colector; emisor a GND
//   Boton Restablecer: 3V3 -> 10 kΩ -> GPIO36, y boton NO entre GPIO36 y GND
//
// Que hace: secuencia automatica de LEDs y buzzer, y despues lee el boton.
//
// Comandos (Monitor Serial a 115200):
//   v / a / r -> encender/apagar LED verde / amarillo / rojo
//   b -> buzzer encendido 1 s con nivel fijo (ALTO)
//   q -> repetir la secuencia automatica
//
// Criterios de exito:
//   - Cada LED enciende solo con su comando y el color corresponde.
//   - Con 'b' el buzzer SUENA de forma continua durante 1 s. Si solo hace "clic" al encender y
//     apagar, es un buzzer PASIVO: avisar, porque el firmware asume buzzer ACTIVO.
//   - Cada pulsacion del boton imprime UNA sola linea "Restablecer presionado" (antirrebote OK)
//     y GPIO36 vuelve a ALTO al soltarlo.

#define LED_VERDE    32
#define LED_AMARILLO 33
#define LED_ROJO     2
#define BUZZER       15
#define BOTON        36
#define ANTIRREBOTE_MS 50

void secuencia() {
  const uint8_t leds[] = {LED_VERDE, LED_AMARILLO, LED_ROJO};
  const char* nombres[] = {"verde", "amarillo", "rojo"};
  for (int i = 0; i < 3; i++) {
    Serial.printf("LED %s encendido\n", nombres[i]);
    digitalWrite(leds[i], HIGH);
    delay(700);
    digitalWrite(leds[i], LOW);
  }
  Serial.println("Buzzer: 3 pitidos cortos");
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(150);
  }
  Serial.println("Ahora presiona el boton Restablecer (o usa v/a/r/b/q).");
}

void alternar(uint8_t pin, const char* nombre) {
  bool nuevo = !digitalRead(pin);
  digitalWrite(pin, nuevo);
  Serial.printf("LED %s %s\n", nombre, nuevo ? "encendido" : "apagado");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(BOTON, INPUT);   // pull-up externo obligatorio (GPIO36 no tiene interno)
  delay(300);
  Serial.println();
  Serial.println("=== Prueba LEDs, buzzer y boton Restablecer ===");
  if (digitalRead(BOTON) == LOW) {
    Serial.println("AVISO: GPIO36 esta en BAJO sin presionar: falta el pull-up de 10 kΩ o el boton esta en corto.");
  }
  secuencia();
}

void loop() {
  static bool estable = HIGH, lectura = HIGH;
  static unsigned long tCambio = 0;
  static uint32_t pulsaciones = 0;

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'v') alternar(LED_VERDE, "verde");
    else if (c == 'a') alternar(LED_AMARILLO, "amarillo");
    else if (c == 'r') alternar(LED_ROJO, "rojo");
    else if (c == 'q') secuencia();
    else if (c == 'b') {
      Serial.println("Buzzer con nivel fijo 1 s: debe sonar continuo (buzzer activo)");
      digitalWrite(BUZZER, HIGH);
      delay(1000);
      digitalWrite(BUZZER, LOW);
    }
  }

  bool ahora = digitalRead(BOTON);
  if (ahora != lectura) {
    lectura = ahora;
    tCambio = millis();
  }
  if (lectura != estable && millis() - tCambio >= ANTIRREBOTE_MS) {
    estable = lectura;
    if (estable == LOW) {
      pulsaciones++;
      Serial.printf("Restablecer presionado (%lu)\n", (unsigned long)pulsaciones);
    } else {
      Serial.println("  ...soltado (GPIO36 = ALTO)");
    }
  }
}
