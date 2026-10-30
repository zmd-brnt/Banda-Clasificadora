// Prueba de integracion: NFC RC522 + Servo SG90 + UART con Raspberry Pi
//
// Objetivo:
//   Reproducir el ciclo de clasificacion del proyecto usando SOLO el lector NFC, el servo y el
//   enlace UART. No hay IR, motor ni banda: la pieza (etiqueta) se acerca y se retira a mano.
//   Los pasos que dependen de hardware ausente se sustituyen asi:
//     - IR de inspeccion  -> la presencia de la etiqueta en el lector (o el comando 'p' para una
//                            pieza SIN etiqueta).
//     - Motor de la banda -> mensajes "banda detenida / banda en marcha" (no se mueve nada).
//     - IR de salida      -> tiempo fijo de transito (TRANSITO_MS) despues de retirar la pieza.
//     - Boton de paro     -> comando 'e'.   Boton restablecer -> comando 'r'.
//
// Ciclo (secciones 5 y 8 del documento del proyecto):
//   1. ESPERA          Se espera una pieza nueva en el lector.
//   2. LECTURA_NFC     "Banda detenida". El RC522 intenta leer el UID durante 1.5 s como maximo.
//   3.                 Se consulta la tabla local (sincronizada desde la Raspberry Pi).
//   4. CLASIFICANDO    El servo mueve la compuerta: A (0°), B (180°) o rechazo (90°).
//   5. LIBERA_LECTOR   "Banda en marcha". La pieza debe retirarse del lector; mientras siga ahi
//                      no se vuelve a leer (doble lectura). Si no se retira en 10 s -> ATASCO.
//   6. CONFIRMA_SALIDA Transito simulado hasta el IR de salida (TRANSITO_MS).
//   7.                 Se actualiza el conteo local (guardado en NVS) y se envia PIEZA por UART.
//   8. RETORNO         La compuerta vuelve al centro y se regresa a ESPERA.
//   La Raspberry Pi confirma cada PIEZA con ACK; sin ACK el ESP32 reintenta. Si el enlace cae,
//   el ESP32 sigue clasificando con su tabla local y reenvia los pendientes al reconectar.
//
// Conexiones (ver docs/conexiones.md):
//   RC522:  SDA->GPIO5  SCK->GPIO18  MOSI->GPIO23  MISO->GPIO19  RST->GPIO4  3.3V->3V3  GND->GND
//   Servo:  senal->GPIO13  rojo->+5V rama de potencia (NO del ESP32)  cafe->GND comun
//   UART:   GPIO17 (TX2)->RPi pin 10 (RXD)   GPIO16 (RX2)<-RPi pin 8 (TXD)   GND<->RPi pin 6
//
// Bibliotecas: MFRC522 (GithubCommunity / miguelbalboa). Preferences viene con el core ESP32.
//
// Protocolo UART (texto, una trama por linea, 115200 8N1):
//   <cuerpo>*HH\n    HH = XOR de todos los caracteres del cuerpo, en hexadecimal
//   ESP32 -> RPi:
//     PIEZA,<seq>,<uid|->,<A|B|R>,<OK|SIN_LECTURA|NO_REGISTRADA>,<ciclo_ms>,<reenvio 0|1>
//     ESTADO,<nombre>                              (en cada cambio de estado)
//     LATIDO,<pendientes>,<version_tabla>          (cada 1 s)
//     ALERTA,<codigo>     REINICIO | ATASCO | PIEZAS_JUNTAS | EMERGENCIA | RESTABLECIDO |
//                         NFC_FALLA | PENDIENTES_PERDIDOS
//   RPi -> ESP32:
//     ACK,<seq>
//     VIVO,<version_tabla>                         (respuesta a cada LATIDO)
//     SYNC,<version>,<uid>=<cat>;<uid>=<cat>;...   (cuando la version no coincide)
//     REENVIAR,<seq>                               (la RPi detecto un hueco en la secuencia)
//
// Comandos por Monitor Serial (USB):
//   p -> pieza sin etiqueta (simula el IR de inspeccion)   e -> paro de emergencia
//   r -> restablecer tras ATASCO o EMERGENCIA              s -> estado    t -> tabla local
//   l -> mostrar/ocultar tramas LATIDO y ESTADO
//
// Criterios de exito: ver pruebas/integracion_test/README.md

