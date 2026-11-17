# Conexiones del sistema — Banda Clasificadora NFC

Guía para alambrar el prototipo y probarlo módulo por módulo.

> **Estado:** asignación de pines **propuesta**, todavía sin registro de pruebas en este documento.
> Cambios de diseño decididos por el equipo y reflejados aquí: **los dos sensores IR (FC-51) se sustituyen por dos HC-SR04**,
> y **GPIO14/GPIO27 pasan a ser `Trig`** de esos sensores, por lo que **AIN1/AIN2 del TB6612 se fijan por cableado** (la banda solo avanza).
> Respeta los pines que ya usan las pruebas de `pruebas/`, salvo **RC522 RST, que pasa de GPIO22 a GPIO4**
> para dejar GPIO22 libre como SCL de la OLED (ver §8).
> Los valores del HC-SR04 de este documento (5 V, `Echo` de 5 V, rango, haz) son de hoja de datos típica y **deben comprobarse con su módulo**.
> Cada vez que algo se verifique físicamente, márcalo en la tabla de verificación (§9).

---

## 1. Antes de conectar: confirmar estos datos

| Dato                                                                          | Por qué importa                                                                                                                                                        |
| ----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Modelo exacto del ESP32 DevKit (30 pines, chip CH340)                         | Los números GPIO son los mismos, pero la **posición física** de los pines cambia. Conecta siempre por la **etiqueta serigrafiada (GPIOxx / Dxx)**, no por la posición. |
| Si la placa tiene diodo entre USB y el pin `VIN`/`5V`                         | Define si se puede tener conectado el USB y la fuente externa al mismo tiempo (§2.3).                                                                                  |
| Tipo de buzzer: **activo** (suena con voltaje DC) o **pasivo** (necesita PWM) | Cambia el código; el cableado de este documento sirve para los dos.                                                                                                    |
| Versión del **HC-SR04** (clon): si su `Echo` es de **5 V** (lo habitual) o de 3.3 V | Si es de 5 V, el divisor de §5.3 es **obligatorio**. Medir `Echo` con multímetro antes de conectarlo al ESP32 |
| Tipo de botón de paro: normalmente **cerrado** (NC)                           | El esquema de §5.7 necesita un contacto NC. Si solo tienen NO, avisar: cambia el diseño.                                                                              |

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
          │          ├── C 1000 µF / 16 V ──┐  (cerca de los servos)
          │          ├── Servo 1 + Servo 2 (+)              │
          │          └── TB6612 VM          │               │
          │                                 └────── GND ────┤
          ▼                                                 │
   ESP32 VIN (5V) ──► regulador interno ──► 3V3 ──┬── RC522 3.3V
                                                  ├── OLED VCC
                                                  ├── TB6612 VCC (lógica)
                                                  └── pull-ups (botones)
   ESP32 GND ─────────────────────────────────────────────── GND común
                                                            │
   FUENTE 5 V · 2.5 A micro USB ──► Raspberry Pi 3          │
   Raspberry Pi GND (pin 6) ─────────────────────────────────┘  (vía el hilo GND de la UART)
