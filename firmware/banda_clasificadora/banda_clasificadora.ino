// Banda clasificadora de objetos por NFC — firmware del ESP32 (sistema completo)
//
// El ESP32 es el nodo de tiempo real: detecta la pieza con los HC-SR04, detiene la banda,
// lee el UID con el RC522, consulta su tabla local, mueve la compuerta, confirma la salida,
// cuenta y reporta a la Raspberry Pi por UART. Clasifica aunque la Raspberry Pi no este.
//
// Documentacion:
//   firmware/README.md          como compilar, cargar, calibrar y probar
//   docs/maquina-de-estados.md  estados y transiciones
//   docs/protocolo-uart.md      tramas con la Raspberry Pi
//   docs/conexiones.md          cableado
//
// Archivos:
//   config.h       pines, tiempos y umbrales ([PROVISIONAL] = falta medirlo)
//   protocolo.*    tramas y checksum (logica pura, probada en firmware/test)
//   tabla.*        tabla local UID -> categoria (logica pura, probada)
//   pendientes.*   eventos sin ACK (logica pura, probada)
//   deteccion.*    distancia y filtro de presencia (logica pura, probada)
//   ultrasonico.*  dos HC-SR04 con interrupciones
//   actuadores.*   motor (TB6612) y compuerta (servo)
//   interfaz.*     paro, boton Restablecer, LEDs, buzzer, OLED
//   nfc.*          RC522
//   enlace.*       UART con la Raspberry Pi
//
// Bibliotecas: MFRC522 (miguelbalboa), Adafruit SSD1306 (+ Adafruit GFX y BusIO).

#include <Preferences.h>
#include <esp_system.h>

#include "actuadores.h"
#include "config.h"
#include "enlace.h"
#include "interfaz.h"
#include "nfc.h"
#include "tabla.h"
#include "ultrasonico.h"

// ================================================================ Estados

// Los nombres son los que se envian en la trama ESTADO y los que muestra el panel web.
enum Estado {
  PAUSA,               // banda detenida esperando el boton Restablecer/Arranque (al encender)
  ESPERA,              // banda en marcha, sin pieza en inspeccion
  LECTURA_NFC,         // banda detenida, leyendo la etiqueta (maximo 1.5 s)
  CLASIFICANDO,        // compuerta moviendose a su posicion
  LIBERA_INSPECCION,   // banda en marcha, la pieza debe salir de la zona de inspeccion
  CONFIRMA_SALIDA,     // esperando que la pieza pase por el sensor de salida
  RETORNO,             // la pieza paso; la compuerta vuelve al centro tras RETORNO_CENTRO_MS
  PAUSA_CONTENEDOR,    // el contenedor destino esta lleno: banda detenida hasta vaciarlo
  ERROR,               // atasco o pieza que no llego a la salida
  EMERGENCIA,          // paro de emergencia
};
const char* const NOMBRE_ESTADO[] = {"PAUSA", "ESPERA", "LECTURA_NFC", "CLASIFICANDO",
                                     "LIBERA_INSPECCION", "CONFIRMA_SALIDA", "RETORNO",
                                     "PAUSA_CONTENEDOR", "ERROR", "EMERGENCIA"};

// ================================================================ Objetos

Ultrasonicos sensores;
Motor motor;
Compuerta compuerta;
Paro paro;
BotonRestablecer boton;
Avisos avisos;
Pantalla pantalla;
LectorNfc nfc;
Enlace enlace;
TablaLocal tabla;
Preferences prefs;

// ================================================================ Datos persistentes (NVS)

struct Conteo {
  uint32_t a, b, r, sinLectura;
};
Conteo conteo = {0, 0, 0, 0};
uint32_t secuencia = 0;
uint16_t contenido[3] = {0, 0, 0};   // piezas en cada contenedor desde el ultimo vaciado
uint16_t capacidad[3] = {CAPACIDAD_INICIAL, CAPACIDAD_INICIAL, CAPACIDAD_INICIAL};

// ================================================================ Estado del ciclo

Estado estado = PAUSA;
unsigned long tEstado = 0;
unsigned long tDeteccion = 0;     // inicio de la pieza (para ciclo_ms)
unsigned long tPrimeraDeteccion = 0;
unsigned long tSalida = 0;
unsigned long tUltimaLectura = 0;
bool salidaVista = false;
bool avisoJuntas = false;
bool avisoSalidaInesperada = false;