#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <esp_arduino_version.h>

// ---- Pines ----
#define SS_PIN     5
#define RST_PIN    4
#define SERVO_PIN  13
#define UART2_TX   17
#define UART2_RX   16

#define BAUD_RATE  115200

// ---- Servo (mismos valores que servo_test) ----
#define PWM_FREQ     50
#define PWM_RESOL    16
#define PWM_CHANNEL  0
#define DUTY_0    1638    // Categoria A (izquierda)
#define DUTY_90   4915    // Centro / rechazo
#define DUTY_180  7864    // Categoria B (derecha)

// ---- Tiempos (ms) ----
#define SONDEO_NFC_MS        50   // cada cuanto se busca una pieza nueva en ESPERA
#define LECTURA_NFC_MS     1500   // limite de lectura del documento
#define SERVO_MS            400   // tiempo para que la compuerta llegue a su posicion
#define PRESENCIA_MS        100   // cada cuanto se comprueba si la etiqueta sigue en el lector
#define AUSENCIAS_RETIRO      3   // comprobaciones fallidas seguidas para dar la pieza por retirada
#define ATASCO_MS         10000   // pieza en el lector mas de este tiempo tras clasificarla -> atasco
#define TRANSITO_MS        2000   // transito simulado desde el lector hasta el IR de salida
#define RETORNO_CENTRO_MS  1000   // espera antes de regresar la compuerta al centro
#define LATIDO_MS          1000
#define ENLACE_TIMEOUT_MS  3000   // sin tramas validas de la RPi -> enlace caido
#define REINTENTO_MS       1000   // reenvio de eventos sin ACK

#define MAX_ETIQUETAS  32
#define MAX_PENDIENTES 16
#define MAX_UID        20         // 7 bytes = 14 caracteres hex; margen para otros tipos
#define MAX_LINEA      700

MFRC522 lector(SS_PIN, RST_PIN);
Preferences prefs;

// ---------- Tabla local ----------
struct Etiqueta {
  char uid[MAX_UID + 1];
  char categoria;
};

Etiqueta tabla[MAX_ETIQUETAS];
int numEtiquetas = 0;
uint32_t versionTabla = 0;

// ---------- Eventos pendientes de ACK ----------
struct Evento {
  bool usado;
  uint32_t seq;
  char uid[MAX_UID + 1];
  char categoria;
  const char* motivo;
  uint32_t ciclo_ms;
  unsigned long ultimoEnvio;
  uint8_t envios;
};

Evento pendientes[MAX_PENDIENTES];
uint32_t secuencia = 0;

// ---------- Estado del ciclo ----------
// Los nombres coinciden con los que usa el simulador del panel web (web/js/simulador.js)
enum Estado { ESPERA, LECTURA_NFC, CLASIFICANDO, LIBERA_LECTOR, CONFIRMA_SALIDA, RETORNO, ERROR, EMERGENCIA };
const char* NOMBRES_ESTADO[] = {"ESPERA", "LECTURA_NFC", "CLASIFICANDO", "LIBERA_LECTOR",
                                "CONFIRMA_SALIDA", "RETORNO", "ERROR", "EMERGENCIA"};

Estado estado = ESPERA;
unsigned long tEstado = 0;
unsigned long tDeteccion = 0;
unsigned long tSondeo = 0;

bool etiquetaDetectada = false;   // la pieza llego con etiqueta (no por el comando 'p')
bool uidPorSeleccionar = false;   // la etiqueta acaba de responder a REQA: se puede seleccionar directo
int ausencias = 0;
bool avisoJuntas = false;

