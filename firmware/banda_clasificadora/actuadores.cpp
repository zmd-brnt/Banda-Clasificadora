#include "actuadores.h"

#include <esp_arduino_version.h>

#include "config.h"

// La API de LEDC cambio en el core 3.x de arduino-esp32 (por pin en vez de por canal).
static void pwmConfigurar(uint8_t pin, uint8_t canal, uint32_t freq, uint8_t resol) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  (void)canal;
  ledcAttach(pin, freq, resol);
#else
  ledcSetup(canal, freq, resol);
  ledcAttachPin(pin, canal);
#endif
}

static void pwmEscribir(uint8_t pin, uint8_t canal, uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  (void)canal;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(canal, duty);
#endif
}

// ---------------------------------------------------------------- Motor

void Motor::begin() {
  pwmConfigurar(PIN_MOTOR_PWMA, MOTOR_CANAL, MOTOR_FREQ, MOTOR_RESOL);
  detener();
}

void Motor::escribir(uint32_t duty) { pwmEscribir(PIN_MOTOR_PWMA, MOTOR_CANAL, duty); }

void Motor::iniciar(uint8_t porcentaje) {
  if (porcentaje > 100) porcentaje = 100;
  const uint32_t maximo = (1u << MOTOR_RESOL) - 1;
  escribir(maximo * porcentaje / 100);
  marcha_ = porcentaje > 0;
}

void Motor::detener() {
  escribir(0);
  marcha_ = false;
}

// ---------------------------------------------------------------- Compuerta

static uint32_t anguloADuty(int ang) {
  ang = constrain(ang, 0, 180);
  const uint32_t us = SERVO_PULSO_MIN_US + (uint32_t)(SERVO_PULSO_MAX_US - SERVO_PULSO_MIN_US) * ang / 180;
  const uint32_t periodoUs = 1000000UL / SERVO_FREQ;
  return us * ((1UL << SERVO_RESOL) - 1) / periodoUs;
}

void Compuerta::begin() {
  pwmConfigurar(PIN_SERVO1, SERVO_CANAL1, SERVO_FREQ, SERVO_RESOL);
  pwmConfigurar(PIN_SERVO2, SERVO_CANAL2, SERVO_FREQ, SERVO_RESOL);
  posicionar('R');
}

// Ambos servos reciben el MISMO desplazamiento respecto a su centro, uno inmediatamente
// despues del otro (microsegundos): giran juntos y en el mismo sentido, sin chocar.
void Compuerta::mover(int desplazamiento) {
  const int a1 = SERVO_CENTRO1 + (SERVO_INVERTIR1 ? -desplazamiento : desplazamiento);
  const int a2 = SERVO_CENTRO2 + (SERVO_INVERTIR2 ? -desplazamiento : desplazamiento);
  pwmEscribir(PIN_SERVO1, SERVO_CANAL1, anguloADuty(a1));
  pwmEscribir(PIN_SERVO2, SERVO_CANAL2, anguloADuty(a2));
}

void Compuerta::posicionar(char categoria) {
  int desplazamiento = 0;                                            // centro: pasa de frente
  if (categoria == 'A') desplazamiento = SERVO_SENTIDO_IZQ * SERVO_DESVIO;    // izquierda
  else if (categoria == 'B') desplazamiento = -SERVO_SENTIDO_IZQ * SERVO_DESVIO;  // derecha
  pos_ = (categoria == 'A' || categoria == 'B') ? categoria : 'R';
  mover(desplazamiento);
}
