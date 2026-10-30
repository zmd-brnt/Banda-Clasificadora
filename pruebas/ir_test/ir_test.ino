// Prueba IR: diagnostico de los dos sensores infrarrojos (inspeccion y salida)
//
// Conexiones:
//   IR inspeccion OUT ----> ESP32 GPIO34
//   IR salida     OUT ----> ESP32 GPIO35
//   VCC de ambos  ----> ESP32 3V3   (asi su salida ya es de 3.3 V, sin divisor)
//   GND de ambos  ----> GND comun
//
//   GPIO34 y GPIO35 son solo de entrada y NO tienen pull-up interno.
//   El modulo FC-51 trae su propio pull-up en OUT.
//
// Por que se mide en milivoltios:
//   GPIO34/35 tambien son entradas del ADC. Leer el voltaje (no solo 0/1) permite
//   distinguir entre:
//     ~3300 mV  -> nivel ALTO   (FC-51: libre)
//     ~0 mV     -> nivel BAJO   (FC-51: detectando... o sin alimentacion / OUT a GND)
//     intermedio -> pin flotante, cable flojo o alimentacion inestable
//
// Modos (por Monitor Serial, 115200):
//   g -> prueba guiada: libre / tapar inspeccion / tapar salida, con diagnostico final
//   m -> monitor continuo: voltaje cada 500 ms y cada cambio de nivel al instante
//   Durante la prueba guiada se avanza con 'n' o Enter.
//
// Criterio de exito (para cada sensor):
//   - Libre: ALTO estable (>= 2300 mV), sin cambios de nivel.
//   - Tapado a ~3 cm: BAJO estable (<= 1000 mV).
//   - Al tapar un sensor, el otro NO cambia.
//   - Diagnostico final: "OK".

#define IR_INSP_PIN   34
#define IR_SALIDA_PIN 35

#define UMBRAL_BAJO_MV  1000
#define UMBRAL_ALTO_MV  2300
#define MUESTREO_MS        2
#define DURACION_PASO_MS 3000
#define MONITOR_MS        500

enum Nivel { BAJO, INDEFINIDO, ALTO };
const char* NOMBRES_NIVEL[] = {"BAJO", "INDEFINIDO", "ALTO"};

struct Sensor {
  uint8_t pin;
  const char* nombre;
};

Sensor sensores[2] = {
  {IR_INSP_PIN, "inspeccion"},
  {IR_SALIDA_PIN, "salida"},
};

struct Estadistica {
  uint32_t muestras;
  uint32_t conteo[3];     // BAJO, INDEFINIDO, ALTO
  uint32_t minMv;
  uint32_t maxMv;
  uint64_t sumaMv;
  uint32_t cambios;       // cambios de nivel durante el muestreo
};

bool modoMonitor = false;
Nivel nivelPrevio[2];
unsigned long tCambio[2];
unsigned long ultimoMonitor = 0;

Nivel clasificar(uint32_t mv) {
  if (mv <= UMBRAL_BAJO_MV) return BAJO;
  if (mv >= UMBRAL_ALTO_MV) return ALTO;
  return INDEFINIDO;
}

uint32_t leerMv(uint8_t pin) {
  return analogReadMilliVolts(pin);
}

// ---------------------------------------------------------------------
// Prueba guiada
// ---------------------------------------------------------------------

void muestrear(Estadistica est[2], uint32_t duracion) {
  Nivel previo[2];
  for (int i = 0; i < 2; i++) {
    est[i] = {0, {0, 0, 0}, UINT32_MAX, 0, 0, 0};
    previo[i] = clasificar(leerMv(sensores[i].pin));
  }

  unsigned long inicio = millis();
  while (millis() - inicio < duracion) {
    for (int i = 0; i < 2; i++) {
      uint32_t mv = leerMv(sensores[i].pin);
      Nivel n = clasificar(mv);
      est[i].muestras++;
      est[i].conteo[n]++;
      est[i].sumaMv += mv;
      if (mv < est[i].minMv) est[i].minMv = mv;
      if (mv > est[i].maxMv) est[i].maxMv = mv;
      // Solo cuenta cambios entre BAJO y ALTO; el paso por INDEFINIDO es normal en una transicion
      if (n != INDEFINIDO && previo[i] != INDEFINIDO && n != previo[i]) est[i].cambios++;
      if (n != INDEFINIDO) previo[i] = n;
    }
    delay(MUESTREO_MS);
  }
}

uint32_t porcentaje(const Estadistica& e, Nivel n) {
  return e.muestras ? (e.conteo[n] * 100) / e.muestras : 0;
}