char uidActual[MAX_UID + 1];
char categoriaActual;
const char* motivoActual;

// A, B, R y, dentro de R, los que no se pudieron leer
struct Conteo {
  uint32_t a, b, r, sinLectura;
};
Conteo conteo = {0, 0, 0, 0};
bool nfcOk = false;

// ---------- UART ----------
char rxLinea[MAX_LINEA];
size_t rxLen = 0;
bool rxDesbordado = false;
unsigned long ultimoRx = 0;
bool enlaceActivo = false;
unsigned long ultimoLatido = 0;
uint32_t erroresTrama = 0;
bool verLatidos = false;

// =====================================================================
// Servo
// =====================================================================

void moverServo(uint32_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

void posicionarCompuerta(char categoria) {
  if (categoria == 'A') {
    moverServo(DUTY_0);
  } else if (categoria == 'B') {
    moverServo(DUTY_180);
  } else {
    moverServo(DUTY_90);
  }
}

// =====================================================================
// Tramas UART
// =====================================================================

uint8_t calcularChecksum(const char* s) {
  uint8_t x = 0;
  while (*s) {
    x ^= (uint8_t)*s++;
  }
  return x;
}

void enviarTrama(const char* cuerpo) {
  char trama[MAX_LINEA + 4];
  snprintf(trama, sizeof(trama), "%s*%02X\n", cuerpo, calcularChecksum(cuerpo));
  Serial2.print(trama);

  bool rutinaria = strncmp(cuerpo, "LATIDO,", 7) == 0 || strncmp(cuerpo, "ESTADO,", 7) == 0;
  if (verLatidos || !rutinaria) {
    Serial.print("  [UART >] ");
    Serial.print(trama);
  }
}

void enviarAlerta(const char* codigo) {
  char cuerpo[48];
  snprintf(cuerpo, sizeof(cuerpo), "ALERTA,%s", codigo);
  enviarTrama(cuerpo);
}

void enviarEvento(Evento& e) {
  char cuerpo[96];
  snprintf(cuerpo, sizeof(cuerpo), "PIEZA,%lu,%s,%c,%s,%lu,%d",
           (unsigned long)e.seq, e.uid, e.categoria, e.motivo,
           (unsigned long)e.ciclo_ms, e.envios > 0 ? 1 : 0);
  enviarTrama(cuerpo);
  e.ultimoEnvio = millis();
  e.envios++;
}

int contarPendientes() {
  int n = 0;
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (pendientes[i].usado) n++;
  }
  return n;
}

Evento* buscarPendiente(uint32_t seq) {
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (pendientes[i].usado && pendientes[i].seq == seq) {
      return &pendientes[i];
    }
  }
  return nullptr;
}

void quitarPendiente(uint32_t seq) {
  Evento* e = buscarPendiente(seq);
  if (!e) return;   // ACK de un evento que ya no esta pendiente (ACK duplicado): se ignora

  e->usado = false;
  Serial.print("  [ACK] seq ");
  Serial.print(seq);
  Serial.print(" confirmado. Pendientes: ");
  Serial.println(contarPendientes());
}

// Busca un lugar libre; si el buffer esta lleno descarta el evento mas antiguo.
Evento& reservarPendiente() {
  int libre = -1;
  int masAntiguo = 0;
  for (int i = 0; i < MAX_PENDIENTES; i++) {
    if (!pendientes[i].usado) {
      libre = i;
      break;
    }
    if (pendientes[i].seq < pendientes[masAntiguo].seq) {
      masAntiguo = i;
    }
  }

  if (libre < 0) {
    Serial.print("  ADVERTENCIA: buffer de pendientes lleno, se pierde seq ");
    Serial.println(pendientes[masAntiguo].seq);
    enviarAlerta("PENDIENTES_PERDIDOS");
    libre = masAntiguo;
  }
  return pendientes[libre];
}

