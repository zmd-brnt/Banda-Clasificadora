// Prueba motor TT + driver TB6612FNG (solo avanza)
//
// Conexiones (docs/conexiones.md §5.5 y §5.7):
//   TB6612 VM   -> +5 V rama de POTENCIA        VCC -> 3V3 del ESP32     GND -> GND comun
//   TB6612 PWMA -> GPIO26
//   TB6612 AIN1 -> 3.3 V (fijo)                 AIN2 -> GND (fijo)
//   TB6612 STBY -> nodo del boton de paro: GPIO25 -> contacto NC -> STBY, 10 kΩ de STBY a GND,
//                  y GPIO39 leyendo el nodo STBY
//   AO1, AO2    -> motor TT (con capacitor de 100 nF entre sus terminales)
//   Si aun no instalan el boton de paro: conecta GPIO25 directo a STBY solo para esta prueba.
//
// Que hace: pone GPIO25 en ALTO (habilita el driver) y aplica PWM a la banda.
//
// Comandos (Monitor Serial a 115200):
//   a -> prueba automatica: 30 %, 60 % y 100 % durante 4 s cada uno, y se detiene
//   0..9 -> ciclo de trabajo 0 %..90 %     f -> 100 %     s -> detener (PWM 0)
//   STBY: el estado del nodo (GPIO39) se imprime cada segundo
//
// Criterios de exito (conexiones.md §7 paso 6):
//   - El motor gira HACIA ADELANTE (la banda avanza en el sentido correcto). Si gira al revés,
//     intercambia los cables AO1 y AO2 (ya no se puede corregir en codigo).
//   - La velocidad cambia de forma visible entre 30, 60 y 100 %.
//   - El ESP32 no se reinicia (no vuelve a aparecer el encabezado) en ningun escalon.
//   - Anota a partir de que porcentaje la banda arranca con carga: es el minimo para PWM_BANDA_PCT.
//   - Opcional: mide el voltaje en VIN del ESP32 al arrancar el motor (no debe caer de ~4.75 V).

#include <esp_arduino_version.h>

#define PIN_PWMA    26
#define PIN_STBY    25   // a traves del contacto NC del boton de paro
#define PIN_LECTURA 39   // nodo STBY

#define FREQ  20000
#define RESOL 10
#define CANAL 2          // solo core 2.x

void escribirPwm(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_PWMA, duty);
#else
  ledcWrite(CANAL, duty);
#endif
}

void velocidad(int pct) {
  escribirPwm(((1u << RESOL) - 1) * (uint32_t)pct / 100);
  Serial.printf("PWM = %d %%\n", pct);
}

void setup() {
  Serial.begin(115200);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_PWMA, FREQ, RESOL);
#else
  ledcSetup(CANAL, FREQ, RESOL);
  ledcAttachPin(PIN_PWMA, CANAL);
#endif
  escribirPwm(0);
  pinMode(PIN_LECTURA, INPUT);
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);
  delay(300);
  Serial.println();
  Serial.println("=== Prueba motor + TB6612 (AIN1/AIN2 fijos: solo avanza) ===");
  Serial.println("a = automatica   0..9 = 0..90 %   f = 100 %   s = detener");
  if (digitalRead(PIN_LECTURA) == LOW) {
    Serial.println("AVISO: STBY esta en BAJO: paro presionado, cable suelto o falta la conexion GPIO25->STBY.");
  }
}

void automatica() {
  const int pasos[] = {30, 60, 100};
  for (int p : pasos) {
    velocidad(p);
    for (int i = 0; i < 4; i++) {
      delay(1000);
      Serial.printf("  t=%ds  STBY=%s\n", i + 1, digitalRead(PIN_LECTURA) ? "ALTO" : "BAJO (paro)");
    }
  }
  velocidad(0);
  Serial.println("Prueba automatica terminada.");
}

void loop() {
  static unsigned long t = 0;
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') automatica();
    else if (c == 'f') velocidad(100);
    else if (c == 's') velocidad(0);
    else if (c >= '0' && c <= '9') velocidad((c - '0') * 10);
  }
  if (millis() - t >= 1000) {
    t = millis();
    Serial.printf("STBY (GPIO39) = %s\n", digitalRead(PIN_LECTURA) ? "ALTO (habilitado)" : "BAJO (paro)");
  }
}
