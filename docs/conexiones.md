# Conexiones del sistema — Banda Clasificadora NFC

Guía para alambrar el prototipo y probarlo módulo por módulo.

> **Estado:** asignación de pines **propuesta**, todavía sin probar en hardware.
> Respeta los pines que ya usan las pruebas de `pruebas/`, salvo **RC522 RST, que pasa de GPIO22 a GPIO4**
> para dejar GPIO22 libre como SCL de la OLED (ver §8).
> Cada vez que algo se verifique físicamente, márcalo en la tabla de verificación (§9).

---

## 1. Antes de conectar: confirmar estos datos

| Dato                                                                          | Por qué importa                                                                                                                                                        |
| ----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Modelo exacto del ESP32 DevKit (30 pines, chip CH340)                         | Los números GPIO son los mismos, pero la **posición física** de los pines cambia. Conecta siempre por la **etiqueta serigrafiada (GPIOxx / Dxx)**, no por la posición. |
| Si la placa tiene diodo entre USB y el pin `VIN`/`5V`                         | Define si se puede tener conectado el USB y la fuente externa al mismo tiempo (§2.3).                                                                                  |
| Tipo de buzzer: **activo** (suena con voltaje DC) o **pasivo** (necesita PWM) | Cambia el código; el cableado de este documento sirve para los dos.                                                                                                    |
| Modelo del sensor IR (el documento dice FC-51)                                | La lógica de salida (activa en bajo) y el voltaje dependen del módulo.                                                                                                 |
| Tipo de botón de paro: normalmente **cerrado** (NC)                           | El esquema de §5.7 n2ecesita un contacto NC. Si solo tienen NO, avisar: cambia el diseño.                                                                              |

---

## 2. Alimentación

### 2.1 Esquema general

```
                 FUENTE 5 V · 2-3 A (ESP32 + periféricos)
                 ┌───────────────┐
                 │  +5V      GND │
                 └───┬───────┬───┘
          ┌──────────┤       │
          │          │       └──────────────────────────────┐  (punto común de tierra)
  RAMA LÓGICA    RAMA POTENCIA                              │
          │          │                                      │
          │          ├── C 1000 µF / 16 V ──┐  (cerca del servo)
          │          ├── Servo SG90 (+)     │               │
          │          └── TB6612 VM          │               │
          │                                 └────── GND ────┤
          ▼                                                 │
   ESP32 VIN (5V) ──► regulador interno ──► 3V3 ──┬── RC522 3.3V
                                                  ├── OLED VCC
                                                  ├── IR inspección VCC
                                                  ├── IR salida VCC
                                                  ├── TB6612 VCC (lógica)
                                                  └── pull-ups (botones)
   ESP32 GND ─────────────────────────────────────────────── GND común
                                                            │
   FUENTE 5 V · 2.5 A micro USB ──► Raspberry Pi 3          │
   Raspberry Pi GND (pin 6) ─────────────────────────────────┘  (vía el hilo GND de la UART)
```

### 2.2 Reglas

1. **Tierra común:** ESP32, fuente de 5 V, TB6612, servo, sensores, OLED, RC522 y Raspberry Pi comparten GND. Sin tierra común, la UART y el PWM del servo no funcionan.
2. **Servo y motor NUNCA desde el pin 3V3 del ESP32.** Van a la rama de potencia de 5 V.
3. **Los dos ramales salen de la bornera de la fuente**, no uno "colgado" del otro. Así la caída de voltaje que provoca el servo al arrancar no llega directo al ESP32 (problema 2 del documento de avance).
4. **Capacitor de 1000 µF (mínimo 470 µF), 16 V o más,** entre +5V y GND de la rama de potencia, lo más cerca posible del servo. **Respeta la polaridad**: la franja del electrolítico es el negativo.
5. **Capacitor cerámico de 100 nF** soldado entre las dos terminales del motor TT, para reducir el ruido eléctrico de las escobillas.
6. **RC522, OLED y sensores IR a 3.3 V.** El RC522 se daña con 5 V. Con los IR a 3.3 V su salida ya es de 3.3 V y **no hace falta divisor de voltaje** (el documento menciona el divisor como alternativa si se alimentan a 5 V).