void reintentarPendientes() {
  if (!enlaceActivo) return;   // sin enlace se conservan; se reenvian al reconectar

  for (int i = 0; i < MAX_PENDIENTES; i++) {
    Evento& e = pendientes[i];
    if (e.usado && millis() - e.ultimoEnvio >= REINTENTO_MS) {
      Serial.print("  [REINTENTO] seq ");
      Serial.print(e.seq);
      Serial.print(" (envio #");
      Serial.print(e.envios + 1);
      Serial.println(")");
      enviarEvento(e);
    }
  }
}

// =====================================================================
// Tabla local
// =====================================================================

// datos = "<version>,<uid>=<cat>;<uid>=<cat>;..."
// Se valida todo antes de reemplazar la tabla actual.
bool cargarTabla(const char* datos) {
  char copia[MAX_LINEA];
  strncpy(copia, datos, sizeof(copia) - 1);
  copia[sizeof(copia) - 1] = '\0';

  char* coma = strchr(copia, ',');
  if (!coma) return false;
  *coma = '\0';
  uint32_t version = strtoul(copia, nullptr, 10);

  Etiqueta nueva[MAX_ETIQUETAS];
  int n = 0;
  char* guardado;
  char* elemento = strtok_r(coma + 1, ";", &guardado);

  while (elemento) {
    char* igual = strchr(elemento, '=');
    if (!igual || n >= MAX_ETIQUETAS) return false;
    *igual = '\0';

    char cat = igual[1];
    size_t largo = strlen(elemento);
    if (largo == 0 || largo > MAX_UID) return false;
    if (cat != 'A' && cat != 'B' && cat != 'R') return false;

    strcpy(nueva[n].uid, elemento);
    nueva[n].categoria = cat;
    n++;
    elemento = strtok_r(nullptr, ";", &guardado);
  }

  memcpy(tabla, nueva, sizeof(Etiqueta) * n);
  numEtiquetas = n;
  versionTabla = version;
  return true;
}

char buscarCategoria(const char* uid) {
  for (int i = 0; i < numEtiquetas; i++) {
    if (strcmp(tabla[i].uid, uid) == 0) {
      return tabla[i].categoria;
    }
  }
  return '\0';
}

void imprimirTabla() {
  Serial.print("Tabla local v");
  Serial.print(versionTabla);
  Serial.print(" (");
  Serial.print(numEtiquetas);
  Serial.println(" etiquetas):");
  for (int i = 0; i < numEtiquetas; i++) {
    Serial.print("  ");
    Serial.print(tabla[i].uid);
    Serial.print(" -> ");
    Serial.println(tabla[i].categoria);
  }
}

// =====================================================================
// Recepcion UART
// =====================================================================

void procesarTrama(char* linea) {
  char* asterisco = strrchr(linea, '*');
  if (!asterisco || strlen(asterisco) != 3 ||
      !isxdigit(asterisco[1]) || !isxdigit(asterisco[2])) {
    erroresTrama++;
    Serial.print("  [UART <] Trama sin checksum, descartada: ");
    Serial.println(linea);
    return;
  }

  *asterisco = '\0';
  uint8_t esperado = strtoul(asterisco + 1, nullptr, 16);
  if (calcularChecksum(linea) != esperado) {
    erroresTrama++;
    Serial.print("  [UART <] Checksum incorrecto, descartada: ");
    Serial.println(linea);
    return;
  }

  ultimoRx = millis();
  if (!enlaceActivo) {
    enlaceActivo = true;
    Serial.println(">> Enlace con la Raspberry Pi ACTIVO");
  }

  if (strncmp(linea, "ACK,", 4) == 0) {
    quitarPendiente(strtoul(linea + 4, nullptr, 10));
  } else if (strncmp(linea, "VIVO,", 5) == 0) {
    if (verLatidos) {
      Serial.print("  [UART <] ");
      Serial.println(linea);
    }
  } else if (strncmp(linea, "SYNC,", 5) == 0) {
    if (cargarTabla(linea + 5)) {
      prefs.putString("tabla", linea + 5);
      Serial.print(">> SYNC aplicado. ");
      imprimirTabla();
    } else {
      erroresTrama++;
      Serial.println("  [UART <] SYNC con formato invalido, se conserva la tabla anterior");
    }
  } else if (strncmp(linea, "REENVIAR,", 9) == 0) {
    uint32_t seq = strtoul(linea + 9, nullptr, 10);
    Evento* e = buscarPendiente(seq);
    Serial.print("  [UART <] La RPi pide reenviar seq ");
    Serial.println(seq);
    if (e) {
      enviarEvento(*e);
    } else {
      Serial.println("  Ese seq ya no esta pendiente (confirmado o descartado): no se puede reenviar");
    }
  } else {
    Serial.print("  [UART <] Trama desconocida: ");
    Serial.println(linea);
  }
}

