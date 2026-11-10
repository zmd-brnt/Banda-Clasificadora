"""Supervisor de la banda clasificadora NFC (Raspberry Pi).

Modulos:
  protocolo   tramas UART (Formato A ampliado), validacion y armado de comandos
  db          base de datos SQLite (etiquetas, piezas, alertas, contenedores)
  supervisor  logica del enlace con el ESP32 (ACK, duplicados, huecos, SYNC, latido)
  servidor    servidor HTTP: panel web, API JSON y eventos en vivo (SSE)
"""
