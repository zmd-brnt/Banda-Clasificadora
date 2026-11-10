// Prueba HC-SR04: los dos sensores ultrasonicos (inspeccion y salida)
//
// Sustituye a pruebas/ir_test (los IR se cambiaron por HC-SR04).
//
// Conexiones (docs/conexiones.md §5.3):
//   HC-SR04 inspeccion: VCC -> +5 V rama logica, GND -> GND comun,
//                       Trig -> GPIO14, Echo -> divisor (1 kΩ / 2 kΩ) -> GPIO34
//   HC-SR04 salida:     VCC -> +5 V rama logica, GND -> GND comun,
//                       Trig -> GPIO27, Echo -> divisor (1 kΩ / 2 kΩ) -> GPIO35
//
// ANTES DE CONECTAR EL ECHO AL ESP32 (paso obligatorio, se hace con multimetro):
//   1. Arma el divisor SIN conectarlo al HC-SR04: entrada del divisor a +5 V.
//   2. Mide el punto medio (lo que ira a GPIO34/35) contra GND.
//   3. Debe dar <= 3.3 V (con 1 kΩ / 2 kΩ: ~3.3 V). Si da mas, NO lo conectes: revisa las resistencias.
//   Despues conecta la entrada del divisor al pin Echo del HC-SR04.
//
// Modos (Monitor Serial a 115200, escribe la letra y Enter):
//   m -> monitor: distancia de ambos sensores, medidos por turnos, cada ~100 ms
//   c -> calibracion guiada: 30 mediciones con la banda libre y 30 con la pieza real frente
//        al sensor; muestra minimo, maximo, promedio, mediciones sin eco y un umbral sugerido
//   x -> interferencia: compara el sensor de salida medido por turnos contra medido justo
//        despues de disparar el de inspeccion (si cambia mucho, se "oyen" entre si)
//   Cualquier otra tecla detiene el modo actual.
//
// Criterios de exito (para cada sensor, con el montaje real):
//   - Libre: distancia estable y MAYOR que el umbral; 0 o casi 0 mediciones sin eco.
//   - Con la pieza real a la distancia de montaje: distancia estable y MENOR que el umbral.
//   - La diferencia entre ambos casos es clara (los rangos de libre y pieza no se traslapan).
//   - La prueba de interferencia no cambia de forma notable las mediciones del sensor de salida.
//   - Anota la distancia minima a la que tu modulo todavia mide bien (acerca la pieza poco a poco).
//   Los umbrales obtenidos se copian a UMBRAL_INSP_CM y UMBRAL_SALIDA_CM en
//   firmware/banda_clasificadora/config.h.

#include "estadistica.h"

#define TRIG_INSP   14
#define ECHO_INSP   34
#define TRIG_SALIDA 27
#define ECHO_SALIDA 35

// Tiempo maximo de espera del eco. 25 ms ~ 4 m; si no llega, se cuenta como "sin eco".
#define TIMEOUT_US  25000UL
#define MUESTRAS    30

const uint8_t TRIG[2] = {TRIG_INSP, TRIG_SALIDA};
const uint8_t ECHO[2] = {ECHO_INSP, ECHO_SALIDA};
const char* NOMBRE[2] = {"inspeccion", "salida"};

char modo = 0;

// Distancia en cm, o -1 si no hubo eco.
float medir(int s) {
  digitalWrite(TRIG[s], LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG[s], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[s], LOW);
  unsigned long us = pulseIn(ECHO[s], HIGH, TIMEOUT_US);
  if (us == 0) return -1;
  return us * 0.0343f / 2.0f;
}

Estadistica tomarMuestras(int s) {
  Estadistica e = {1e9, -1, 0, 0, 0};
  for (int i = 0; i < MUESTRAS; i++) {
    float d = medir(s);
    if (d < 0) {
      e.sinEco++;
    } else {
      e.validas++;
      e.suma += d;
      if (d < e.min) e.min = d;
      if (d > e.max) e.max = d;
    }
    delay(60);   // dejar que se apaguen los ecos antes de la siguiente medicion
  }
  return e;
}

void imprimir(const char* titulo, const Estadistica& e) {
  Serial.printf("  %s: ", titulo);
  if (e.validas == 0) {
    Serial.printf("ninguna medicion valida (%d sin eco)\n", e.sinEco);
    return;
  }
  Serial.printf("min %.1f  max %.1f  promedio %.1f cm  (%d validas, %d sin eco)\n", e.min, e.max,
                e.suma / e.validas, e.validas, e.sinEco);
}