// Nivel que aparece en al menos el 90 % de las muestras; INDEFINIDO si ninguno domina
Nivel predominante(const Estadistica& e) {
  if (porcentaje(e, ALTO) >= 90) return ALTO;
  if (porcentaje(e, BAJO) >= 90) return BAJO;
  return INDEFINIDO;
}

void imprimirEstadistica(const char* nombre, const Estadistica& e) {
  Serial.print("    IR ");
  Serial.print(nombre);
  Serial.print(": promedio ");
  Serial.print((uint32_t)(e.sumaMv / (e.muestras ? e.muestras : 1)));
  Serial.print(" mV  [min ");
  Serial.print(e.minMv);
  Serial.print(", max ");
  Serial.print(e.maxMv);
  Serial.print("]  ALTO ");
  Serial.print(porcentaje(e, ALTO));
  Serial.print("%  BAJO ");
  Serial.print(porcentaje(e, BAJO));
  Serial.print("%  INDEF ");
  Serial.print(porcentaje(e, INDEFINIDO));
  Serial.print("%  cambios ");
  Serial.print(e.cambios);
  Serial.print("  -> ");
  Serial.println(NOMBRES_NIVEL[predominante(e)]);
}

bool esperarAvance() {
  while (Serial.available()) Serial.read();
  Serial.println("    (envia 'n' o Enter cuando estes listo, 'q' para cancelar)");
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'q') return false;
      if (c == 'n' || c == '\n' || c == '\r') {
        delay(50);
        while (Serial.available()) Serial.read();
        return true;
      }
    }
    delay(10);
  }
}

// libre[i] = sensor i sin nada; tapado[i] = sensor i tapado; otro[i] = sensor i mientras se tapaba el OTRO
void diagnosticar(int i, const Estadistica& libre, const Estadistica& tapado, const Estadistica& otro) {
  Nivel nl = predominante(libre);
  Nivel nt = predominante(tapado);
  Nivel no = predominante(otro);

  Serial.print("  IR ");
  Serial.print(sensores[i].nombre);
  Serial.print(" (GPIO");
  Serial.print(sensores[i].pin);
  Serial.println("):");

  bool ok = true;

  if (porcentaje(libre, INDEFINIDO) > 10 || porcentaje(tapado, INDEFINIDO) > 10) {
    ok = false;
    Serial.println("    PROBLEMA: voltaje intermedio. Pin flotante, cable de senal flojo o");
    Serial.println("    alimentacion del modulo inestable. Medir VCC del modulo y continuidad OUT->GPIO.");
  }

  if (libre.cambios > 2) {
    ok = false;
    Serial.println("    PROBLEMA: oscila estando libre. Potenciometro en el limite, luz ambiental");
    Serial.println("    o un falso contacto. Girar el potenciometro hacia menos sensibilidad.");
  }

  if (nl == ALTO && nt == BAJO) {
    Serial.println("    Logica: activo en BAJO (como FC-51). Compatible con el codigo actual.");
  } else if (nl == BAJO && nt == ALTO) {
    ok = false;
    Serial.println("    PROBLEMA: activo en ALTO (logica invertida respecto a FC-51).");
    Serial.println("    Hay que invertir la lectura en el codigo, o confirmar el modelo del modulo.");
  } else if (nl == BAJO && nt == BAJO) {
    ok = false;
    Serial.println("    PROBLEMA: siempre en 0 V. No responde a la mano. Causas probables:");
    Serial.println("      1) el modulo no tiene alimentacion (LED de encendido apagado),");
    Serial.println("      2) potenciometro tan sensible que detecta la mesa o la estructura,");
    Serial.println("      3) OUT en corto con GND.");
  } else if (nl == ALTO && nt == ALTO) {
    ok = false;
    Serial.println("    PROBLEMA: siempre en 3.3 V. No detecta la mano. Acercarla (2-3 cm) o");
    Serial.println("    girar el potenciometro hacia mas sensibilidad. Verificar que el emisor no este tapado.");
  } else {
    ok = false;
    Serial.println("    PROBLEMA: lectura inconsistente (ningun nivel domina en algun paso).");
    Serial.println("    Repetir la prueba manteniendo la mano quieta; si se repite, revisar conexiones.");
  }

  if (no != nl) {
    ok = false;
    Serial.println("    PROBLEMA: cambio al tapar el OTRO sensor. Los dos estan acoplados:");
    Serial.println("    revisar VCC/GND compartidos (riel de protoboard, cable comun flojo) o que");
    Serial.println("    la mano no quede frente a ambos.");
  }

  Serial.println(ok ? "    RESULTADO: OK" : "    RESULTADO: REVISAR");
}

