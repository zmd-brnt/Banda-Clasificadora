// Pruebas unitarias de la logica pura del firmware (se ejecutan en la PC, sin ESP32).
//
// Cubre: protocolo (checksum, tramas, comandos), tabla local, cola de pendientes y
// filtro de presencia de los HC-SR04.
//
// Ejecutar:  ./firmware/test/correr_pruebas.sh
// Resultado esperado: "TODAS LAS PRUEBAS PASARON" y codigo de salida 0.

#include <stdio.h>
#include <string.h>

#include "../banda_clasificadora/deteccion.h"
#include "../banda_clasificadora/pendientes.h"
#include "../banda_clasificadora/protocolo.h"
#include "../banda_clasificadora/tabla.h"

static int fallas = 0;
static int total = 0;

#define VERIFICAR(cond)                                                \
  do {                                                                 \
    total++;                                                           \
    if (!(cond)) {                                                     \
      fallas++;                                                        \
      printf("  FALLA %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
    }                                                                  \
  } while (0)

// ---------------------------------------------------------------- Protocolo

static void pruebaChecksum() {
  printf("checksum y armado de tramas\n");
  // Valor calculado a mano: XOR de 'A','C','K',',','1' = 0x41^0x43^0x4B^0x2C^0x31
  VERIFICAR(proto::checksum("ACK,1") == (0x41 ^ 0x43 ^ 0x4B ^ 0x2C ^ 0x31));
  VERIFICAR(proto::checksum("") == 0);

  char t[32];
  size_t n = proto::armarTrama("ACK,1", t, sizeof(t));
  char esperado[32];
  snprintf(esperado, sizeof(esperado), "ACK,1*%02X\n", proto::checksum("ACK,1"));
  VERIFICAR(n == strlen(esperado));
  VERIFICAR(strcmp(t, esperado) == 0);

  char chico[6];
  VERIFICAR(proto::armarTrama("ACK,12345", chico, sizeof(chico)) == 0);   // no cabe
}

static void pruebaValidar() {
  printf("validacion de tramas\n");
  char buena[32];
  snprintf(buena, sizeof(buena), "VIVO,7*%02X", proto::checksum("VIVO,7"));
  VERIFICAR(proto::validarTrama(buena));
  VERIFICAR(strcmp(buena, "VIVO,7") == 0);   // queda solo el cuerpo

  char minus[32];
  snprintf(minus, sizeof(minus), "VIVO,7*%02x", proto::checksum("VIVO,7"));
  VERIFICAR(proto::validarTrama(minus));     // hex en minusculas tambien vale

  char mala[] = "VIVO,7*00";
  VERIFICAR(!proto::validarTrama(mala));
  VERIFICAR(strcmp(mala, "VIVO,7*00") == 0);   // no se modifica si es invalida

  char sinAst[] = "VIVO,7";
  VERIFICAR(!proto::validarTrama(sinAst));
  char corta[] = "*00";
  VERIFICAR(!proto::validarTrama(corta));
  char noHex[] = "VIVO,7*ZZ";
  VERIFICAR(!proto::validarTrama(noHex));
}

static void pruebaComandos() {
  printf("interpretacion de comandos\n");
  proto::Comando c;

  VERIFICAR(proto::interpretarComando("ACK,42", c) && c.tipo == proto::CMD_ACK && c.numero == 42);
  VERIFICAR(proto::interpretarComando("VIVO,3", c) && c.tipo == proto::CMD_VIVO && c.numero == 3);
  VERIFICAR(proto::interpretarComando("REENVIAR,9", c) && c.tipo == proto::CMD_REENVIAR && c.numero == 9);
  VERIFICAR(proto::interpretarComando("SYNC,5,04AA=A;04BB=B", c) && c.tipo == proto::CMD_SYNC &&
            c.numero == 5 && strcmp(c.datos, "5,04AA=A;04BB=B") == 0);
  VERIFICAR(proto::interpretarComando("SYNC,6,", c) && c.tipo == proto::CMD_SYNC);   // tabla vacia
  VERIFICAR(proto::interpretarComando("VACIADO,B", c) && c.tipo == proto::CMD_VACIADO && c.categoria == 'B');
  VERIFICAR(proto::interpretarComando("CAPACIDAD,R,20", c) && c.tipo == proto::CMD_CAPACIDAD &&
            c.categoria == 'R' && c.numero == 20);
  VERIFICAR(proto::interpretarComando("PARO", c) && c.tipo == proto::CMD_PARO);

  // Mal formados
  VERIFICAR(!proto::interpretarComando("ACK,", c));
  VERIFICAR(!proto::interpretarComando("ACK,12a", c));
  VERIFICAR(!proto::interpretarComando("ACK,-1", c));
  VERIFICAR(!proto::interpretarComando("ACK,99999999999", c));   // no cabe en 32 bits
  VERIFICAR(!proto::interpretarComando("SYNC,x,04AA=A", c));
  VERIFICAR(!proto::interpretarComando("SYNC,5", c));
  VERIFICAR(!proto::interpretarComando("VACIADO,C", c));
  VERIFICAR(!proto::interpretarComando("VACIADO,AB", c));
  VERIFICAR(!proto::interpretarComando("CAPACIDAD,A,0", c));
  VERIFICAR(!proto::interpretarComando("CAPACIDAD,A,1000", c));
  VERIFICAR(!proto::interpretarComando("CAPACIDAD,A", c));
  VERIFICAR(!proto::interpretarComando("PAROX", c));
  VERIFICAR(!proto::interpretarComando("HOLA,1", c) && c.tipo == proto::CMD_DESCONOCIDO);
}

// ---------------------------------------------------------------- Tabla local

static void pruebaTabla() {
  printf("tabla local\n");
  TablaLocal t;
  VERIFICAR(t.cantidad() == 0 && t.version() == 0);
  VERIFICAR(t.buscar("04AA") == '\0');

  VERIFICAR(t.cargar("3,04A1B2C3D4E5F6=A;04112233445566=B;04FFFFFFFFFFFF=R"));
  VERIFICAR(t.version() == 3 && t.cantidad() == 3);
  VERIFICAR(t.buscar("04A1B2C3D4E5F6") == 'A');
  VERIFICAR(t.buscar("04112233445566") == 'B');
  VERIFICAR(t.buscar("04FFFFFFFFFFFF") == 'R');
  VERIFICAR(t.buscar("04A1B2C3D4E5F7") == '\0');
  VERIFICAR(t.buscar("04a1b2c3d4e5f6") == '\0');   // la Pi envia siempre MAYUSCULAS

  // Errores: se conserva la tabla anterior
  VERIFICAR(!t.cargar("4,04AA=C"));                  // categoria invalida
  VERIFICAR(!t.cargar("4,04AA=AB"));
  VERIFICAR(!t.cargar("4,04:AA=A"));                 // ':' no permitido
  VERIFICAR(!t.cargar("4,04aa=A"));                  // minusculas no permitidas
  VERIFICAR(!t.cargar("4,=A"));                      // UID vacio
  VERIFICAR(!t.cargar("4,04AA"));                    // sin '='
  VERIFICAR(!t.cargar("4,04AA=A;04AA=B"));           // UID repetido
  VERIFICAR(!t.cargar("x,04AA=A"));                  // version no numerica
  VERIFICAR(!t.cargar("404AA=A"));                   // sin coma
  VERIFICAR(!t.cargar("4,0123456789ABCDEF01234=A")); // UID de 21 caracteres
  VERIFICAR(t.version() == 3 && t.cantidad() == 3);

  // Tolerancias y casos limite
  VERIFICAR(t.cargar("5,04AA=A;"));                  // ';' final
  VERIFICAR(t.cantidad() == 1 && t.version() == 5);
  VERIFICAR(t.cargar("6,"));                         // tabla vacia valida
  VERIFICAR(t.cantidad() == 0 && t.version() == 6);

  // Capacidad maxima
  char datos[1200] = "7,";
  for (int i = 0; i < MAX_ETIQUETAS; i++) {
    char e[24];
    snprintf(e, sizeof(e), "%s04%012X=A", i ? ";" : "", i);
    strcat(datos, e);
  }
  VERIFICAR(t.cargar(datos) && t.cantidad() == MAX_ETIQUETAS);
  strcat(datos, ";04FFFFFFFFFFFF=B");
  VERIFICAR(!t.cargar(datos));                       // una de mas
}

// ---------------------------------------------------------------- Pendientes

static void pruebaPendientes() {
  printf("cola de pendientes\n");
  ColaPendientes c;
  uint32_t perdido = 99;
  VERIFICAR(c.cantidad() == 0);

  Evento& e1 = c.agregar(1, "04AA", 'A', MOTIVO_OK, 3000, &perdido);
  VERIFICAR(perdido == 0 && e1.seq == 1 && e1.categoria == 'A' && strcmp(e1.uid, "04AA") == 0);
  c.agregar(2, "-", 'R', MOTIVO_SIN_LECTURA, 1500, &perdido);
  VERIFICAR(c.cantidad() == 2);
  VERIFICAR(c.buscar(2) && c.buscar(2)->motivo == MOTIVO_SIN_LECTURA);
  VERIFICAR(c.buscar(3) == nullptr);

  VERIFICAR(c.quitar(1));
  VERIFICAR(!c.quitar(1));   // ACK duplicado
  VERIFICAR(c.cantidad() == 1);

  // Llenar: el siguiente sustituye al seq mas antiguo
  c.vaciar();
  for (uint32_t s = 10; s < 10 + MAX_PENDIENTES; s++) c.agregar(s, "04AA", 'A', MOTIVO_OK, 1, &perdido);
  VERIFICAR(c.cantidad() == MAX_PENDIENTES && perdido == 0);
  c.agregar(100, "04BB", 'B', MOTIVO_OK, 1, &perdido);
  VERIFICAR(perdido == 10);
  VERIFICAR(c.buscar(10) == nullptr && c.buscar(100) != nullptr && c.cantidad() == MAX_PENDIENTES);

  // Se puede copiar como bloque (asi se guarda en NVS)
  ColaPendientes copia;
  memcpy(copia.datos(), c.datos(), ColaPendientes::tamDatos());
  VERIFICAR(copia.cantidad() == MAX_PENDIENTES && copia.buscar(100) != nullptr);

  VERIFICAR(strcmp(nombreMotivo(MOTIVO_OK), "OK") == 0);
  VERIFICAR(strcmp(nombreMotivo(MOTIVO_SIN_LECTURA), "SIN_LECTURA") == 0);
  VERIFICAR(strcmp(nombreMotivo(MOTIVO_NO_REGISTRADA), "NO_REGISTRADA") == 0);
}

// ---------------------------------------------------------------- Deteccion

static void pruebaDeteccion() {
  printf("distancia y filtro de presencia\n");
  // 583 us ~ 10 cm (583 * 0.0343 / 2 = 9.998)
  float d = distanciaCm(583);
  VERIFICAR(d > 9.9f && d < 10.1f);
  VERIFICAR(distanciaCm(0) == 0.0f);

  FiltroPresencia f(2, 3);
  VERIFICAR(!f.presente());
  VERIFICAR(!f.actualizar(true));    // 1 de 2
  VERIFICAR(!f.presente());
  VERIFICAR(f.actualizar(true));     // 2 de 2 -> presente
  VERIFICAR(f.presente());

  VERIFICAR(!f.actualizar(false));   // 1 de 3
  VERIFICAR(!f.actualizar(true));    // se reinicia la cuenta
  VERIFICAR(!f.actualizar(false));
  VERIFICAR(!f.actualizar(false));
  VERIFICAR(f.presente());           // todavia 2 de 3
  VERIFICAR(f.actualizar(false));    // 3 de 3 -> libre
  VERIFICAR(!f.presente());

  // Un eco aislado no activa
  f.reiniciar();
  f.actualizar(true);
  f.actualizar(false);
  f.actualizar(true);
  VERIFICAR(!f.presente());
}

int main() {
  pruebaChecksum();
  pruebaValidar();
  pruebaComandos();
  pruebaTabla();
  pruebaPendientes();
  pruebaDeteccion();

  printf("\n%d verificaciones, %d fallas\n", total, fallas);
  if (fallas == 0) {
    printf("TODAS LAS PRUEBAS PASARON\n");
    return 0;
  }
  return 1;
}
