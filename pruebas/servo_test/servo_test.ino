// Prueba compuerta doble: DOS servos SG90 en los laterales de la banda
//
// Idea: cada servo lleva un brazo (paleta) en un lado de la banda. En reposo los dos brazos
// quedan paralelos a la banda (pegados a los laterales) y la pieza pasa de frente -> RECHAZO.
// Para clasificar, los DOS brazos giran AL MISMO TIEMPO y EN EL MISMO SENTIDO. Asi quedan
// paralelos entre si (como un canal diagonal) y nunca chocan:
//
//      Vista desde arriba (la banda avanza hacia abajo)
//
//      CENTRO (rechazo)      IZQUIERDA (cat. A)       DERECHA (cat. B)
//      |  S1      S2  |      |  S1      S2  |         |  S1      S2  |
//      |  |        |  |      |   /       /  |         |  \       \   |
//      |  |        |  |      |  /       /   |         |   \       \  |
//      |  |  pieza |  |      | <- se va a la izq.     |   se va a la der. ->
//      v  v        v  v
//
//   El brazo del lado contrario al desvio es el que empuja; el otro se abre hacia fuera.
//
// Conexiones (docs/conexiones.md §5.4):
//   Servo 1 (lateral IZQUIERDO) senal -> GPIO13
//   Servo 2 (lateral DERECHO)   senal -> GPIO12   (ver aviso de arranque abajo)
//   Rojo de ambos  -> +5 V rama de POTENCIA (NO el pin 5V/3V3 del ESP32)
//   Cafe de ambos  -> GND comun (fuente + ESP32)
//   Capacitor 1000 uF / 16 V entre +5 V y GND, cerca de los servos.
//
// AVISO GPIO12: es pin de arranque. Si estuviera en ALTO al encender, el ESP32 no arranca.
//   La entrada de senal del SG90 no lo sube, pero COMPRUEBALO: con el servo 2 conectado,
//   presiona EN/RST; el ESP32 debe arrancar y mostrar el encabezado. Si no arranca,
//   desconecta el servo 2 de GPIO12 y conecta su senal tambien a GPIO13 (en paralelo con
//   el servo 1): se moveran igual, pero ya no se podra calibrar cada uno por separado.
//
// Comandos (Monitor Serial a 115200, con "Nueva linea"):
//   i -> categoria A: desviar a la IZQUIERDA
//   d -> categoria B: desviar a la DERECHA
//   c -> centro: la pieza pasa de frente (rechazo / sin categoria)
//   <angulo>    -> mueve los DOS servos a ese angulo (0..180)
//   1 <angulo>  -> solo el servo 1 (para calibrar)     ej. "1 95"
//   2 <angulo>  -> solo el servo 2 (para calibrar)     ej. "2 88"
//   p -> imprime la calibracion actual
//
// Calibracion (con los brazos montados):
//   1. Envia "c". Si algun brazo no queda paralelo a la banda, busca su angulo con "1 <ang>"
//      o "2 <ang>" y copialo en CENTRO_S1 / CENTRO_S2.
//   2. Envia "i" y "d". Ajusta DESVIO_GRADOS hasta que el brazo que empuja cruce la banda
//      lo suficiente para desviar la pieza (normalmente 35..55 grados).
//   3. Si con "i" los brazos se van a la DERECHA, cambia SENTIDO_IZQ de +1 a -1.
//   4. Si un servo esta montado al reves (eje hacia abajo), pon su INVERTIR_Sx en true:
//      sin esto los brazos girarian en sentidos opuestos y CHOCARIAN.
//
// Criterio de exito:
//   - En "c" los dos brazos quedan pegados a los laterales y la pieza pasa de frente.
//   - En "i"/"d" los dos brazos giran a la vez, quedan paralelos y no se tocan.
//   - La pieza real termina en el contenedor correcto en 10 de 10 intentos por lado.
//   - El ESP32 no se reinicia al mover los dos servos juntos.

#include <esp_arduino_version.h>

#define PIN_SERVO1 13    // lateral izquierdo
#define PIN_SERVO2 12    // lateral derecho

#define PWM_FREQ   50    // 50 Hz = periodo de 20 ms
#define PWM_RESOL  16    // 0..65535
#define CANAL_S1   0     // solo core 2.x (canales 0 y 1 comparten temporizador a 50 Hz)
#define CANAL_S2   1

// ---- Calibracion (ajustar con la compuerta montada) ----
const int  CENTRO_S1     = 90;     // angulo en el que el brazo 1 queda paralelo a la banda
const int  CENTRO_S2     = 90;     // idem brazo 2
const int  DESVIO_GRADOS = 45;     // cuanto giran los brazos para desviar
const int  SENTIDO_IZQ   = +1;     // +1 o -1: hacia donde gira para mandar a la izquierda
const bool INVERTIR_S1   = false;  // true si el servo 1 esta montado al reves
const bool INVERTIR_S2   = false;  // true si el servo 2 esta montado al reves