### 2.3 USB y fuente externa al mismo tiempo

- **Mientras programas por USB:** desconecta el cable +5V de la fuente que va al pin `VIN`, pero **deja conectada la GND común**. La rama de potencia (servo y motor) puede seguir alimentada por la fuente.
- Solo si comprobaron que la placa tiene un diodo entre USB y `VIN`, pueden tener ambos conectados.

### 2.4 Consumo estimado (para verificar con multímetro)

| Carga | Corriente aproximada | Rama |
|---|---|---|
| ESP32 (WiFi apagado) | 50–80 mA | Lógica |
| RC522 | 15–30 mA | 3.3 V |
| OLED 0.96" | 10–20 mA | 3.3 V |
| 2× IR FC-51 | 20–40 mA | 3.3 V |
| LEDs (3) | ~5 mA c/u | 3.3 V |
| Servo SG90 | 100–250 mA moviéndose, **picos de ~650 mA** | Potencia |
| Motor TT a 5 V | 150–250 mA, **arranque/bloqueo ~1 A** | Potencia |

Total en el peor caso: ~2 A → la fuente de **3 A** da margen. Son valores típicos de hojas de datos, no medidos.

---

## 3. Mapa de pines del ESP32

| GPIO | Dirección | Conectado a | Notas |
|---|---|---|---|
| **5**  | Salida | RC522 SDA (SS) | Pin de arranque (*strapping*): debe estar en ALTO al encender; el RC522 no lo impide. Ya usado en `nfc_test`. |
| **18** | Salida | RC522 SCK | Bus VSPI por defecto |
| **23** | Salida | RC522 MOSI | Bus VSPI por defecto |
| **19** | Entrada | RC522 MISO | Bus VSPI por defecto |
| **4**  | Salida | RC522 RST | **Antes GPIO22**, se movió para liberar el I2C |
| **21** | E/S | OLED SDA | I2C por defecto de Arduino-ESP32 |
| **22** | Salida | OLED SCL | I2C por defecto de Arduino-ESP32 |
| **34** | Entrada | IR inspección OUT | Solo entrada, **sin pull-up interno** (el módulo FC-51 ya trae el suyo) |
| **35** | Entrada | IR salida OUT | Solo entrada, sin pull-up interno |
| **13** | Salida PWM | Servo señal | Ya usado en `servo_test` |
| **26** | Salida PWM | TB6612 PWMA | Velocidad de la banda |
| **14** | Salida | TB6612 AIN1 | Dirección. Emite un pulso al arrancar; no mueve el motor porque STBY está en BAJO (§5.7) |
| **27** | Salida | TB6612 AIN2 | Dirección |
| **25** | Salida | Botón de paro → TB6612 STBY | Habilita el motor **pasando por el contacto NC del botón** (§5.7) |
| **39** | Entrada | Detección del paro (nodo STBY) | Solo entrada; con pull-down externo de 10 kΩ |
| **36** | Entrada | Botón **Restablecer** | Solo entrada; con pull-up externo de 10 kΩ |
| **32** | Salida | LED verde | Operación normal |
| **33** | Salida | LED amarillo | Espera / pausa |
| **2**  | Salida | LED rojo | Error. Es el pin del LED azul de la placa (encenderán juntos). *Strapping*: un LED con resistencia a GND no afecta el arranque. |
| **15** | Salida | Buzzer (vía transistor) | *Strapping*: en BAJO al arrancar solo silencia el log de arranque; no afecta el funcionamiento. |
| **17** | Salida | UART2 TX → Raspberry Pi RX | Ya usado en `uart_test` |
| **16** | Entrada | UART2 RX ← Raspberry Pi TX | Ya usado en `uart_test` |

**Pines que NO deben usarse:**
- **GPIO 6–11:** conectados a la memoria flash interna.
- **GPIO 1 y 3:** son el puerto USB-serial (Monitor Serial y programación).
- **GPIO 12:** si está en ALTO al encender, el ESP32 no arranca.
- **GPIO 0:** botón BOOT.