```

La **rama lógica de 5 V** (la que entra por `VIN`) también alimenta el `VCC` de los **dos HC-SR04**; sus `Echo` van al ESP32 solo a través del divisor de §5.3.

### 2.2 Reglas

1. **Tierra común:** ESP32, fuente de 5 V, TB6612, servo, sensores, OLED, RC522 y Raspberry Pi comparten GND. Sin tierra común, la UART y el PWM del servo no funcionan.
2. **Servo y motor NUNCA desde el pin 3V3 del ESP32.** Van a la rama de potencia de 5 V.
3. **Los dos ramales salen de la bornera de la fuente**, no uno "colgado" del otro. Así la caída de voltaje que provoca el servo al arrancar no llega directo al ESP32 (problema 2 del documento de avance).
4. **Capacitor de 1000 µF (mínimo 470 µF), 16 V o más,** entre +5V y GND de la rama de potencia, lo más cerca posible de los dos servos. **Respeta la polaridad**: la franja del electrolítico es el negativo.
5. **Capacitor cerámico de 100 nF** soldado entre las dos terminales del motor TT, para reducir el ruido eléctrico de las escobillas.
6. **RC522 y OLED a 3.3 V; HC-SR04 a 5 V.** El RC522 se daña con 5 V. Los dos HC-SR04 se alimentan de la **rama lógica de 5 V** (no de 3V3) y su pin `Echo` entrega **5 V**, que **no debe llegar directo al ESP32** (sus GPIO son de 3.3 V): cada `Echo` pasa por un **divisor de voltaje** (§5.3). `Trig` es una salida de 3.3 V del ESP32; el HC-SR04 típico la reconoce, pero se comprueba en la prueba.

### 2.3 USB y fuente externa al mismo tiempo

- **Mientras programas por USB:** desconecta el cable +5V de la fuente que va al pin `VIN`, pero **deja conectada la GND común**. La rama de potencia (servo y motor) puede seguir alimentada por la fuente.
- Solo si comprobaron que la placa tiene un diodo entre USB y `VIN`, pueden tener ambos conectados.

### 2.4 Consumo estimado (para verificar con multímetro)

| Carga | Corriente aproximada | Rama |
|---|---|---|
| ESP32 (WiFi apagado) | 50–80 mA | Lógica |
| RC522 | 15–30 mA | 3.3 V |
| OLED 0.96" | 10–20 mA | 3.3 V |
| 2× HC-SR04 | ~15 mA c/u (típico) | **5 V lógica** |
| LEDs (3) | ~5 mA c/u | 3.3 V |
| 2× Servo SG90 (se mueven a la vez) | 100–250 mA c/u moviéndose, **picos de ~650 mA c/u (~1.3 A juntos)** | Potencia |
| Motor TT a 5 V | 150–250 mA, **arranque/bloqueo ~1 A** | Potencia |

Total en el peor caso: ~2.7 A → con una fuente de **3 A** el margen es **justo**; con una de 2 A **no alcanza** (usar la de 3 A o más). Son valores típicos de hojas de datos, no medidos.

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
| **34** | Entrada | HC-SR04 inspección **Echo** (vía divisor) | Solo entrada, sin pull-up interno. **Nunca 5 V directo** |
| **35** | Entrada | HC-SR04 salida **Echo** (vía divisor) | Solo entrada, sin pull-up interno. **Nunca 5 V directo** |
| **13** | Salida PWM | Servo 1 (lateral **izquierdo**) señal | Ya usado en `servo_test` |
| **12** | Salida PWM | Servo 2 (lateral **derecho**) señal | **Nuevo.** Pin de arranque: si está en ALTO al encender, el ESP32 no arranca. La entrada del SG90 no lo sube, pero **comprobarlo** (§5.4) |
| **26** | Salida PWM | TB6612 PWMA | Velocidad de la banda |
| **14** | Salida | HC-SR04 inspección **Trig** | Antes AIN1. Emite un pulso al arrancar: solo provoca una medición espuria que el firmware debe ignorar |
| **27** | Salida | HC-SR04 salida **Trig** | Antes AIN2 |
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
- **GPIO 12:** solo para la señal del servo 2 (no le pongas pull-up ni nada que lo suba al encender; si estuviera en ALTO al arrancar, el ESP32 no inicia).
- **GPIO 0:** botón BOOT.

Libres de reserva: ninguno cómodo. GPIO14 y GPIO27 ya no están disponibles: ahora son `Trig`. **Consecuencia:** AIN1 y AIN2 del TB6612 van fijos por cableado (§5.5) y la banda **no tiene reversa**. GPIO12 se ocupó para el segundo servo. Cualquier función nueva que necesite un pin de salida exige otra decisión.

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

### 5.3 Sensores ultrasónicos HC-SR04 (reemplazan a los IR FC-51)

| HC-SR04 | Sensor inspección | Sensor salida |
|---|---|---|
| VCC | **+5 V rama lógica** | **+5 V rama lógica** |
| GND | GND común | GND común |
| Trig | **GPIO14** | **GPIO27** |
| Echo | **GPIO34**, vía divisor | **GPIO35**, vía divisor |

**Divisor de voltaje en cada `Echo`** (de 5 V a un nivel seguro para el ESP32):

```
HC-SR04 Echo ──[1 kΩ]──┬────► GPIO34 (o GPIO35)
                       │
                     [2 kΩ]   (o dos de 1 kΩ en serie)
                       │
                      GND