void esperarEnter(const char* instruccion) {
  Serial.println(instruccion);
  Serial.println("  ...presiona Enter cuando este listo");
  while (Serial.available()) Serial.read();
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();
}

void calibrar() {
  for (int s = 0; s < 2; s++) {
    Serial.printf("\n=== Calibracion del sensor de %s ===\n", NOMBRE[s]);
    esperarEnter("1) Deja la banda LIBRE frente al sensor (sin pieza).");
    Estadistica libre = tomarMuestras(s);
    imprimir("Libre", libre);
    esperarEnter("2) Coloca la PIEZA REAL frente al sensor, donde quedara durante la operacion.");
    Estadistica pieza = tomarMuestras(s);
    imprimir("Pieza", pieza);

    if (pieza.validas == 0) {
      Serial.println("  RESULTADO: el sensor no ve la pieza. Revisa orientacion, distancia minima y tamano de la pieza.");
      continue;
    }
    // "Libre" puede no tener eco (nada enfrente): eso tambien cuenta como libre
    float minLibre = libre.validas ? libre.min : 1e9;
    if (pieza.max < minLibre) {
      float umbral = libre.validas ? (pieza.max + minLibre) / 2 : pieza.max + 2.0f;
      Serial.printf("  RESULTADO: OK. Umbral sugerido para %s: %.1f cm\n", NOMBRE[s], umbral);
    } else {
      Serial.println("  RESULTADO: los rangos de libre y pieza se traslapan: no hay umbral confiable.");
      Serial.println("  Revisa que el sensor no vea la banda o la estructura y que la pieza quede de frente.");
    }
    if (pieza.sinEco > 0) {
      Serial.println("  AVISO: hubo mediciones sin eco con la pieza puesta (pieza chica, inclinada o blanda?).");
    }
  }
  Serial.println("\nCalibracion terminada. Copia los umbrales a firmware/banda_clasificadora/config.h");
  modo = 0;
}

void interferencia() {
  Serial.println("\n=== Interferencia entre sensores ===");
  Serial.println("Deja el montaje como en operacion (con o sin pieza, igual en ambas partes).");
  Serial.println("A) Sensor de salida medido solo (por turnos):");
  Estadistica solo = tomarMuestras(1);
  imprimir("Salida solo", solo);

  Serial.println("B) Sensor de salida medido 1 ms despues de disparar el de inspeccion:");
  Estadistica e = {1e9, -1, 0, 0, 0};
  for (int i = 0; i < MUESTRAS; i++) {
    digitalWrite(TRIG[0], HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG[0], LOW);
    delay(1);
    float d = medir(1);
    if (d < 0) e.sinEco++;
    else {
      e.validas++;
      e.suma += d;
      if (d < e.min) e.min = d;
      if (d > e.max) e.max = d;
    }
    delay(60);
  }
  imprimir("Salida con inspeccion activa", e);
  Serial.println("Si B difiere mucho de A, los sensores se oyen entre si: el firmware ya los mide por");
  Serial.println("turnos, pero conviene orientarlos para que no se apunten.");
  modo = 0;
}

void setup() {
  Serial.begin(115200);
  for (int s = 0; s < 2; s++) {
    pinMode(TRIG[s], OUTPUT);
    digitalWrite(TRIG[s], LOW);
    pinMode(ECHO[s], INPUT);   // GPIO34/35: solo entrada
  }
  delay(300);
  Serial.println();
  Serial.println("=== Prueba HC-SR04 (inspeccion GPIO14/34, salida GPIO27/35) ===");
  Serial.println("Recuerda: el Echo pasa por el divisor y ya mediste <= 3.3 V (ver encabezado).");
  Serial.println("m = monitor   c = calibracion guiada   x = interferencia");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'm' || c == 'c' || c == 'x') modo = c;
    else if (c != '\n' && c != '\r') {
      modo = 0;
      Serial.println("Detenido. m / c / x");
    }
  }

  if (modo == 'c') calibrar();
  else if (modo == 'x') interferencia();
  else if (modo == 'm') {
    float a = medir(0);
    delay(50);
    float b = medir(1);
    Serial.printf("inspeccion: %7s cm   salida: %7s cm\n", a < 0 ? "sin eco" : String(a, 1).c_str(),
                  b < 0 ? "sin eco" : String(b, 1).c_str());
    delay(50);
  }
}