// Pulso del SG90: ~500 us = 0 grados, ~2400 us = 180 grados (igual que la prueba anterior)
#define PULSO_MIN_US 500
#define PULSO_MAX_US 2400

uint32_t anguloADuty(int ang) {
  ang = constrain(ang, 0, 180);
  uint32_t us = PULSO_MIN_US + (uint32_t)(PULSO_MAX_US - PULSO_MIN_US) * ang / 180;
  return us * 65535UL / 20000UL;
}

void escribir(int pin, int canal, int ang) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(pin, anguloADuty(ang));
#else
  ledcWrite(canal, anguloADuty(ang));
#endif
}

// Angulo absoluto de cada servo a partir de un desplazamiento comun respecto al centro.
// Ambos reciben el MISMO desplazamiento -> giran en el mismo sentido y no chocan.
int anguloServo(int centro, bool invertir, int desplazamiento) {
  return centro + (invertir ? -desplazamiento : desplazamiento);
}

void moverCompuerta(int desplazamiento, const char* nombre) {
  int a1 = anguloServo(CENTRO_S1, INVERTIR_S1, desplazamiento);
  int a2 = anguloServo(CENTRO_S2, INVERTIR_S2, desplazamiento);
  escribir(PIN_SERVO1, CANAL_S1, a1);   // uno inmediatamente despues del otro:
  escribir(PIN_SERVO2, CANAL_S2, a2);   // la diferencia es de microsegundos
  Serial.printf(">> %s   servo1=%d°  servo2=%d°\n", nombre, constrain(a1, 0, 180), constrain(a2, 0, 180));
}

void izquierda() { moverCompuerta( SENTIDO_IZQ * DESVIO_GRADOS, "IZQUIERDA (categoria A)"); }
void derecha()   { moverCompuerta(-SENTIDO_IZQ * DESVIO_GRADOS, "DERECHA (categoria B)"); }
void centro()    { moverCompuerta(0, "CENTRO (rechazo, pasa de frente)"); }

void imprimirCalibracion() {
  Serial.printf("CENTRO_S1=%d  CENTRO_S2=%d  DESVIO=%d  SENTIDO_IZQ=%+d  INVERTIR_S1=%d  INVERTIR_S2=%d\n",
                CENTRO_S1, CENTRO_S2, DESVIO_GRADOS, SENTIDO_IZQ, INVERTIR_S1, INVERTIR_S2);
}

void setup() {
  Serial.begin(115200);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_SERVO1, PWM_FREQ, PWM_RESOL);
  ledcAttach(PIN_SERVO2, PWM_FREQ, PWM_RESOL);
#else
  ledcSetup(CANAL_S1, PWM_FREQ, PWM_RESOL);
  ledcSetup(CANAL_S2, PWM_FREQ, PWM_RESOL);
  ledcAttachPin(PIN_SERVO1, CANAL_S1);
  ledcAttachPin(PIN_SERVO2, CANAL_S2);
#endif
  delay(300);
  Serial.println();
  Serial.println("=== Prueba compuerta doble (2x SG90) ===");
  imprimirCalibracion();
  centro();
  delay(1000);

  Serial.println("Ciclo automatico: izquierda -> centro -> derecha -> centro (x2)");
  for (int ciclo = 0; ciclo < 2; ciclo++) {
    izquierda(); delay(1500);
    centro();    delay(1500);
    derecha();   delay(1500);
    centro();    delay(1500);
  }

  Serial.println();
  Serial.println("Control manual: i = izquierda  d = derecha  c = centro");
  Serial.println("  <ang> = ambos   '1 <ang>' / '2 <ang>' = uno solo   p = calibracion");
}

void loop() {
  if (!Serial.available()) return;
  String e = Serial.readStringUntil('\n');
  e.trim();
  if (e.length() == 0) return;

  if (e == "i" || e == "I")      izquierda();
  else if (e == "d" || e == "D") derecha();
  else if (e == "c" || e == "C") centro();
  else if (e == "p" || e == "P") imprimirCalibracion();
  else if ((e.startsWith("1 ") || e.startsWith("2 ")) && e.length() > 2) {
    int ang = e.substring(2).toInt();
    if (ang < 0 || ang > 180) { Serial.println("Angulo fuera de 0..180"); return; }
    if (e[0] == '1') escribir(PIN_SERVO1, CANAL_S1, ang);
    else             escribir(PIN_SERVO2, CANAL_S2, ang);
    Serial.printf(">> Solo servo %c = %d°\n", e[0], ang);
  } else if (isDigit(e[0])) {
    int ang = e.toInt();
    if (ang < 0 || ang > 180) { Serial.println("Angulo fuera de 0..180"); return; }
    escribir(PIN_SERVO1, CANAL_S1, ang);
    escribir(PIN_SERVO2, CANAL_S2, ang);
    Serial.printf(">> Ambos servos = %d°\n", ang);
  } else {
    Serial.println("Entrada no valida. Usa i, d, c, p, <ang>, '1 <ang>' o '2 <ang>'.");
  }
}
