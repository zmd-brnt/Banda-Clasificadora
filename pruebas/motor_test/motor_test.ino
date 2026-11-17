// Prueba motor TT + driver TB6612FNG (SIN boton de paro, solo avanza)
//
// Conexiones minimas:
//   TB6612 VM   -> +5 V de la fuente (rama de potencia)
//   TB6612 VCC  -> 3V3 del ESP32
//   TB6612 GND  -> GND comun (fuente + ESP32)   <- sin esto NO gira
//   TB6612 STBY -> GPIO25 (directo)
//   TB6612 PWMA -> GPIO26
//   TB6612 AIN1 -> 3V3 (fijo)                   <- si queda suelto, el motor NO gira
//   TB6612 AIN2 -> GND (fijo)
//   AO1, AO2    -> motor TT
//
// Al encender: el motor arranca SOLO a 100 % (no hace falta escribir nada).
//
// Comandos (Monitor Serial a 115200):
//   s -> detener      f -> 100 %      0..9 -> 0 %..90 %
//   d -> PWMA en ALTO fijo SIN PWM (descarta problemas del PWM)
//   a -> prueba automatica 30 / 60 / 100 % (4 s cada uno)

#include <esp_arduino_version.h>

#define PIN_PWMA 26
#define PIN_STBY 25

#define FREQ  20000
#define RESOL 10
#define CANAL 2          // solo core 2.x

void pwmIniciar() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_PWMA, FREQ, RESOL);
#else
  ledcSetup(CANAL, FREQ, RESOL);
  ledcAttachPin(PIN_PWMA, CANAL);
#endif
}

void pwmSoltar() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcDetach(PIN_PWMA);
#else
  ledcDetachPin(PIN_PWMA);
#endif
}

void escribirPwm(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_PWMA, duty);
#else
  ledcWrite(CANAL, duty);
#endif
}

bool modoDigital = false;

void velocidad(int pct) {
  if (modoDigital) {          // volver de modo 'd' a PWM
    pwmIniciar();
    modoDigital = false;
  }
  escribirPwm(((1u << RESOL) - 1) * (uint32_t)pct / 100);
  Serial.printf("PWM = %d %%\n", pct);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);   // driver habilitado
  pwmIniciar();
  delay(300);
  Serial.println();
  Serial.println("=== Prueba motor + TB6612 (sin boton de paro) ===");
  Serial.println("s = detener  f = 100 %  0..9 = 0..90 %  d = PWMA fijo en ALTO  a = automatica");
  velocidad(100);                 // arranca solo
}

void automatica() {
  const int pasos[] = {30, 60, 100};
  for (int p : pasos) {
    velocidad(p);
    delay(4000);
  }
  velocidad(0);
  Serial.println("Prueba automatica terminada.");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') automatica();
    else if (c == 'f') velocidad(100);
    else if (c == 's') velocidad(0);
    else if (c >= '0' && c <= '9') velocidad((c - '0') * 10);
    else if (c == 'd') {
      pwmSoltar();
      pinMode(PIN_PWMA, OUTPUT);
      digitalWrite(PIN_PWMA, HIGH);
      modoDigital = true;
      Serial.println("PWMA = ALTO fijo (sin PWM)");
    }
  }
}