char uidActual[MAX_UID + 1] = "-";
char categoriaActual = 'R';
uint8_t motivoActual = MOTIVO_OK;
char lineaPantalla[22] = "";
bool verDistancias = false;

// ================================================================ Utilidades

int indice(char cat) { return cat == 'A' ? 0 : cat == 'B' ? 1 : 2; }

void guardarContadores() {
  prefs.putUInt("seq", secuencia);
  prefs.putBytes("conteo", &conteo, sizeof(conteo));
  prefs.putBytes("contenido", contenido, sizeof(contenido));
}

void guardarPendientes() { prefs.putBytes("pend", enlace.cola().datos(), ColaPendientes::tamDatos()); }

void ponerLinea(const char* texto) {
  strncpy(lineaPantalla, texto, sizeof(lineaPantalla) - 1);
  lineaPantalla[sizeof(lineaPantalla) - 1] = '\0';
}

void refrescarPantalla(bool forzar = false) {
  DatosPantalla d;
  d.estado = NOMBRE_ESTADO[estado];
  d.a = conteo.a;
  d.b = conteo.b;
  d.r = conteo.r;
  d.sinLectura = conteo.sinLectura;
  for (int i = 0; i < 3; i++) {
    d.contenido[i] = contenido[i];
    d.capacidad[i] = capacidad[i];
  }
  d.enlace = enlace.activo();
  d.pendientes = enlace.cola().cantidad();
  d.linea = lineaPantalla;
  pantalla.mostrar(d, forzar);
}

// Verde = operacion normal, amarillo = espera/pausa, rojo = error o emergencia.
void ledsSegunEstado() {
  bool rojo = estado == ERROR || estado == EMERGENCIA;
  bool amarillo = estado == PAUSA || estado == PAUSA_CONTENEDOR;
  avisos.leds(!rojo && !amarillo, amarillo, rojo);
}

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  tEstado = millis();
  enlace.estado(NOMBRE_ESTADO[nuevo]);
  ledsSegunEstado();
  refrescarPantalla(true);
}

void enviarContenedores() { enlace.contenedores(contenido, capacidad); }

// ================================================================ Errores y emergencia

void entrarEmergencia(const char* origen) {
  motor.detener();          // el boton fisico ya corto el driver; esto cubre el paro por software
  compuerta.posicionar('R');
  avisos.buzzer(BUZZER_INTERMITENTE);
  Serial.printf("\n!! PARO DE EMERGENCIA (%s): banda detenida y compuerta al centro.\n", origen);
  Serial.println("!! La pieza en curso NO se registra. Suelta el paro y presiona Restablecer.");
  enlace.alerta("EMERGENCIA", origen);
  ponerLinea("PARO DE EMERGENCIA");
  cambiarEstado(EMERGENCIA);
}

void entrarError(const char* codigo, const char* detalle, const char* explicacion) {
  motor.detener();
  compuerta.posicionar('R');
  avisos.buzzer(BUZZER_INTERMITENTE);
  Serial.printf("\n!! %s: %s\n", codigo, explicacion);
  Serial.println("!! Banda detenida. Retira la pieza y presiona Restablecer.");
  enlace.alerta(codigo, detalle);
  ponerLinea(codigo);
  cambiarEstado(ERROR);
}

// Boton Restablecer (o comando 'r' del Monitor Serial).
void restablecer() {
  switch (estado) {
    case PAUSA:
      if (paro.presionado()) {
        Serial.println("No se puede arrancar: el paro de emergencia esta presionado.");
        avisos.buzzer(BUZZER_CORTO);
        return;
      }
      Serial.println(">> Arranque: banda en marcha.");
      ponerLinea("En marcha");
      enlace.alerta("ARRANQUE");
      tPrimeraDeteccion = 0;
      motor.iniciar(PWM_BANDA_PCT);
      cambiarEstado(ESPERA);
      return;

    case PAUSA_CONTENEDOR: {
      // Confirmacion local del vaciado (sirve aunque la Raspberry Pi este apagada)
      int i = indice(categoriaActual);
      contenido[i] = 0;
      guardarContadores();
      enviarContenedores();
      char det[2] = {categoriaActual, '\0'};
      enlace.alerta("VACIADO", det);
      Serial.printf(">> Contenedor %c marcado como vaciado (boton local).\n", categoriaActual);
      return;   // el ciclo continua solo en la siguiente vuelta
    }

    case ERROR:
    case EMERGENCIA:
      if (paro.presionado()) {
        Serial.println("Suelta primero el boton de paro.");
        avisos.buzzer(BUZZER_CORTO);
        return;
      }
      if (sensores.presente(SENSOR_INSPECCION) || sensores.presente(SENSOR_SALIDA)) {
        Serial.println("Todavia hay una pieza frente a un sensor: retirala primero.");
        avisos.buzzer(BUZZER_CORTO);
        return;
      }
      avisos.buzzer(BUZZER_APAGADO);
      compuerta.posicionar('R');
      enlace.alerta("RESTABLECIDO");
      Serial.println(">> Restablecido: banda en marcha.");
      ponerLinea("Restablecido");
      tPrimeraDeteccion = 0;
      avisoJuntas = false;
      motor.iniciar(PWM_BANDA_PCT);
      cambiarEstado(ESPERA);
      return;

    default:
      Serial.println("No hay nada que restablecer en este estado.");
  }
}

