#include "interfaz.h"

#include <Wire.h>

#include "config.h"

// ---------------------------------------------------------------- Paro

static volatile bool avisoParo = false;
static void IRAM_ATTR isrParo() { avisoParo = true; }

void Paro::begin() {
  pinMode(PIN_PARO_LECTURA, INPUT);   // GPIO39: solo entrada; pull-down externo de 10 kΩ
  pinMode(PIN_PARO_SALIDA, OUTPUT);
  digitalWrite(PIN_PARO_SALIDA, HIGH);   // habilita el driver a traves del contacto NC
  delay(5);
  attachInterrupt(digitalPinToInterrupt(PIN_PARO_LECTURA), isrParo, FALLING);
  avisoParo = false;
}

bool Paro::presionado() const { return digitalRead(PIN_PARO_LECTURA) == LOW; }

bool Paro::consumirAviso() {
  if (!avisoParo) return false;
  avisoParo = false;
  return true;
}

// ---------------------------------------------------------------- Boton Restablecer

void BotonRestablecer::begin() {
  pinMode(PIN_RESTABLECER, INPUT);   // GPIO36: solo entrada; pull-up externo de 10 kΩ
  estable_ = lectura_ = digitalRead(PIN_RESTABLECER);
  tCambio_ = millis();
}

bool BotonRestablecer::pulsado() {
  bool ahora = digitalRead(PIN_RESTABLECER);
  if (ahora != lectura_) {
    lectura_ = ahora;
    tCambio_ = millis();
  }
  if (lectura_ != estable_ && millis() - tCambio_ >= ANTIRREBOTE_MS) {
    estable_ = lectura_;
    return estable_ == LOW;   // flanco de bajada estable = pulsacion
  }
  return false;
}

// ---------------------------------------------------------------- LEDs y buzzer

void Avisos::begin() {
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AMARILLO, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  leds(false, false, false);
  digitalWrite(PIN_BUZZER, LOW);
}

void Avisos::leds(bool verde, bool amarillo, bool rojo) {
  digitalWrite(PIN_LED_VERDE, verde);
  digitalWrite(PIN_LED_AMARILLO, amarillo);
  digitalWrite(PIN_LED_ROJO, rojo);
}

void Avisos::buzzer(PatronBuzzer p) {
  if (p == patron_ && p == BUZZER_INTERMITENTE) return;   // ya estaba sonando
  patron_ = p;
  tInicio_ = millis();
}

// Buzzer ACTIVO: basta con encender/apagar el transistor.
void Avisos::actualizar() {
  unsigned long t = millis() - tInicio_;
  bool sonar = false;
  switch (patron_) {
    case BUZZER_APAGADO: break;
    case BUZZER_CORTO:
      sonar = t < 120;
      if (t >= 120) patron_ = BUZZER_APAGADO;
      break;
    case BUZZER_DOBLE:
      sonar = t < 120 || (t >= 240 && t < 360);
      if (t >= 360) patron_ = BUZZER_APAGADO;
      break;
    case BUZZER_INTERMITENTE:
      sonar = (t % 800) < 300;
      break;
  }
  digitalWrite(PIN_BUZZER, sonar ? HIGH : LOW);
}

// ---------------------------------------------------------------- Pantalla

bool Pantalla::begin() {
  Wire.begin();   // SDA 21, SCL 22
  Wire.beginTransmission(OLED_DIRECCION);
  if (Wire.endTransmission() != 0) {   // nadie responde en esa direccion
    ok_ = false;
    return false;
  }
  oled_ = new Adafruit_SSD1306(OLED_ANCHO, OLED_ALTO, &Wire, -1);
  ok_ = oled_->begin(SSD1306_SWITCHCAPVCC, OLED_DIRECCION);
  if (ok_) {
    oled_->clearDisplay();
    oled_->setTextColor(SSD1306_WHITE);
    oled_->setTextSize(1);
    oled_->setCursor(0, 0);
    oled_->println("Banda clasificadora");
    oled_->println("Iniciando...");
    oled_->display();
  }
  return ok_;
}

void Pantalla::mostrar(const DatosPantalla& d, bool forzar) {
  if (!ok_) return;
  if (!forzar && millis() - tUltima_ < PANTALLA_MS) return;
  tUltima_ = millis();

  // 8 renglones de 21 caracteres con texto tamano 1
  oled_->clearDisplay();
  oled_->setCursor(0, 0);
  oled_->println(d.estado);
  oled_->drawFastHLine(0, 9, OLED_ANCHO, SSD1306_WHITE);
  oled_->setCursor(0, 12);
  oled_->printf("A:%lu B:%lu R:%lu\n", (unsigned long)d.a, (unsigned long)d.b, (unsigned long)d.r);
  oled_->printf("Sin leer: %lu\n", (unsigned long)d.sinLectura);
  oled_->printf("C %u/%u %u/%u %u/%u\n", d.contenido[0], d.capacidad[0], d.contenido[1],
                d.capacidad[1], d.contenido[2], d.capacidad[2]);
  if (d.enlace) {
    oled_->printf("Pi: OK  pend: %d\n", d.pendientes);
  } else {
    oled_->printf("SIN SUPERVISOR p:%d\n", d.pendientes);
  }
  oled_->println();
  oled_->println(d.linea ? d.linea : "");
  oled_->display();
}