```

- Con 1 kΩ y 2 kΩ, un `Echo` de 5 V llega al pin en ~3.3 V (relación 2/3). **Mide con el multímetro** ese nodo antes de conectarlo al ESP32.
- Si el `Echo` de su módulo ya es de 3.3 V, el divisor no es necesario (comprobarlo, no suponerlo).
- `Trig` no necesita divisor: es salida del ESP32.
- **Funcionamiento (hoja de datos típica, por verificar):** un pulso corto (~10 µs) en `Trig` y el `Echo` dura proporcional a la distancia; el firmware mide ese tiempo y compara con un **umbral en cm**. No hay `LOW/HIGH` de "objeto detectado" ni potenciómetro de ajuste.
- **Mide los dos sensores alternados, no a la vez**, para evitar que uno capte el eco del otro. Oriéntalos para que no se apunten entre sí.
- Hay una **distancia mínima** de detección y un haz cónico: comprueba con la pieza real que se mide bien al colocarla donde el RC522 la lee (~2 cm) y que el sensor **no** detecte la banda ni la estructura.
- Las piezas pequeñas, blandas o inclinadas pueden no devolver eco: prueba con las piezas reales.
- El pulso de GPIO14 al arrancar (§3) puede disparar una medición falsa; el firmware debe descartar las primeras mediciones.

### 5.4 Compuerta doble: dos servos SG90

Dos servos, uno en cada lateral de la banda, cada uno con un brazo (paleta).

| Cable | Servo 1 (lateral izquierdo) | Servo 2 (lateral derecho) |
|---|---|---|
| Café / negro (GND) | GND común | GND común |
| Rojo (V+) | **+5 V rama de potencia** | **+5 V rama de potencia** |
| Naranja / amarillo (señal) | **GPIO13** | **GPIO12** |

**Funcionamiento** (vista desde arriba, la banda avanza hacia abajo):

```
   CENTRO (rechazo)      IZQUIERDA (cat. A)     DERECHA (cat. B)
   |  S1      S2  |      |  S1      S2  |       |  S1      S2  |
   |  |        |  |      |   /       /  |       |  \       \   |
   |  |        |  |      |  /       /   |       |   \       \  |
   v  pasa de frente     <- sale a la izq.      sale a la der. ->