// ================================================================ Comandos de la Raspberry Pi

void alRecibirComando(const proto::Comando& c) {
  switch (c.tipo) {
    case proto::CMD_SYNC: {
      if (tabla.cargar(c.datos)) {
        prefs.putString("tabla", c.datos);
        Serial.printf(">> SYNC aplicado: v%lu, %d etiquetas\n", (unsigned long)tabla.version(), tabla.cantidad());
        char det[12];
        snprintf(det, sizeof(det), "%lu", (unsigned long)tabla.version());
        enlace.alerta("SYNC_OK", det);
      } else {
        Serial.println("  SYNC con formato invalido: se conserva la tabla anterior");
        enlace.alerta("SYNC_INVALIDO");
      }
      break;
    }
    case proto::CMD_VACIADO: {
      contenido[indice(c.categoria)] = 0;
      guardarContadores();
      enviarContenedores();
      Serial.printf(">> Contenedor %c vaciado (desde el panel)\n", c.categoria);
      break;
    }
    case proto::CMD_CAPACIDAD: {
      int i = indice(c.categoria);
      capacidad[i] = (uint16_t)c.numero;
      prefs.putBytes("capacidad", capacidad, sizeof(capacidad));
      enviarContenedores();
      Serial.printf(">> Capacidad del contenedor %c = %u\n", c.categoria, capacidad[i]);
      break;
    }
    case proto::CMD_PARO:
      if (estado != EMERGENCIA) entrarEmergencia("PANEL");
      break;
    default:
      break;
  }
}

// ================================================================ Ciclo de clasificacion

void iniciarClasificacion() {
  compuerta.posicionar(categoriaActual);
  Serial.printf("  Compuerta -> %s\n", categoriaActual == 'A'   ? "izquierda (A)"
                                       : categoriaActual == 'B' ? "derecha (B)"
                                                                : "centro (rechazo)");
  cambiarEstado(CLASIFICANDO);
}

void registrarPieza() {
  uint32_t ciclo = millis() - tDeteccion;
  secuencia++;
  if (categoriaActual == 'A') conteo.a++;
  else if (categoriaActual == 'B') conteo.b++;
  else conteo.r++;
  if (motivoActual == MOTIVO_SIN_LECTURA) conteo.sinLectura++;
  int i = indice(categoriaActual);
  if (contenido[i] < 0xFFFF) contenido[i]++;
  guardarContadores();

  Serial.printf("  [Salida] Paso confirmado. Ciclo %lu ms. seq %lu. A/B/R: %lu / %lu / %lu\n",
                (unsigned long)ciclo, (unsigned long)secuencia, (unsigned long)conteo.a,
                (unsigned long)conteo.b, (unsigned long)conteo.r);
  enlace.pieza(secuencia, uidActual, categoriaActual, motivoActual, ciclo);
  enviarContenedores();

  char linea[22];
  snprintf(linea, sizeof(linea), "#%lu %c %s", (unsigned long)secuencia, categoriaActual,
           motivoActual == MOTIVO_OK ? "OK" : motivoActual == MOTIVO_SIN_LECTURA ? "SIN LEER" : "NO REG");
  ponerLinea(linea);
}