Libres de reserva: ninguno cómodo. Si se necesita otro pin, la opción más fácil es fijar AIN1/AIN2 a 3.3 V/GND (la banda solo avanza hacia adelante) y liberar GPIO14 y GPIO27.

---

## 4. Raspberry Pi 3 Model B

| Pin físico | Función | Conectar a |
|---|---|---|
| **8**  | GPIO14 · TXD | ESP32 **GPIO16** (RX2) |
| **10** | GPIO15 · RXD | ESP32 **GPIO17** (TX2) |
| **6**  | GND | ESP32 GND |

```
 Raspberry Pi (header de 40 pines, vista superior, pin 1 junto a la ranura microSD)
   pin 2  (5V)   pin 4 (5V)   pin 6 (GND) ●   pin 8 (TXD) ●   pin 10 (RXD) ●
   pin 1  (3V3)  pin 3        pin 5          pin 7            pin 9
```

- **TX va con RX** (cruzado). Es el error más común.
- **No conectes los pines de 5 V de la Raspberry Pi al ESP32.** Cada uno tiene su propia fuente; solo se unen las tierras.
- Ambos trabajan a 3.3 V: no hace falta convertidor de nivel.
- En la Raspberry Pi: `sudo raspi-config` → *Interface Options* → *Serial Port* → login shell: **No**, hardware: **Sí** → reiniciar.

---

## 5. Conexión de cada módulo

### 5.1 Lector NFC RC522 (SPI)

| RC522 | ESP32 |
|---|---|
| SDA (SS) | GPIO5 |
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| IRQ | sin conectar |
| GND | GND |
| RST | **GPIO4** |
| 3.3V | 3V3 |

- **Solo 3.3 V.**
- Cables cortos (menos de 15 cm): el SPI es sensible a cables largos en protoboard.
- Montaje: la etiqueta debe pasar a **~2 cm o menos** de la antena y en la misma orientación (problema 1 del avance). Evita metal detrás de la antena.

### 5.2 Pantalla OLED 0.96" I2C (SSD1306)

| OLED | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO22 |
| SDA | GPIO21 |

- Dirección I2C habitual: `0x3C` (algunas son `0x3D`). Se confirma con un escáner I2C.
- **Revisa el orden de los pines en tu módulo**: algunos traen VCC y GND invertidos.

### 5.3 Sensores IR de obstáculo (FC-51)

| FC-51 | IR inspección | IR salida |
|---|---|---|
| VCC | 3V3 | 3V3 |
| GND | GND | GND |
| OUT | **GPIO34** | **GPIO35** |

- Salida **activa en BAJO**: `LOW` = hay objeto, `HIGH` = libre. El LED del módulo se enciende al detectar.
- Ajustar el potenciómetro azul para que detecte la pieza a la distancia real de montaje y **no** detecte la banda ni la estructura.
- La luz solar directa y las superficies negras mates afectan la detección: probar con las piezas reales.

### 5.4 Servomotor SG90

| Cable del servo | Conectar a |
|---|---|
| Café / negro (GND) | GND común |
| Rojo (V+) | **+5 V rama de potencia** (no al ESP32) |
| Naranja / amarillo (señal) | GPIO13 |

- Posiciones (de `servo_test`): 0° = Categoría A, 90° = centro/rechazo, 180° = Categoría B. Ajustar los ángulos reales cuando la compuerta esté montada.
- La señal de 3.3 V del ESP32 suele funcionar con el SG90. Si el servo tiembla o no responde, revisar primero la alimentación y la tierra común.

### 5.5 Driver TB6612FNG + motor TT

| TB6612FNG | Conectar a |
|---|---|
| VM | +5 V rama de potencia |
| VCC | 3V3 del ESP32 |
| GND (cualquiera de los 3) | GND común |
| PWMA | GPIO26 |
| AIN1 | GPIO14 |
| AIN2 | GPIO27 |
| STBY | Nodo del botón de paro (§5.7) |
| AO1, AO2 | Terminales del motor TT |
| PWMB, BIN1, BIN2, BO1, BO2 | Sin conectar (canal B no usado) |

Tabla de control (canal A):