void leerUart() {
  while (Serial2.available()) {
    char c = Serial2.read();

    if (c == '\r') continue;

    if (c == '\n') {
      rxLinea[rxLen] = '\0';
      if (rxDesbordado) {
        erroresTrama++;
        Serial.println("  [UART <] Linea demasiado larga, descartada");
      } else if (rxLen > 0) {
        procesarTrama(rxLinea);
      }
      rxLen = 0;
      rxDesbordado = false;
      continue;
    }

    if (rxLen < MAX_LINEA - 1) {
      rxLinea[rxLen++] = c;
    } else {
      rxDesbordado = true;
    }
  }
}

void revisarEnlace() {
  if (enlaceActivo && millis() - ultimoRx > ENLACE_TIMEOUT_MS) {
    enlaceActivo = false;
    Serial.println(">> Enlace con la Raspberry Pi CAIDO: se sigue clasificando con la tabla local");
  }

  if (millis() - ultimoLatido >= LATIDO_MS) {
    ultimoLatido = millis();
    char cuerpo[48];
    snprintf(cuerpo, sizeof(cuerpo), "LATIDO,%d,%lu", contarPendientes(), (unsigned long)versionTabla);
    enviarTrama(cuerpo);
  }
}

// =====================================================================
// NFC
// =====================================================================

// Pieza nueva: REQA solo despierta etiquetas que acaban de entrar al campo. Una etiqueta que
// ya se leyo y se dejo sobre el lector queda en HALT y no vuelve a disparar el ciclo.
bool piezaNueva() {
  return nfcOk && lector.PICC_IsNewCardPresent();
}

// Etiqueta en el campo, aunque ya se haya leido (WUPA tambien despierta las que estan en HALT).
bool etiquetaPresente() {
  byte atqa[2];
  byte largo = sizeof(atqa);
  MFRC522::StatusCode s = lector.PICC_WakeupA(atqa, &largo);
  if (s == MFRC522::STATUS_OK || s == MFRC522::STATUS_COLLISION) {
    lector.PICC_HaltA();
    return true;
  }
  return false;
}

// yaDespierta: la etiqueta acaba de responder a PICC_IsNewCardPresent y se puede seleccionar.
bool leerUid(char* destino, bool yaDespierta) {
  if (!yaDespierta) {
    byte atqa[2];
    byte largo = sizeof(atqa);
    MFRC522::StatusCode s = lector.PICC_WakeupA(atqa, &largo);
    if (s != MFRC522::STATUS_OK && s != MFRC522::STATUS_COLLISION) return false;
  }
  if (!lector.PICC_ReadCardSerial()) return false;

  destino[0] = '\0';
  for (byte i = 0; i < lector.uid.size && (i * 2) < MAX_UID; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02X", lector.uid.uidByte[i]);
    strcat(destino, hex);
  }

  lector.PICC_HaltA();
  lector.PCD_StopCrypto1();
  return true;
}