void pruebaGuiada() {
  Estadistica libre[2], tapaInsp[2], tapaSalida[2];

  Serial.println();
  Serial.println("=== Prueba guiada de los IR ===");

  Serial.println("Paso 1/3: retira TODO de ambos sensores.");
  if (!esperarAvance()) { Serial.println("Cancelada."); return; }
  Serial.println("  Midiendo 3 s...");
  muestrear(libre, DURACION_PASO_MS);
  imprimirEstadistica("inspeccion", libre[0]);
  imprimirEstadistica("salida", libre[1]);

  Serial.println("Paso 2/3: tapa SOLO el IR de INSPECCION con la mano a ~3 cm y no la muevas.");
  if (!esperarAvance()) { Serial.println("Cancelada."); return; }
  Serial.println("  Midiendo 3 s...");
  muestrear(tapaInsp, DURACION_PASO_MS);
  imprimirEstadistica("inspeccion", tapaInsp[0]);
  imprimirEstadistica("salida", tapaInsp[1]);

  Serial.println("Paso 3/3: tapa SOLO el IR de SALIDA con la mano a ~3 cm y no la muevas.");
  if (!esperarAvance()) { Serial.println("Cancelada."); return; }
  Serial.println("  Midiendo 3 s...");
  muestrear(tapaSalida, DURACION_PASO_MS);
  imprimirEstadistica("inspeccion", tapaSalida[0]);
  imprimirEstadistica("salida", tapaSalida[1]);

  Serial.println();
  Serial.println("=== Diagnostico ===");
  diagnosticar(0, libre[0], tapaInsp[0], tapaSalida[0]);
  diagnosticar(1, libre[1], tapaSalida[1], tapaInsp[1]);
  Serial.println();
  Serial.println("Comandos: g=prueba guiada  m=monitor continuo");
}

// ---------------------------------------------------------------------
// Monitor continuo
// ---------------------------------------------------------------------

void iniciarMonitor() {
  for (int i = 0; i < 2; i++) {
    nivelPrevio[i] = clasificar(leerMv(sensores[i].pin));
    tCambio[i] = millis();
  }
  ultimoMonitor = 0;
}

void monitor() {
  for (int i = 0; i < 2; i++) {
    uint32_t mv = leerMv(sensores[i].pin);
    Nivel n = clasificar(mv);
    if (n != nivelPrevio[i]) {
      Serial.print("[");
      Serial.print(millis());
      Serial.print(" ms] IR ");
      Serial.print(sensores[i].nombre);
      Serial.print(": ");
      Serial.print(NOMBRES_NIVEL[nivelPrevio[i]]);
      Serial.print(" -> ");
      Serial.print(NOMBRES_NIVEL[n]);
      Serial.print(" (");
      Serial.print(mv);
      Serial.print(" mV, estuvo ");
      Serial.print(millis() - tCambio[i]);
      Serial.println(" ms en el nivel anterior)");
      nivelPrevio[i] = n;
      tCambio[i] = millis();
    }
  }

  if (millis() - ultimoMonitor >= MONITOR_MS) {
    ultimoMonitor = millis();
    for (int i = 0; i < 2; i++) {
      uint32_t mv = leerMv(sensores[i].pin);
      Serial.print("IR ");
      Serial.print(sensores[i].nombre);
      Serial.print(": ");
      Serial.print(mv);
      Serial.print(" mV ");
      Serial.print(NOMBRES_NIVEL[clasificar(mv)]);
      Serial.print(i == 0 ? "   |   " : "\n");
    }
  }
}

// ---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(IR_INSP_PIN, INPUT);
  pinMode(IR_SALIDA_PIN, INPUT);
  delay(200);

  Serial.println();
  Serial.println("=== Prueba IR: inspeccion (GPIO34) y salida (GPIO35) ===");
  Serial.println("Umbrales: BAJO <= 1000 mV, ALTO >= 2300 mV, intermedio = INDEFINIDO");
  Serial.println("Comandos: g=prueba guiada  m=monitor continuo (m otra vez para pausar)");
  Serial.println();

  modoMonitor = true;
  iniciarMonitor();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'g') {
      modoMonitor = false;
      pruebaGuiada();
    } else if (c == 'm') {
      modoMonitor = !modoMonitor;
      if (modoMonitor) iniciarMonitor();
      Serial.println(modoMonitor ? "Monitor continuo activado" : "Monitor en pausa");
    }
  }

  if (modoMonitor) {
    monitor();
  }
  delay(MUESTREO_MS);
}