| AIN1 | AIN2 | PWMA | Resultado |
|---|---|---|---|
| H | L | PWM | Avanza (velocidad según el ciclo de trabajo) |
| L | H | PWM | Retrocede |
| H | H | x | Freno |
| L | L | x | Libre |
| x | x | x | **STBY = L → motor apagado** |

- VM mínimo del TB6612: 4.5 V. Si la fuente cae por debajo al moverse el servo, el driver se apaga: otra razón para usar el capacitor de §2.2.
- Si la banda avanza al revés, intercambia AO1 y AO2 (o AIN1 y AIN2 en el código).

### 5.6 LEDs y buzzer

```
GPIO32 ──[330 Ω]──►|── GND      LED verde    (ánodo, pata larga, hacia la resistencia)
GPIO33 ──[330 Ω]──►|── GND      LED amarillo
GPIO2  ──[330 Ω]──►|── GND      LED rojo

                 +5V (rama lógica)
                   │
                [BUZZER]  (+ arriba, − abajo)
                   │
GPIO15 ──[1 kΩ]── B  NPN (2N2222 / BC547)
                   C ── a la terminal − del buzzer
                   E ── GND
```

- El buzzer va con transistor porque puede consumir más corriente de la que un GPIO entrega con seguridad (máximo recomendado ~12–20 mA).
- Si el buzzer es **magnético** (no piezoeléctrico), agrega un diodo 1N4148 en paralelo con el buzzer: cátodo (franja) hacia +5 V.
- Revisa la orientación del 2N2222 o del BC547 en su hoja de datos: el orden de E, B y C es diferente en cada uno.

### 5.7 Botón de paro de emergencia (detención por hardware)

El documento pide que el paro **no dependa solo del programa**. Con un botón **NC** (normalmente cerrado) se logra así:

```
GPIO25 ─────o  o───────┬──────────► TB6612 STBY
          botón NC     │
          (paro)       ├──────────► GPIO39 (detección)
                       │
                     [10 kΩ]
                       │
                      GND
```

- **En operación** (botón sin presionar, contacto cerrado): el programa pone GPIO25 en ALTO → STBY = ALTO → el motor puede moverse. GPIO39 lee ALTO.
- **Al presionar el paro** (contacto abierto): la resistencia de 10 kΩ lleva STBY a BAJO → **el driver apaga el motor por hardware**, aunque el programa esté trabado. GPIO39 lee BAJO → interrupción en el código → servo al centro, LED rojo y buzzer.
- **Si se corta un cable del botón**, el efecto es el mismo que presionarlo (diseño a prueba de fallas).
- **Al encender el ESP32**, GPIO25 está en BAJO hasta que el programa lo activa → el motor no arranca solo.
- En el código, el paro se detecta cuando **GPIO25 = ALTO y GPIO39 = BAJO**.
- **Limitación:** el servo **no** se corta por hardware; se lleva al centro por software. Con un SG90 el riesgo es bajo, pero hay que mencionarlo en el reporte. Para cortar también el servo haría falta un botón de dos contactos o un relé.

> Esto es una **recomendación de este documento**: el avance solo especifica "botón con interrupción". Conviene confirmarlo con el profesor o con el equipo.

### 5.8 Botón Restablecer

Lo usa el estado de error ("hasta presionar reinicio" en la tabla de errores del documento).

```
3V3 ──[10 kΩ]──┬────── GPIO36
               │
               o  botón NO (normalmente abierto)
               o
               │
              GND
```

- Sin presionar: GPIO36 = ALTO. Presionado: BAJO.
- GPIO36 no tiene pull-up interno, por eso la resistencia externa es obligatoria.
- Opcional: capacitor de 100 nF entre GPIO36 y GND para reducir el rebote.

---

## 6. Lista de material para el cableado

| Material | Cantidad |
|---|---|
| Resistencias 330 Ω | 3 |
| Resistencias 10 kΩ | 2 |
| Resistencia 1 kΩ | 1 |
| Transistor NPN 2N2222 o BC547 | 1 |
| Capacitor electrolítico 470–1000 µF, ≥16 V | 1 |
| Capacitores cerámicos 100 nF | 2–3 |
| Diodo 1N4148 (solo si el buzzer es magnético) | 1 |
| Botón NC (paro de emergencia, de preferencia tipo hongo) | 1 |
| Botón NO (Restablecer) | 1 |
| Bornera o regleta para la fuente de 5 V | 1 |