// =====================================================================
// Ciclo de clasificacion
// =====================================================================

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  tEstado = millis();

  char cuerpo[32];
  snprintf(cuerpo, sizeof(cuerpo), "ESTADO,%s", NOMBRES_ESTADO[nuevo]);
  enviarTrama(cuerpo);
}

void guardarConteo() {
  prefs.putBytes("conteo", &conteo, sizeof(conteo));
}

void registrarEvento() {
  uint32_t ciclo = millis() - tDeteccion;

  secuencia++;
  prefs.putUInt("seq", secuencia);

  if (categoriaActual == 'A') conteo.a++;
  else if (categoriaActual == 'B') conteo.b++;
  else conteo.r++;
  if (strcmp(motivoActual, "SIN_LECTURA") == 0) conteo.sinLectura++;
  guardarConteo();

  Evento& e = reservarPendiente();
  e.usado = true;
  e.seq = secuencia;
  strcpy(e.uid, uidActual);
  e.categoria = categoriaActual;
  e.motivo = motivoActual;
  e.ciclo_ms = ciclo;
  e.envios = 0;

  Serial.print("  [IR salida simulado] Paso confirmado. Ciclo: ");
  Serial.print(ciclo);
  Serial.print(" ms. Evento seq ");
  Serial.print(secuencia);
  Serial.print(". Conteo A/B/R: ");
  Serial.print(conteo.a); Serial.print(" / ");
  Serial.print(conteo.b); Serial.print(" / ");
  Serial.println(conteo.r);

  if (enlaceActivo) {
    enviarEvento(e);
  } else {
    e.ultimoEnvio = millis();
    Serial.println("  Sin enlace: el evento queda pendiente");
  }
}

void entrarError(const char* codigo, const char* explicacion) {
  Serial.print("!! ");
  Serial.print(codigo);
  Serial.print(": ");
  Serial.println(explicacion);
  Serial.println("!! Banda detenida y compuerta al centro. Retira la pieza y envia 'r' para restablecer.");
  moverServo(DUTY_90);
  enviarAlerta(codigo);
  cambiarEstado(ERROR);
}

// Simula el IR de inspeccion reactivandose mientras la pieza anterior sigue en el ciclo.
void vigilarPiezasJuntas() {
  if (avisoJuntas || millis() - tSondeo < SONDEO_NFC_MS) return;
  tSondeo = millis();
  if (piezaNueva()) {
    avisoJuntas = true;
    Serial.println("  ADVERTENCIA: llego otra pieza antes de terminar la anterior (piezas muy juntas).");
    Serial.println("  Se atendera cuando la compuerta vuelva al centro.");
    enviarAlerta("PIEZAS_JUNTAS");
  }
}

// Equivale a que el IR de inspeccion detecte la pieza: se detiene la banda y se lee el NFC.
void iniciarPieza(bool conEtiqueta) {
  etiquetaDetectada = conEtiqueta;
  uidPorSeleccionar = conEtiqueta;
  tDeteccion = millis();
  Serial.println();
  Serial.println(conEtiqueta ? "[Lector] Pieza detectada." : "[IR simulado] Pieza sin etiqueta detectada.");
  Serial.println("  [Banda] Detenida. Leyendo NFC (maximo 1.5 s)...");
  cambiarEstado(LECTURA_NFC);
}

