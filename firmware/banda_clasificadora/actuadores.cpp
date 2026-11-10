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

void Compuerta::begin() {
  pwmConfigurar(PIN_SERVO, SERVO_CANAL, SERVO_FREQ, SERVO_RESOL);
  posicionar('R');
}

void Compuerta::posicionar(char categoria) {
  uint32_t duty = categoria == 'A' ? SERVO_DUTY_A : categoria == 'B' ? SERVO_DUTY_B : SERVO_DUTY_R;
  pos_ = (categoria == 'A' || categoria == 'B') ? categoria : 'R';
  pwmEscribir(PIN_SERVO, SERVO_CANAL, duty);
}
