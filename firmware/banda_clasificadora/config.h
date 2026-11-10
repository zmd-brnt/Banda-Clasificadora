// config.h — Pines, tiempos y umbrales del firmware de la banda clasificadora
//
// Pines: docs/conexiones.md §3 (con las decisiones HC-SR04 y AIN1/AIN2 fijos).
//
// IMPORTANTE: los valores marcados [PROVISIONAL] NO estan medidos. Son un punto de partida
// para que el firmware funcione en la mesa; se reemplazan con las mediciones del montaje
// real (velocidad de la banda, distancias, umbrales de los HC-SR04). Ver docs/mecanica.md
// cuando exista y la seccion "Calibracion" de firmware/README.md.

#pragma once

// ---------------------------------------------------------------- Pines
// RC522 (SPI VSPI)
#define PIN_RC522_SS     5
#define PIN_RC522_RST    4
// SCK 18, MOSI 23, MISO 19: bus VSPI por defecto (SPI.begin())

// OLED SSD1306 (I2C por defecto: SDA 21, SCL 22)
#define OLED_DIRECCION   0x3C   // algunas son 0x3D: confirmar con pruebas/oled_test
#define OLED_ANCHO       128
#define OLED_ALTO        64

// HC-SR04 (Echo SIEMPRE a traves del divisor de voltaje, docs/conexiones.md §5.3)
#define PIN_TRIG_INSP    14
#define PIN_ECHO_INSP    34
#define PIN_TRIG_SALIDA  27
#define PIN_ECHO_SALIDA  35

// Servo SG90
#define PIN_SERVO        13

// TB6612FNG: AIN1 = 3.3 V y AIN2 = GND fijos por cableado (solo avanza)
#define PIN_MOTOR_PWMA   26

// Paro de emergencia: GPIO25 alimenta STBY a traves del contacto NC; GPIO39 lee el nodo STBY
#define PIN_PARO_SALIDA  25
#define PIN_PARO_LECTURA 39

// Boton Restablecer / Arranque (pull-up externo de 10 kΩ, activo en BAJO)
#define PIN_RESTABLECER  36

// LEDs y buzzer (buzzer ACTIVO, a traves de transistor NPN)
#define PIN_LED_VERDE    32
#define PIN_LED_AMARILLO 33
#define PIN_LED_ROJO     2
#define PIN_BUZZER       15

// UART2 hacia la Raspberry Pi
#define PIN_UART_TX      17
#define PIN_UART_RX      16
#define UART_BAUD        115200

// ---------------------------------------------------------------- Servo
// Mismos valores que pruebas/servo_test (50 Hz, 16 bits). Ajustar con la compuerta montada.
#define SERVO_FREQ       50
#define SERVO_RESOL      16
#define SERVO_CANAL      0      // solo para el core ESP32 2.x
#define SERVO_DUTY_A     1638   // 0°   izquierda
#define SERVO_DUTY_R     4915   // 90°  centro / rechazo
#define SERVO_DUTY_B     7864   // 180° derecha

// ---------------------------------------------------------------- Motor
#define MOTOR_FREQ       20000  // Hz (el TB6612 admite hasta 100 kHz)
#define MOTOR_RESOL      10     // bits
#define MOTOR_CANAL      2      // solo para el core ESP32 2.x (otro temporizador que el servo)
#define PWM_BANDA_PCT    60     // [PROVISIONAL] ciclo de trabajo de la banda en %

// ---------------------------------------------------------------- HC-SR04
#define MEDICION_MS        50     // ventana por medicion; los dos sensores se miden alternados
#define UMBRAL_INSP_CM     8.0f   // [PROVISIONAL] distancia <= umbral -> hay pieza en inspeccion
#define UMBRAL_SALIDA_CM   8.0f   // [PROVISIONAL] distancia <= umbral -> hay pieza en la salida
#define DISTANCIA_MIN_CM   1.0f   // ecos mas cortos se consideran invalidos
#define MUESTRAS_ACTIVAR   2      // mediciones "cercano" seguidas para pasar a presente
#define MUESTRAS_LIBERAR   3      // mediciones "libre/sin eco" seguidas para pasar a libre
#define MEDICIONES_DESCARTE 4     // al arrancar (GPIO14 emite un pulso al encender)

// ---------------------------------------------------------------- Tiempos del ciclo (ms)
#define RETARDO_PARO_MS     0     // [PROVISIONAL] la banda sigue este tiempo tras detectar la
                                  // pieza, para centrar la etiqueta sobre el RC522
#define LECTURA_NFC_MS      1500  // documento de avance §5
#define SERVO_MS            400   // tiempo para que la compuerta llegue (integracion_test)
#define ATASCO_MS           5000  // [PROVISIONAL] "N segundos" del avance §11
#define SALIDA_TIMEOUT_MS   6000  // [PROVISIONAL] maximo desde que se libera la inspeccion
                                  // hasta que la pieza llega al sensor de salida
#define RETORNO_CENTRO_MS   1000  // [PROVISIONAL] desde que la pieza pasa la salida hasta
                                  // regresar la compuerta al centro
#define CAPACIDAD_INICIAL   15    // piezas por contenedor; se cambia desde el panel

// ---------------------------------------------------------------- Enlace con la Raspberry Pi
#define LATIDO_MS          1000
#define ENLACE_TIMEOUT_MS  3000
#define REINTENTO_MS       1000
#define MAX_LINEA          700

// ---------------------------------------------------------------- Interfaz local
#define PANTALLA_MS        250    // refresco maximo de la OLED
#define ANTIRREBOTE_MS     50