// Otra pieza llega a inspeccion mientras la anterior no ha terminado.
void vigilarPiezasJuntas() {
  if (avisoJuntas || !sensores.presente(SENSOR_INSPECCION)) return;
  avisoJuntas = true;
  Serial.println("  ADVERTENCIA: llego otra pieza antes de terminar la anterior (piezas muy juntas).");
  enlace.alerta("PIEZAS_JUNTAS");
  avisos.buzzer(BUZZER_CORTO);
}

void cicloClasificacion() {
  unsigned long enEstado = millis() - tEstado;

  switch (estado) {
    case PAUSA:
    case ERROR:
    case EMERGENCIA:
      break;   // se sale con restablecer()

    case ESPERA:
      // Pieza en la salida sin haber pasado por inspeccion (el sensor de inspeccion no la vio)
      if (sensores.presente(SENSOR_SALIDA)) {
        if (!avisoSalidaInesperada) {
          avisoSalidaInesperada = true;
          Serial.println("  ADVERTENCIA: pieza en la salida sin pasar por inspeccion (no se registra).");
          enlace.alerta("SALIDA_INESPERADA");
        }
      } else {
        avisoSalidaInesperada = false;
      }

      if (!sensores.presente(SENSOR_INSPECCION)) {
        tPrimeraDeteccion = 0;
        break;
      }
      if (tPrimeraDeteccion == 0) tPrimeraDeteccion = millis();
#if RETARDO_PARO_MS > 0
      if (millis() - tPrimeraDeteccion < RETARDO_PARO_MS) break;
#endif

      motor.detener();
      tDeteccion = tPrimeraDeteccion;
      tPrimeraDeteccion = 0;
      avisoJuntas = false;
      strcpy(uidActual, "-");
      Serial.printf("\n[Inspeccion] Pieza detectada a %.1f cm. Banda detenida. Leyendo NFC...\n",
                    sensores.distancia(SENSOR_INSPECCION));
      cambiarEstado(LECTURA_NFC);
      break;

    case LECTURA_NFC: {
      bool leido = false;
      if (millis() - tUltimaLectura >= 30) {
        tUltimaLectura = millis();
        leido = nfc.leerUid(uidActual);
      }
      if (leido) {
        char cat = tabla.buscar(uidActual);
        categoriaActual = cat ? cat : 'R';
        motivoActual = cat ? MOTIVO_OK : MOTIVO_NO_REGISTRADA;
      } else if (enEstado >= LECTURA_NFC_MS) {
        strcpy(uidActual, "-");
        categoriaActual = 'R';
        motivoActual = MOTIVO_SIN_LECTURA;
      } else {
        break;   // seguir intentando
      }
      Serial.printf("  UID %s (%lu ms) -> categoria %c, %s\n", uidActual, enEstado, categoriaActual,
                    nombreMotivo(motivoActual));
      if (motivoActual == MOTIVO_NO_REGISTRADA) ponerLinea("NO REGISTRADA");
      else if (motivoActual == MOTIVO_SIN_LECTURA) ponerLinea("SIN LECTURA");

      int i = indice(categoriaActual);
      if (contenido[i] >= capacidad[i]) {
        char det[2] = {categoriaActual, '\0'};
        Serial.printf("  Contenedor %c lleno (%u/%u): banda en pausa hasta vaciarlo.\n", categoriaActual,
                      contenido[i], capacidad[i]);
        enlace.alerta("CONTENEDOR_LLENO", det);
        avisos.buzzer(BUZZER_DOBLE);
        char linea[22];
        snprintf(linea, sizeof(linea), "LLENO %c: vaciar", categoriaActual);
        ponerLinea(linea);
        cambiarEstado(PAUSA_CONTENEDOR);
      } else {
        iniciarClasificacion();
      }
      break;
    }

    case PAUSA_CONTENEDOR: {
      int i = indice(categoriaActual);
      if (contenido[i] < capacidad[i]) {
        Serial.println("  Contenedor con espacio: se continua.");
        iniciarClasificacion();
      }
      break;
    }

    case CLASIFICANDO:
      if (enEstado >= SERVO_MS) {
        motor.iniciar(PWM_BANDA_PCT);
        cambiarEstado(LIBERA_INSPECCION);
      }
      break;

    case LIBERA_INSPECCION:
      if (!sensores.presente(SENSOR_INSPECCION)) {
        salidaVista = false;
        cambiarEstado(CONFIRMA_SALIDA);
      } else if (enEstado >= ATASCO_MS) {
        entrarError("ATASCO", "INSPECCION", "la pieza no salio de la zona de inspeccion.");
      }
      break;

    case CONFIRMA_SALIDA:
      vigilarPiezasJuntas();
      if (sensores.presente(SENSOR_SALIDA)) {
        if (!salidaVista) {
          salidaVista = true;
          tSalida = millis();
        } else if (millis() - tSalida >= ATASCO_MS) {
          entrarError("ATASCO", "SALIDA", "la pieza se quedo frente al sensor de salida.");
        }
      } else if (salidaVista) {
        registrarPieza();
        cambiarEstado(RETORNO);
      } else if (enEstado >= SALIDA_TIMEOUT_MS) {
        // El servo no informa su posicion: si la pieza no llega a la salida, no sabemos donde quedo
        entrarError("SIN_SALIDA", nullptr, "la pieza no llego al sensor de salida a tiempo.");
      }
      break;

    case RETORNO:
      vigilarPiezasJuntas();
      if (enEstado >= RETORNO_CENTRO_MS) {
        compuerta.posicionar('R');
        cambiarEstado(ESPERA);
      }
      break;
  }
}