void cicloClasificacion() {
  switch (estado) {
    case ESPERA:
      if (millis() - tSondeo >= SONDEO_NFC_MS) {
        tSondeo = millis();
        if (piezaNueva()) {
          iniciarPieza(true);
        }
      }
      break;

    case LECTURA_NFC: {
      bool leido = nfcOk && leerUid(uidActual, uidPorSeleccionar);
      uidPorSeleccionar = false;

      if (leido) {
        char cat = buscarCategoria(uidActual);
        if (cat) {
          categoriaActual = cat;
          motivoActual = "OK";
        } else {
          categoriaActual = 'R';
          motivoActual = "NO_REGISTRADA";
        }
        etiquetaDetectada = true;
      } else if (millis() - tEstado >= LECTURA_NFC_MS) {
        strcpy(uidActual, "-");
        categoriaActual = 'R';
        motivoActual = "SIN_LECTURA";
      } else {
        break;   // seguir intentando
      }

      Serial.print("  UID: ");
      Serial.print(uidActual);
      Serial.print("  (leido en ");
      Serial.print(millis() - tEstado);
      Serial.println(" ms)");
      Serial.print("  Categoria: ");
      Serial.print(categoriaActual);
      Serial.print("  Motivo: ");
      Serial.println(motivoActual);

      posicionarCompuerta(categoriaActual);
      Serial.print("  Compuerta -> ");
      Serial.println(categoriaActual == 'A' ? "izquierda (A)" : categoriaActual == 'B' ? "derecha (B)" : "centro (rechazo)");
      cambiarEstado(CLASIFICANDO);
      break;
    }

    case CLASIFICANDO:
      if (millis() - tEstado >= SERVO_MS) {
        Serial.println("  [Banda] En marcha.");
        if (etiquetaDetectada) {
          Serial.println("  Retira la pieza del lector (la banda la llevaria hacia la compuerta)...");
          ausencias = 0;
          cambiarEstado(LIBERA_LECTOR);
        } else {
          cambiarEstado(CONFIRMA_SALIDA);
        }
      }
      break;

    case LIBERA_LECTOR:
      if (millis() - tEstado >= ATASCO_MS) {
        entrarError("ATASCO", "la pieza no salio del lector.");
        break;
      }
      if (millis() - tSondeo >= PRESENCIA_MS) {
        tSondeo = millis();
        ausencias = etiquetaPresente() ? 0 : ausencias + 1;
        if (ausencias >= AUSENCIAS_RETIRO) {
          Serial.println("  [Lector] Libre. Pieza en transito hacia el IR de salida...");
          cambiarEstado(CONFIRMA_SALIDA);
        }
      }
      break;

    case CONFIRMA_SALIDA:
      vigilarPiezasJuntas();
      if (millis() - tEstado >= TRANSITO_MS) {
        registrarEvento();
        cambiarEstado(RETORNO);
      }
      break;

    case RETORNO:
      vigilarPiezasJuntas();
      if (millis() - tEstado >= RETORNO_CENTRO_MS) {
        moverServo(DUTY_90);
        Serial.println("  Compuerta al centro. Listo para la siguiente pieza.");
        avisoJuntas = false;
        cambiarEstado(ESPERA);
      }
      break;

    case ERROR:
    case EMERGENCIA:
      // Solo se sale con el comando 'r' (ver atenderMonitor)
      break;
  }
}

// =====================================================================
// Monitor Serial
// =====================================================================

void imprimirEstado() {
  Serial.println("--- Estado ---");
  Serial.print("  Ciclo: ");          Serial.println(NOMBRES_ESTADO[estado]);
  Serial.print("  RC522: ");          Serial.println(nfcOk ? "OK" : "FALLA");
  Serial.print("  Enlace RPi: ");     Serial.println(enlaceActivo ? "ACTIVO" : "CAIDO");
  Serial.print("  Ultimo seq: ");     Serial.println(secuencia);
  Serial.print("  Pendientes: ");     Serial.println(contarPendientes());
  Serial.print("  Tabla: v");         Serial.print(versionTabla);
  Serial.print(", ");                 Serial.print(numEtiquetas); Serial.println(" etiquetas");
  Serial.print("  Conteo A/B/R: ");
  Serial.print(conteo.a); Serial.print(" / ");
  Serial.print(conteo.b); Serial.print(" / ");
  Serial.print(conteo.r);
  Serial.print("  (sin leer: ");      Serial.print(conteo.sinLectura); Serial.println(")");
  Serial.print("  Tramas invalidas: "); Serial.println(erroresTrama);
}

