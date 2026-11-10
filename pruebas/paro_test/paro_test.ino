// Prueba del paro de emergencia por hardware
//
// Objetivo (docs/conexiones.md §5.7 y §7 paso 7): demostrar que al presionar el paro el motor
// se detiene AUNQUE EL PROGRAMA ESTE TRABADO, porque el contacto NC corta STBY del TB6612.
//
// Conexiones:
//   GPIO25 -> contacto NC del boton de paro -> nodo STBY del TB6612
//   Nodo STBY -> 10 kΩ -> GND   (pull-down)
//   Nodo STBY -> GPIO39          (deteccion)
//   Motor y TB6612 como en pruebas/motor_test (PWMA -> GPIO26, AIN1 = 3.3 V, AIN2 = GND)
//
// Comandos (Monitor Serial a 115200):
//   m -> motor al 60 % (sin trabar el programa); muestra cada cambio de GPIO39
//   b -> motor al 60 % y el programa se TRABA a proposito (no lee nada, no cambia el PWM).
//        Presiona el paro: el motor debe detenerse. Para salir, presiona EN (reinicio).
//   s -> detener el motor
//
// Criterios de exito:
//   1. En modo m, al presionar el paro: el motor se detiene, aparece "PARO PRESIONADO" y GPIO39 = BAJO.
//      Al soltarlo (girar el hongo): GPIO39 = ALTO. El motor vuelve a girar porque esta prueba
//      no tiene logica de restablecer (el firmware real SI exige el boton Restablecer).
//   2. En modo b (programa trabado), al presionar el paro el motor se detiene igual. <- clave
//   3. Desconecta un cable del boton con el motor girando: el motor se detiene (falla segura).
//   4. Cuenta de flancos: una pulsacion limpia produce 1 o pocos flancos (anota cuantos; el
//      firmware tolera rebotes porque entra a EMERGENCIA con el primero).

#include <esp_arduino_version.h>

#define PIN_PWMA    26
#define PIN_STBY    25
#define PIN_LECTURA 39

volatile uint32_t flancos = 0;
void IRAM_ATTR isr() { flancos = flancos + 1; }

void escribirPwm(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_PWMA, duty);
#else
  ledcWrite(2, duty);
#endif
}

void motor(int pct) { escribirPwm(1023u * (uint32_t)pct / 100); }

void setup() {
  Serial.begin(115200);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_PWMA, 20000, 10);
#else
  ledcSetup(2, 20000, 10);
  ledcAttachPin(PIN_PWMA, 2);
#endif
  motor(0);
  pinMode(PIN_LECTURA, INPUT);
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);
  attachInterrupt(digitalPinToInterrupt(PIN_LECTURA), isr, CHANGE);
  delay(300);
  Serial.println();
  Serial.println("=== Prueba del paro de emergencia ===");
  Serial.println("m = motor 60 %   b = motor 60 % y TRABAR el programa   s = detener");
  Serial.printf("GPIO39 al arrancar: %s\n", digitalRead(PIN_LECTURA) ? "ALTO (normal)" : "BAJO (paro presionado o cableado)");
}

void loop() {
  static int ultimo = -1;
  static uint32_t flancosPrevios = 0;

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'm') {
      motor(60);
      Serial.println("Motor al 60 %. Presiona el paro.");
    } else if (c == 's') {
      motor(0);
      Serial.println("Motor detenido.");
    } else if (c == 'b') {
      motor(60);
      Serial.println("Motor al 60 %. PROGRAMA TRABADO a partir de ahora. Presiona el paro:");
      Serial.println("el motor debe detenerse. Presiona EN para salir.");
      Serial.flush();
      while (true) delay(1);   // simula un programa colgado: ya no hace nada mas
    }
  }

  int nivel = digitalRead(PIN_LECTURA);
  if (nivel != ultimo) {
    ultimo = nivel;
    Serial.println(nivel ? "GPIO39 = ALTO: paro libre, driver habilitado" : "GPIO39 = BAJO: PARO PRESIONADO (driver deshabilitado)");
  }
  if (flancos != flancosPrevios) {
    delay(100);   // junta los rebotes de una pulsacion
    Serial.printf("  flancos registrados: %lu\n", (unsigned long)(flancos - flancosPrevios));
    flancosPrevios = flancos;
  }
}
