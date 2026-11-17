// actuadores.h — Motor de la banda (TB6612FNG) y compuerta doble (2 servos SG90 laterales)
//
// Motor: AIN1/AIN2 fijos por cableado, por eso solo se controla la velocidad (PWMA).
// La habilitacion del driver (STBY) la maneja paro.h, porque pasa por el boton de paro.

#pragma once
#include <Arduino.h>

class Motor {
 public:
  void begin();
  void iniciar(uint8_t porcentaje);   // 0..100
  void detener();
  bool enMarcha() const { return marcha_; }

 private:
  void escribir(uint32_t duty);
  bool marcha_ = false;
};

class Compuerta {
 public:
  void begin();                 // deja la compuerta al centro
  void posicionar(char categoria);   // 'A' izquierda, 'B' derecha, otro -> centro (pasa de frente)
  char posicion() const { return pos_; }

 private:
  void mover(int desplazamiento);    // mismo giro para los dos servos
  char pos_ = 'R';
};