// ================================================================ Monitor Serial

void imprimirTabla() {
  Serial.printf("Tabla local v%lu (%d etiquetas):\n", (unsigned long)tabla.version(), tabla.cantidad());
  for (int i = 0; i < tabla.cantidad(); i++) {
    Serial.printf("  %s -> %c\n", tabla.en(i).uid, tabla.en(i).categoria);
  }
}

void imprimirEstado() {
  Serial.println("--- Estado ---");
  Serial.printf("  Ciclo: %s   Motor: %s   Compuerta: %c\n", NOMBRE_ESTADO[estado],
                motor.enMarcha() ? "en marcha" : "detenido", compuerta.posicion());
  Serial.printf("  RC522: %s (0x%02X)   OLED: %s   Paro: %s\n", nfc.ok() ? "OK" : "FALLA", nfc.version(),
                pantalla.ok() ? "OK" : "no detectada", paro.presionado() ? "PRESIONADO" : "libre");
  Serial.printf("  Inspeccion: %s (%.1f cm)   Salida: %s (%.1f cm)\n",
                sensores.presente(SENSOR_INSPECCION) ? "PIEZA" : "libre", sensores.distancia(SENSOR_INSPECCION),
                sensores.presente(SENSOR_SALIDA) ? "PIEZA" : "libre", sensores.distancia(SENSOR_SALIDA));
  Serial.printf("  Enlace: %s   ultimo seq: %lu   pendientes: %d   tramas invalidas: %lu\n",
                enlace.activo() ? "ACTIVO" : "CAIDO", (unsigned long)secuencia, enlace.cola().cantidad(),
                (unsigned long)enlace.tramasInvalidas());
  Serial.printf("  Tabla v%lu (%d etiquetas)\n", (unsigned long)tabla.version(), tabla.cantidad());
  Serial.printf("  Conteo A/B/R: %lu / %lu / %lu (sin leer %lu)\n", (unsigned long)conteo.a,
                (unsigned long)conteo.b, (unsigned long)conteo.r, (unsigned long)conteo.sinLectura);
  Serial.printf("  Contenedores A %u/%u  B %u/%u  R %u/%u\n", contenido[0], capacidad[0], contenido[1],
                capacidad[1], contenido[2], capacidad[2]);
}

void atenderMonitor() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 's': imprimirEstado(); break;
    case 't': imprimirTabla(); break;
    case 'r': restablecer(); break;   // equivale al boton Restablecer
    case 'e':
      if (estado != EMERGENCIA) entrarEmergencia("MONITOR");
      break;
    case 'l':
      enlace.verRutinarias = !enlace.verRutinarias;
      Serial.println(enlace.verRutinarias ? "Tramas rutinarias visibles" : "Tramas rutinarias ocultas");
      break;
    case 'd':
      verDistancias = !verDistancias;
      Serial.println(verDistancias ? "Distancias visibles (cada 500 ms)" : "Distancias ocultas");
      break;
    case 'h':
    case '?':
      Serial.println("Comandos: s=estado t=tabla r=restablecer/arranque e=emergencia l=tramas d=distancias");
      break;
  }
}

