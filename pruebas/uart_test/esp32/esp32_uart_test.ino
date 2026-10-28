// Prueba UART: ESP32 <-> Raspberry Pi
//
// Conexiones:
//   ESP32 GPIO17 (TX2) ----> RPi GPIO15 (RXD / pin fisico 10)
//   ESP32 GPIO16 (RX2) <---- RPi GPIO14 (TXD / pin fisico 8)
//   ESP32 GND -------------- RPi GND   (pin fisico 6)
//
//   Ambos trabajan a 3.3V logico, no se necesita divisor de voltaje.
//
// Funcionamiento:
//   El ESP32 envia "PING:<n>\n" cada 2 segundos por UART2.
//   Espera recibir "PONG:<n>\n" de la Raspberry Pi.
//   Todo se muestra por el Monitor Serial (USB) para depuracion.
//
// Criterio de exito:
//   - El Monitor Serial muestra "Enviado: PING:1", "Recibido: PONG:1", etc.
//   - Si no llega respuesta en 1 segundo, muestra "Sin respuesta".

#define BAUD_RATE 115200
#define UART2_TX 17
#define UART2_RX 16

uint32_t secuencia = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(BAUD_RATE, SERIAL_8N1, UART2_RX, UART2_TX);

  Serial.println("=== Prueba UART ESP32 ===");
  Serial.println("Enviando PING cada 2 segundos por UART2...");
  Serial.println("Esperando PONG de la Raspberry Pi...");
  Serial.println();
}

void loop() {
  secuencia++;

  String mensaje = "PING:" + String(secuencia) + "\n";
  Serial2.print(mensaje);
  Serial.print("Enviado:  PING:");
  Serial.println(secuencia);

  unsigned long inicio = millis();
  String respuesta = "";
  bool recibido = false;

  while (millis() - inicio < 1000) {
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n') {
        recibido = true;
        break;
      }
      respuesta += c;
    }
  }

  if (recibido) {
    Serial.print("Recibido: ");
    Serial.println(respuesta);

    if (respuesta.startsWith("PONG:" + String(secuencia))) {
      Serial.println(">> OK - Secuencia correcta");
    } else {
      Serial.println(">> ADVERTENCIA - Secuencia no coincide");
    }
  } else {
    Serial.println(">> Sin respuesta (timeout 1s)");
  }

  Serial.println("---");
  delay(2000);
}