---

## 7. Orden de montaje y prueba

Conecta y prueba **un módulo a la vez**. No pases al siguiente hasta que el anterior cumpla su criterio de éxito.

| # | Módulo | Cómo probar | Criterio de éxito | Prueba disponible |
|---|---|---|---|---|
| 0 | Fuente y tierras | Multímetro **sin módulos conectados** | 4.9–5.2 V en ambas ramas; continuidad entre todas las GND; **sin** continuidad entre +5V y GND | — |
| 1 | ESP32 solo | Cargar un *blink* | Carga sin errores; el LED de la placa parpadea | Ejemplo *Blink* del IDE |
| 2 | Servo | `servo_test` | 3 posiciones repetibles; el ESP32 no se reinicia | `pruebas/servo_test` |
| 3 | RC522 | `nfc_test` (**actualizado a RST = GPIO4**) | Firmware `0x91`/`0x92`; mismo UID en cada lectura; UID de 7 bytes con las NTAG213 | `pruebas/nfc_test` |
| 4 | OLED | Escáner I2C, luego un texto | Aparece la dirección `0x3C`/`0x3D`; se ve el texto | **Falta** |
| 5 | IR × 2 | Leer GPIO34/35 por el Monitor Serial | Cambian de 1 a 0 al pasar la pieza, a la altura real de montaje | **Falta** |
| 6 | Motor + TB6612 | PWM al 30, 60 y 100 %; ambos sentidos | El motor gira y cambia de velocidad; el ESP32 no se reinicia | **Falta** |
| 7 | Paro de emergencia | Con el motor girando, presionar el botón | El motor se detiene **aunque el programa no haga nada**; GPIO39 cambia a 0 | **Falta** |
| 8 | LEDs, buzzer, Restablecer | Encender cada salida y leer el botón | Cada LED y el buzzer funcionan; GPIO36 cambia al presionar | **Falta** |
| 9 | UART con Raspberry Pi | `uart_test` en ambos lados | PING/PONG con la secuencia correcta durante 5 minutos sin errores | `pruebas/uart_test` |
| 10 | Todo junto | Servo + motor moviéndose a la vez | El ESP32 **no se reinicia** y el RC522 sigue leyendo | — |

El paso 10 es el que valida el problema de caída de voltaje del avance. Si falla, medir el voltaje en `VIN` del ESP32 mientras arranca el motor.

---

## 8. Cambios respecto a las pruebas existentes

| Prueba | Cambio | Motivo |
|---|---|---|
| `pruebas/nfc_test/nfc_test.ino` | `RST_PIN` 22 → **4** | GPIO22 es el SCL por defecto del I2C, necesario para la OLED |
| `servo_test`, `uart_test` | Sin cambios | — |

---

## 9. Tabla de verificación

Llenar conforme se prueba. Una conexión no se marca como verificada sin evidencia (foto, captura del Monitor Serial o medición).

| Módulo                | Conectado | Probado | Fecha | Evidencia / notas |
| --------------------- | --------- | ------- | ----- | ----------------- |
| Fuente y tierras      | ☐         | ☐       |       |                   |
| ESP32                 | ☐         | ☐       |       |                   |
| Servo SG90            | ☐         | ☐       |       |                   |
| RC522                 | ☐         | ☐       |       |                   |
| OLED                  | ☐         | ☐       |       |                   |
| IR inspección         | ☐         | ☐       |       |                   |
| IR salida             | ☐         | ☐       |       |                   |
| TB6612 + motor        | ☐         | ☐       |       |                   |
| Paro de emergencia    | ☐         | ☐       |       |                   |
| LEDs / buzzer         | ☐         | ☐       |       |                   |
| Botón Restablecer     | ☐         | ☐       |       |                   |
| UART con Raspberry Pi | ☐         | ☐       |       |                   |
| Sistema completo      | ☐         | ☐       |       |                   |