void atenderMonitor() {
  if (!Serial.available()) return;

  char c = Serial.read();
  switch (c) {
    case 's': imprimirEstado(); break;
    case 't': imprimirTabla(); break;
    case 'l':
      verLatidos = !verLatidos;
      Serial.println(verLatidos ? "Tramas LATIDO/ESTADO visibles" : "Tramas LATIDO/ESTADO ocultas");
      break;
    case 'p':
      if (estado == ESPERA) {
        iniciarPieza(false);
      } else {
        Serial.println("Hay una pieza en curso: espera a que el ciclo regrese a ESPERA.");
      }
      break;
    case 'e':
      if (estado == EMERGENCIA) break;
      moverServo(DUTY_90);
      Serial.println();
      Serial.println("!! PARO DE EMERGENCIA: banda detenida y compuerta al centro. La pieza en curso no se registra.");
      Serial.println("!! Retira la pieza y envia 'r' para restablecer.");
      enviarAlerta("EMERGENCIA");
      cambiarEstado(EMERGENCIA);
      break;
    case 'r':
      if (estado != ERROR && estado != EMERGENCIA) {
        Serial.println("No hay error que restablecer.");
      } else if (nfcOk && etiquetaPresente()) {
        Serial.println("Sigue habiendo una etiqueta en el lector: retirala primero.");
      } else {
        Serial.println(">> Restablecido. Listo para la siguiente pieza.");
        avisoJuntas = false;
        enviarAlerta("RESTABLECIDO");
        cambiarEstado(ESPERA);
      }
      break;
  }
}

// =====================================================================

void setup() {
  Serial.begin(115200);
  Serial2.setRxBufferSize(1024);
  Serial2.begin(BAUD_RATE, SERIAL_8N1, UART2_RX, UART2_TX);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RESOL);
#else
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOL);
  ledcAttachPin(SERVO_PIN, PWM_CHANNEL);
#endif
  moverServo(DUTY_90);

  delay(200);
  Serial.println();
  Serial.println("=== Prueba de integracion: NFC + Servo + UART ===");

  // Secuencia, conteo y tabla guardados en memoria no volatil (sobreviven a reinicios)
  prefs.begin("integracion", false);
  secuencia = prefs.getUInt("seq", 0);
  if (prefs.getBytesLength("conteo") == sizeof(conteo)) {
    prefs.getBytes("conteo", &conteo, sizeof(conteo));
  }
  String guardada = prefs.getString("tabla", "");
  if (guardada.length() > 0 && cargarTabla(guardada.c_str())) {
    Serial.print("Tabla recuperada de memoria. ");
    imprimirTabla();
  } else {
    Serial.println("Sin tabla guardada: todas las piezas iran a rechazo hasta recibir SYNC.");
  }
  Serial.print("Ultimo seq guardado: ");
  Serial.print(secuencia);
  Serial.print("   Conteo A/B/R guardado: ");
  Serial.print(conteo.a); Serial.print(" / ");
  Serial.print(conteo.b); Serial.print(" / ");
  Serial.println(conteo.r);

  SPI.begin();
  lector.PCD_Init();
  delay(100);
  byte version = lector.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("Firmware del RC522: 0x");
  Serial.println(version, HEX);
  nfcOk = !(version == 0x00 || version == 0xFF);
  if (nfcOk) {
    lector.PCD_SetAntennaGain(MFRC522::RxGain_max);
  } else {
    Serial.println("ERROR: No se detecta el RC522. Usa 'p' para probar el rechazo SIN_LECTURA.");
    enviarAlerta("NFC_FALLA");
  }

  enviarAlerta("REINICIO");
  cambiarEstado(ESPERA);

  Serial.println();
  Serial.println("Comandos: p=pieza sin etiqueta  e=emergencia  r=restablecer  s=estado  t=tabla  l=latidos");
  Serial.println("Acerca una etiqueta al lector (2 cm o menos)...");
}

void loop() {
  leerUart();
  revisarEnlace();
  cicloClasificacion();
  reintentarPendientes();
  atenderMonitor();
}
