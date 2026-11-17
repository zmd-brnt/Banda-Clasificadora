// interfaz.h — Paro de emergencia, boton Restablecer, LEDs, buzzer y pantalla OLED
//
// Paro (docs/conexiones.md §5.7): GPIO25 pone ALTO el nodo STBY del TB6612 a traves del
// contacto NC del boton. Al presionar el paro, el contacto se abre, la resistencia de
// 10 kΩ lleva STBY a BAJO y el driver apaga el motor POR HARDWARE, aunque el programa este
// trabado. GPIO39 lee ese nodo: BAJO con GPIO25 en ALTO = paro presionado (o cable cortado).
// Los servos NO se cortan por hardware: el programa los lleva al centro.

#pragma once
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

class Paro {
 public:
  void begin();             // habilita STBY (GPIO25 = ALTO) y la interrupcion de GPIO39
  bool presionado() const;  // nivel actual del nodo STBY
  bool consumirAviso();     // true una vez si hubo un flanco de bajada desde la ultima llamada
};

class BotonRestablecer {
 public:
  void begin();
  // true una sola vez por cada pulsacion (con antirrebote). Llamar en cada vuelta de loop().
  bool pulsado();

 private:
  bool estable_ = true;     // ALTO = sin presionar (pull-up externo)
  bool lectura_ = true;
  unsigned long tCambio_ = 0;
};

enum PatronBuzzer { BUZZER_APAGADO, BUZZER_CORTO, BUZZER_DOBLE, BUZZER_INTERMITENTE };

class Avisos {
 public:
  void begin();
  void leds(bool verde, bool amarillo, bool rojo);
  void buzzer(PatronBuzzer p);   // CORTO y DOBLE suenan una vez; INTERMITENTE hasta apagarlo
  void actualizar();             // llamar en cada vuelta de loop()

 private:
  PatronBuzzer patron_ = BUZZER_APAGADO;
  unsigned long tInicio_ = 0;
};

struct DatosPantalla {
  const char* estado;
  uint32_t a, b, r, sinLectura;
  uint16_t contenido[3];   // A, B, R
  uint16_t capacidad[3];
  bool enlace;
  int pendientes;
  const char* linea;       // ultimo evento o aviso
};

class Pantalla {
 public:
  bool begin();   // false si no responde en el bus I2C (el sistema sigue sin pantalla)
  bool ok() const { return ok_; }
  void mostrar(const DatosPantalla& d, bool forzar = false);

 private:
  Adafruit_SSD1306* oled_ = nullptr;
  bool ok_ = false;
  unsigned long tUltima_ = 0;
};