```

- **Centro:** los dos brazos quedan paralelos a la banda; la pieza sin categoría (o no registrada) **pasa de frente** al contenedor de rechazo.
- **Categoría A → izquierda; categoría B → derecha.**
- Los dos servos reciben **el mismo giro, al mismo tiempo y en el mismo sentido**: los brazos quedan paralelos entre sí y **no pueden chocar**. El brazo del lado contrario al desvío empuja; el otro se abre hacia fuera.
- **Montaje:** los dos servos con el eje **hacia el mismo lado** (por ejemplo, ambos hacia arriba). Si uno se monta al revés, girarían en sentidos opuestos y chocarían; se corrige con `INVERTIR_S1/S2` en `servo_test`.
- Los ángulos (`CENTRO_S1`, `CENTRO_S2`, `DESVIO_GRADOS`, por defecto 90° y ±45°) son **provisionales**: se calibran con los brazos montados siguiendo el encabezado de `pruebas/servo_test/servo_test.ino`.
- **Comprobación de GPIO12:** con el servo 2 conectado, presiona EN/RST; el ESP32 debe arrancar normalmente. Si no arranca, conecta la señal del servo 2 también a GPIO13 (en paralelo con el servo 1): se mueven igual, pero ya no se calibran por separado.
- La señal de 3.3 V del ESP32 suele funcionar con el SG90. Si un servo tiembla o el ESP32 se reinicia al moverlos juntos, revisar primero la fuente (§2.4), el capacitor y la tierra común.

### 5.5 Driver TB6612FNG + motor TT

| TB6612FNG | Conectar a |
|---|---|
| VM | +5 V rama de potencia |
| VCC | 3V3 del ESP32 |
| GND (cualquiera de los 3) | GND común |
| PWMA | GPIO26 |
| AIN1 | **3.3 V (fijo)** — ya no va a un GPIO |
| AIN2 | **GND (fijo)** — ya no va a un GPIO |
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

> Con AIN1/AIN2 fijos solo aplica la **primera fila**. Las demás se conservan como referencia del driver.

- VM mínimo del TB6612: 4.5 V. Si la fuente cae por debajo al moverse el servo, el driver se apaga: otra razón para usar el capacitor de §2.2.
- **No hay reversa ni freno por software:** con AIN1 = H y AIN2 = L fijos (§ tabla, primera fila) el motor solo avanza; solo se controlan `PWMA` y `STBY`. Si la banda avanza al revés, **intercambia AO1 y AO2** en el motor (ya no se puede corregir en código).

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
- **Al presionar el paro** (contacto abierto): la resistencia de 10 kΩ lleva STBY a BAJO → **el driver apaga el motor por hardware**, aunque el programa esté trabado. GPIO39 lee BAJO → interrupción en el código → servos al centro, LED rojo y buzzer.
- **Si se corta un cable del botón**, el efecto es el mismo que presionarlo (diseño a prueba de fallas).
- **Al encender el ESP32**, GPIO25 está en BAJO hasta que el programa lo activa → el motor no arranca solo.
- En el código, el paro se detecta cuando **GPIO25 = ALTO y GPIO39 = BAJO**.
- **Limitación:** los servos **no** se cortan por hardware; se llevan al centro por software. Con un SG90 el riesgo es bajo, pero hay que mencionarlo en el reporte. Para cortar también el servo haría falta un botón de dos contactos o un relé.

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
| Resistencias 1 kΩ | 3 (1 del buzzer + 2 del divisor de `Echo`) |
| Resistencias 2 kΩ (o 2 de 1 kΩ en serie cada una) | 2 (divisor de `Echo`) |
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
| 2 | 2× Servo (compuerta) | `servo_test` | Centro/izquierda/derecha repetibles; los dos brazos giran a la vez sin tocarse; el ESP32 arranca con GPIO12 conectado y no se reinicia | `pruebas/servo_test` (actualizada a 2 servos, sin probar en hardware) |
| 3 | RC522 | `nfc_test` (**actualizado a RST = GPIO4**) | Firmware `0x91`/`0x92`; mismo UID en cada lectura; UID de 7 bytes con las NTAG213 | `pruebas/nfc_test` |
| 4 | OLED | Escáner I2C, luego un texto | Aparece la dirección `0x3C`/`0x3D`; se ve el texto | `pruebas/oled_test` (sin probar en hardware) |
| 5 | HC-SR04 × 2 | **Antes de conectar:** alimentar a 5 V y medir con multímetro el voltaje de `Echo` y de la salida del divisor. Luego leer la distancia por el Monitor Serial | `Echo` llega al ESP32 a ≤3.3 V; distancia con banda libre estable; cambia claramente al pasar la pieza real; un sensor no capta al otro; distancia mínima medida y compatible con la geometría | `pruebas/ultrasonico_test` (sin probar en hardware; el encabezado del .ino explica la medición del divisor) |
| 6 | Motor + TB6612 | PWM al 30, 60 y 100 % con AIN1/AIN2 fijos (solo avanza); confirmar el sentido | El motor gira **hacia adelante** y cambia de velocidad; si gira al revés, intercambiar AO1/AO2; el ESP32 no se reinicia | `pruebas/motor_test` (sin probar en hardware) |
| 7 | Paro de emergencia | Con el motor girando, presionar el botón | El motor se detiene **aunque el programa no haga nada**; GPIO39 cambia a 0 | `pruebas/paro_test`, modo `b`: congela el programa a propósito (sin probar en hardware) |
| 8 | LEDs, buzzer, Restablecer | Encender cada salida y leer el botón | Cada LED y el buzzer funcionan; GPIO36 cambia al presionar | `pruebas/indicadores_test` (sin probar en hardware) |
| 9 | UART con Raspberry Pi | `uart_test` en ambos lados | PING/PONG con la secuencia correcta durante 5 minutos sin errores | `pruebas/uart_test` |
| 10 | Todo junto | Servo + motor moviéndose a la vez | El ESP32 **no se reinicia** y el RC522 sigue leyendo | `firmware/banda_clasificadora` (comando `s` del Monitor Serial) |

El paso 10 es el que valida el problema de caída de voltaje del avance. Si falla, medir el voltaje en `VIN` del ESP32 mientras arranca el motor.

---

## 8. Cambios respecto a las pruebas existentes

| Prueba | Cambio | Motivo |
|---|---|---|
| `pruebas/nfc_test/nfc_test.ino` | `RST_PIN` 22 → **4** | GPIO22 es el SCL por defecto del I2C, necesario para la OLED |
| `pruebas/servo_test/servo_test.ino` | **Dos servos** (GPIO13 y GPIO12), movimiento simultáneo en el mismo sentido | Compuerta lateral doble: A izquierda, B derecha, rechazo de frente |
| `pruebas/motor_test/motor_test.ino` | Sin botón de paro: GPIO25 directo a STBY; arranca solo al 100 % | Diagnóstico del motor que no giraba |
| `uart_test` | Sin cambios | — |
| `firmware/banda_clasificadora` | Compuerta doble (GPIO13 y GPIO12), misma lógica que `servo_test`; ángulos en `config.h` | Compuerta lateral doble |
| `pruebas/ir_test/ir_test.ino` | **Obsoleto** | Los IR se sustituyen por HC-SR04 |
| `pruebas/ultrasonico_test` | **Nueva** (escrita, compila, sin probar en hardware) | Medir distancia, calibrar umbral (modo `c`), interferencia entre sensores (modo `x`) |
| `pruebas/oled_test`, `motor_test`, `paro_test`, `indicadores_test` | **Nuevas** (escritas, compilan, sin probar en hardware) | Pasos 4, 6, 7 y 8 de §7 |
| `esp32_integracion_test.ino` | **Dos servos** (GPIO13 y GPIO12), misma lógica que `servo_test` | No usa sensores ni motor. El sistema completo está en `firmware/banda_clasificadora` |

---

## 9. Tabla de verificación

Llenar conforme se prueba. Una conexión no se marca como verificada sin evidencia (foto, captura del Monitor Serial o medición).

> **Nota:** el equipo indica que ya probó en físico el servo, el RC522 y la UART con la Raspberry Pi, pero **aquí no hay evidencia registrada**. Cuando tengan la captura, foto o medición, marquen la fila y anoten fecha y enlace al archivo.

| Módulo                | Conectado | Probado | Fecha | Evidencia / notas |
| --------------------- | --------- | ------- | ----- | ----------------- |
| Fuente y tierras      | ☐         | ☐       |       |                   |
| ESP32                 | ☐         | ☐       |       |                   |
| Servo 1 (izq., GPIO13) | ☐         | ☐       |       |                   |
| Servo 2 (der., GPIO12) | ☐         | ☐       |       |                   |
| RC522                 | ☐         | ☐       |       |                   |
| OLED                  | ☐         | ☐       |       |                   |
| HC-SR04 inspección    | ☐         | ☐       |       |                   |
| HC-SR04 salida        | ☐         | ☐       |       |                   |
| TB6612 + motor        | ☐         | ☐       |       |                   |
| Paro de emergencia    | ☐         | ☐       |       |                   |
| LEDs / buzzer         | ☐         | ☐       |       |                   |
| Botón Restablecer     | ☐         | ☐       |       |                   |
| UART con Raspberry Pi | ☐         | ☐       |       |                   |
| Sistema completo      | ☐         | ☐       |       |                   |
