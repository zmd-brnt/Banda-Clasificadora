// Prueba Servo: Movimiento del SG90 en tres posiciones
//
// Conexiones:
//   ESP32 GPIO13 ----> Servo signal (cable naranja o blanco)
//   Fuente 5V    ----> Servo Vcc    (cable rojo)
//   GND comun    ----> Servo GND    (cable cafe o negro)
//   ESP32 GND    ----> GND comun
//
// IMPORTANTE sobre la alimentacion:
//   El SG90 puede consumir hasta 750mA en movimiento.
//   NO alimentar el servo desde el pin 5V del ESP32 en pruebas largas.
//   Mejor usar una fuente externa de 5V (cargador USB, por ejemplo)
//   con tierra comun al ESP32.
//   Para una prueba rapida sin carga, el USB del ESP32 puede funcionar,
//   pero si el servo tiembla o el ESP32 se reinicia, necesitas fuente externa.
//
// Posiciones de la compuerta:
//   0°   -> Categoria A (izquierda)
//   90°  -> Centro / posicion neutral
//   180° -> Categoria B (derecha)
//
// Funcionamiento:
//   Cicla automaticamente entre las tres posiciones cada 2 segundos.
//   Muestra la posicion actual en el Monitor Serial.
//   Despues de 3 ciclos, permite control manual por Monitor Serial.
//
// Criterio de exito:
//   - El servo se mueve suavemente a tres posiciones distintas.
//   - Siempre regresa a la misma posicion para el mismo angulo.
//   - No tiembla excesivamente en reposo.
//   - Si tiembla o el ESP32 se reinicia, la alimentacion es insuficiente.

#define SERVO_PIN 13

// Parametros PWM del ESP32 para servomotores
#define PWM_FREQ     50      // 50 Hz = periodo de 20ms (estandar servo)
#define PWM_RESOL    16      // 16 bits de resolucion (0-65535)
#define PWM_CHANNEL  0

// Pulsos del SG90 (microsegundos):
//   ~500us  = 0 grados
//   ~1500us = 90 grados
//   ~2400us = 180 grados
// Convertidos a duty cycle de 16 bits a 50Hz (periodo = 20000us):
//   duty = (pulso_us / 20000) * 65535
#define DUTY_0    1638    // ~500us  -> 0 grados
#define DUTY_90   4915    // ~1500us -> 90 grados
#define DUTY_180  7864    // ~2400us -> 180 grados

struct Posicion {
  const char* nombre;
  uint32_t duty;
  int angulo;
};

Posicion posiciones[] = {
  {"Categoria A (0°)",    DUTY_0,   0},
  {"Centro (90°)",        DUTY_90,  90},
  {"Categoria B (180°)",  DUTY_180, 180},
};

#include <esp_arduino_version.h>

void moverServo(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

void irAPosicion(int indice) {
  Serial.print("  Moviendo a: ");
  Serial.print(posiciones[indice].nombre);
  Serial.print("  [duty=");
  Serial.print(posiciones[indice].duty);
  Serial.println("]");

  moverServo(posiciones[indice].duty);
}

void setup() {
  Serial.begin(115200);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RESOL);
#else
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOL);
  ledcAttachPin(SERVO_PIN, PWM_CHANNEL);
#endif

  Serial.println("=== Prueba Servo SG90 ===");
  Serial.println();
  Serial.println("Fase 1: Ciclo automatico (3 repeticiones)");
  Serial.println("  Observa que el servo se mueva a tres posiciones distintas.");
  Serial.println();

  // Fase 1: Ciclo automatico
  for (int ciclo = 1; ciclo <= 3; ciclo++) {
    Serial.print("Ciclo ");
    Serial.print(ciclo);
    Serial.println("/3:");

    for (int i = 0; i < 3; i++) {
      irAPosicion(i);
      delay(2000);
    }

    Serial.println("---");
  }

  // Regresar al centro
  Serial.println("Regresando al centro...");
  moverServo(DUTY_90);
  delay(1000);

  Serial.println();
  Serial.println("Fase 2: Control manual");
  Serial.println("  Envia por Monitor Serial:");
  Serial.println("    'a' -> Categoria A (0°)");
  Serial.println("    'c' -> Centro (90°)");
  Serial.println("    'b' -> Categoria B (180°)");
  Serial.println("    Un numero 0-180 -> angulo especifico");
  Serial.println();
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String entrada = Serial.readStringUntil('\n');
  entrada.trim();

  if (entrada.length() == 0) {
    return;
  }

  if (entrada == "a" || entrada == "A") {
    Serial.println(">> Categoria A");
    irAPosicion(0);
  } else if (entrada == "c" || entrada == "C") {
    Serial.println(">> Centro");
    irAPosicion(1);
  } else if (entrada == "b" || entrada == "B") {
    Serial.println(">> Categoria B");
    irAPosicion(2);
  } else {
    int angulo = entrada.toInt();
    if (angulo >= 0 && angulo <= 180) {
      uint32_t duty = map(angulo, 0, 180, DUTY_0, DUTY_180);
      Serial.print(">> Angulo personalizado: ");
      Serial.print(angulo);
      Serial.print("°  [duty=");
      Serial.print(duty);
      Serial.println("]");
      moverServo(duty);
    } else {
      Serial.println("Entrada no valida. Usa 'a', 'c', 'b', o un numero 0-180.");
    }
  }
}