void mostrarDistancias() {
  static unsigned long t = 0;
  if (!verDistancias || millis() - t < 500) return;
  t = millis();
  Serial.printf("  [HC-SR04] inspeccion %.1f cm %s | salida %.1f cm %s\n", sensores.distancia(SENSOR_INSPECCION),
                sensores.presente(SENSOR_INSPECCION) ? "PIEZA" : "libre", sensores.distancia(SENSOR_SALIDA),
                sensores.presente(SENSOR_SALIDA) ? "PIEZA" : "libre");
}

// ================================================================ Arranque

const char* motivoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "ENCENDIDO";
    case ESP_RST_EXT: return "EXTERNO";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANICO";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT";   // caida de voltaje
    default: return "OTRO";
  }
}

void cargarMemoria() {
  prefs.begin("banda", false);
  secuencia = prefs.getUInt("seq", 0);
  if (prefs.getBytesLength("conteo") == sizeof(conteo)) prefs.getBytes("conteo", &conteo, sizeof(conteo));
  if (prefs.getBytesLength("contenido") == sizeof(contenido)) prefs.getBytes("contenido", contenido, sizeof(contenido));
  if (prefs.getBytesLength("capacidad") == sizeof(capacidad)) prefs.getBytes("capacidad", capacidad, sizeof(capacidad));
  if (prefs.getBytesLength("pend") == ColaPendientes::tamDatos()) {
    prefs.getBytes("pend", enlace.cola().datos(), ColaPendientes::tamDatos());
  }
  String guardada = prefs.getString("tabla", "");
  if (guardada.length() > 0 && tabla.cargar(guardada.c_str())) {
    Serial.print("Tabla recuperada de memoria. ");
    imprimirTabla();
  } else {
    Serial.println("Sin tabla guardada: todas las piezas iran a rechazo hasta recibir SYNC.");
  }
  Serial.printf("Memoria: ultimo seq %lu, A/B/R %lu/%lu/%lu, %d pendientes sin ACK\n", (unsigned long)secuencia,
                (unsigned long)conteo.a, (unsigned long)conteo.b, (unsigned long)conteo.r,
                enlace.cola().cantidad());
}

void setup() {
  Serial.begin(115200);

  // Primero lo que mueve cosas, en estado seguro
  motor.begin();          // PWM = 0
  compuerta.begin();      // centro
  avisos.begin();
  paro.begin();           // habilita STBY: el motor sigue parado porque PWM = 0
  boton.begin();
  delay(200);

  Serial.println();
  Serial.println("=== Banda clasificadora NFC — ESP32 ===");
  const char* motivo = motivoReinicio();
  Serial.printf("Motivo del reinicio: %s\n", motivo);

  cargarMemoria();
  enlace.begin(alRecibirComando, guardarPendientes);
  enlace.alerta("REINICIO", motivo);

  if (pantalla.begin()) {
    Serial.println("OLED detectada.");
  } else {
    Serial.printf("ADVERTENCIA: no responde la OLED en 0x%02X (el sistema sigue sin pantalla).\n", OLED_DIRECCION);
  }

  if (nfc.begin()) {
    Serial.printf("RC522 OK (firmware 0x%02X).\n", nfc.version());
  } else {
    Serial.printf("ERROR: no se detecta el RC522 (0x%02X). Todas las piezas iran a rechazo SIN_LECTURA.\n",
                  nfc.version());
    enlace.alerta("NFC_FALLA");
  }

  sensores.begin();

  ponerLinea("Pulse Restablecer");
  cambiarEstado(PAUSA);
  enviarContenedores();

  if (paro.presionado()) {
    Serial.println("Paro de emergencia presionado al encender.");
    entrarEmergencia("BOTON");
  }
  Serial.println("Banda detenida. Presiona Restablecer (o 'r') para arrancar. 'h' = ayuda.");

  enableLoopWDT();   // si loop() se traba mas de ~5 s, el watchdog reinicia el ESP32
}

void loop() {
  sensores.actualizar();
  enlace.actualizar(tabla.version(), secuencia);
  if (enlace.acabaDeConectar()) {
    enlace.estado(NOMBRE_ESTADO[estado]);
    enviarContenedores();
  }

  // El paro tiene prioridad sobre todo lo demas
  bool avisoParo = paro.consumirAviso();
  if ((avisoParo || paro.presionado()) && estado != EMERGENCIA) {
    entrarEmergencia("BOTON");
  }

  if (boton.pulsado()) restablecer();
  cicloClasificacion();

  avisos.actualizar();
  atenderMonitor();
  mostrarDistancias();
  refrescarPantalla();
}
