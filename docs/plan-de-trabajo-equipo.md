# Plan de trabajo del equipo — qué falta y cómo dividirlo en 4 módulos

> **Cómo leer este documento.** Cada afirmación lleva una etiqueta de origen:
>
> - **[Repo]** — está escrito en un archivo del repositorio (se indica cuál).
> - **[Recomendación]** — es una propuesta de este documento, **no** una decisión del equipo ni del profesor.
> - **[Por confirmar]** — no hay información en el repositorio; el equipo debe responderla antes de decidir.
>
> Lo que no aparece en el repositorio **no se da por hecho**. Por ejemplo: el repositorio no contiene cronograma,
> presupuesto detallado ni fecha de la entrega final (el documento de avance en `docs/` solo llega a la
> sección 11); eso se pregunta en la sección 8, no se inventa.

---

## 1. Resumen ejecutivo

1. **Arquitectura definida [Repo: `docs/Avance2ProyectoFinal_Equipo 07.md`]:** ESP32 = control en tiempo real
   (IR, RC522, motor, servo, OLED, avisos, paro). Raspberry Pi 3 B = supervisor (historial, tabla maestra de
   etiquetas, panel web). Enlace UART de 3 hilos. El ESP32 debe seguir clasificando solo si la Pi falla.
2. **Estado real: nada está verificado en hardware.** La tabla de verificación de `docs/conexiones.md §9` está
   completamente vacía (13 filas sin marcar), la asignación de pines se declara "propuesta, todavía sin probar
   en hardware", y `pruebas/integracion_test/README.md` dice textualmente: *"Todavía no se ha probado en el
   hardware real"*. Existe código escrito y compilado, pero **no hay evidencia en el repo de que algún módulo
   haya pasado su criterio de éxito**. El equipo indica (en conversación, no en el repo) que **sí corrió en físico** las
   pruebas individuales (servo, NFC, IR, UART); falta **registrar sus resultados** (§8, pregunta 13).
   Además, los IR se sustituyen por **HC-SR04** (§1.1), por lo que lo probado de `ir_test` no es reutilizable.
3. **Lo que ya existe es código de prueba, no el sistema final.** La prueba de integración
   (`esp32_integracion_test.ino`, 822 líneas) cubre RC522 + servo + UART, **sin** IR, motor, OLED, LEDs, buzzer,
   botones ni contenedor lleno. La Raspberry Pi tiene un script de consola que guarda un CSV; **no hay
   servidor, base de datos ni conexión del panel web a datos reales**. El panel web existe solo como
   prototipo visual con un simulador del ESP32 dentro del navegador.
4. **Restricción que condiciona cómo trabajar en paralelo:** la lista de hardware pide **1** ESP32, **1** RC522,
   **1** Raspberry Pi, **1** servo y **1** driver [Repo: avance §9]. Cuatro personas no pueden probar a la vez
   sobre el mismo hardware. La sección 5 propone cómo evitar que esto bloquee al equipo.
5. **División propuesta [Recomendación]:** A) Mecánica y calibración física · B) Electrónica, alimentación y
   periféricos de seguridad · C) Firmware del ESP32 (núcleo) · D) Raspberry Pi + panel web real. Antes de
   separarse, el equipo debe **congelar tres contratos** (sección 4): pines, protocolo UART y máquina de
   estados. Sin eso, las piezas no se podrán unir.

---

## 1.1 Cambio de diseño: los dos IR se sustituyen por HC-SR04

**Decisión del equipo (comunicada en conversación; el repo todavía dice FC-51):** los dos sensores infrarrojos
(inspección y salida) se reemplazan por **dos sensores ultrasónicos HC-SR04**. Cada vez que este documento diga
"IR", léase "HC-SR04"; el resto del repo (`conexiones.md`, `ir_test`, avance) **aún no refleja el cambio**.

Lo siguiente es conocimiento general de la hoja de datos típica del HC-SR04 y **debe verificarse con el módulo
físico** (marca/versión del clon que tengan). No es medición del equipo:

| Aspecto | HC-SR04 (típico) | Consecuencia para este proyecto |
|---|---|---|
| Alimentación | 5 V | Salen de la **rama lógica de 5 V** (no de 3V3 como los IR en `conexiones.md §2.1`). Revisar el consumo en el cálculo de §2.4 |
| Pin `Echo` | Nivel de **5 V** | El ESP32 trabaja a 3.3 V: **el `Echo` necesita divisor de voltaje** (p. ej. dos resistencias) o un módulo que entregue 3.3 V. Sin esto se arriesga el GPIO. `conexiones.md §2.2` regla 6 (sin divisor) **deja de aplicar** |
| Pin `Trig` | Entrada: pulso corto (~10 µs) | Es una **salida** del ESP32: cada sensor consume **2 pines** (Trig + Echo) |
| Rango | ~2 cm a ~400 cm (nominal) | Hay **distancia mínima**: la pieza más cerca que eso no se mide bien. Hay que compararla con la distancia de ~2 cm que exige el NFC |
| Ángulo del haz | Cónico (~15° típico) | Puede detectar la estructura, la banda o el otro sensor; la geometría de montaje importa más que con IR |
| Salida | **Distancia** (tiempo de eco), no 0/1 | El firmware debe medir tiempo y fijar un **umbral** en cm; el eco puede fallar con piezas pequeñas, blandas o inclinadas |
| Frecuencia de medición | Se recomiendan decenas de ms entre mediciones | Condiciona qué tan rápido puede reaccionar el ciclo; medir con la velocidad real de la banda (módulo A) |
| Dos sensores | Pueden recibir el eco del otro | Medirlos **alternados**, no a la vez, y orientarlos para que no se apunten entre sí |

**Efectos sobre el plan** (ya reflejados en las secciones siguientes):
- `pruebas/ir_test` queda **obsoleto**; hace falta una prueba nueva `pruebas/ultrasonico_test` (módulo B).
- Pines — **DECIDIDO por el equipo:** `GPIO34/35` = **Echo** (solo entrada; con divisor) y **`GPIO14` = Trig del sensor de
  inspección, `GPIO27` = Trig del sensor de salida** (la asignación de cuál es cuál es la del orden de esta frase;
  confirmar al cablear). Esto libera GPIO14/27 porque **AIN1 y AIN2 del TB6612 se fijan por cableado**:
  AIN1 → 3.3 V y AIN2 → GND (sentido "avanza", según la tabla de control de `conexiones.md §5.5`).
  Consecuencias: (1) la banda **solo avanza hacia adelante, no hay reversa**; (2) el motor se controla únicamente con
  **PWMA (GPIO26)** y **STBY (GPIO25, vía el botón de paro)**; (3) si la banda gira al revés, se **intercambian AO1/AO2**
  en el motor (ya no se puede corregir en código); (4) el firmware no puede liberar un atasco invirtiendo la banda;
  (5) `conexiones.md` ya advierte que **GPIO14 emite un pulso al arrancar**: con un HC-SR04 solo provoca una medición
  espuria, que el firmware debe ignorar durante el arranque. Alternativas descartadas: compartir un `Trig` (se dispararían
  a la vez, interferencia).
- La detección de **atasco** (sensor de inspección ocupado más de N s), de **doble lectura** y de **salida** se redefine
  con umbrales de distancia, no con un nivel lógico.
- Hay que **volver a comprobar** el ajuste mecánico: ¿el sensor de inspección detecta la pieza dentro de su rango útil
  *y* el RC522 queda a ~2 cm? (módulo A).

---

## 2. Qué existe hoy en el repositorio (inventario verificado)

| Elemento | Ubicación | Qué es | Estado según el propio repo |
|---|---|---|---|
| Documento de avance 2 | `docs/Avance2ProyectoFinal_Equipo 07.md` | Arquitectura, componentes, flujo, errores, hardware | Documento de diseño. Su título aún dice "por color" aunque el contenido es NFC |
| Guía de conexiones | `docs/conexiones.md` | Alimentación, mapa de pines, cableado, orden de montaje | "Propuesta, sin probar en hardware" |
| `pruebas/servo_test` | `.ino` | Mueve el SG90 a 0°/90°/180° | Escrito. Sin resultado registrado |
| `pruebas/nfc_test` | `.ino` + `platformio.ini` | Lee UID con RC522 (RST movido a GPIO4) | Escrito. Sin resultado registrado |
| `pruebas/ir_test` | `.ino` (332 líneas) | Diagnóstico de los 2 IR midiendo milivoltios | **Obsoleto** (los IR se cambian por HC-SR04, §1.1) |
| `pruebas/uart_test` | `.ino` + `.py` | PING/PONG ESP32 ↔ Pi | Escrito. Sin resultado registrado |
| `pruebas/integracion_test` | `.ino` (822 l.) + `.py` (255 l.) | RC522 + servo + UART con protocolo con checksum, ACK, reenvío, SYNC, NVS | "Compilado; script de la Pi probado contra un ESP32 simulado; **no probado en hardware**". El simulador usado **no está en el repo** |
| `web/` | HTML/CSS/JS sin dependencias | Panel de 5 pestañas + simulador del ESP32 | Prototipo visual. Servidor, UART real y BD **pendientes** (`web/README.md`) |

Archivo a revisar: `pruebas/nfc_test/.pio/build/project.checksum` está versionado en git; es un artefacto de
compilación de PlatformIO y normalmente no se sube [Recomendación: agregarlo a `.gitignore`].

---

## 3. Qué falta por cubrir

Los pendientes se agrupan por área. Todos se derivan de lo que dicen el avance (§8 "Actividades pendientes"),
`conexiones.md` (§7 "Falta") y los README del repo, contrastado con el código existente.

### 3.1 Verificación de hardware (nada confirmado)

- [ ] Fuente y tierras: medir 4.9–5.2 V en ambas ramas, continuidad de GND, sin corto +5V/GND (`conexiones.md §7` paso 0).
- [ ] **Registrar resultados** de `servo_test`, `nfc_test` y `uart_test` (el equipo dice que ya se corrieron; falta la evidencia: captura, foto o medición). `ir_test` ya no aplica.
- [ ] **Nuevo:** escribir y ejecutar una prueba de los dos **HC-SR04** (distancia estable, umbral, divisor en `Echo`, interferencia entre ambos, distancia mínima real con la pieza).
- [ ] Verificar que el RC522 reporta firmware `0x91/0x92` y lee UID de **7 bytes** con las NTAG213 [Repo: conexiones §7 paso 3].
- [ ] Escribir y ejecutar las pruebas que `conexiones.md` marca como **"Falta"**: OLED (escáner I2C + texto), motor + TB6612 (PWM 30/60/100 %, ambos sentidos), paro de emergencia, LEDs/buzzer/botón Restablecer.
- [ ] Confirmar los datos de `conexiones.md §1`: modelo exacto del ESP32 DevKit (30 pines/CH340), si hay diodo USB–VIN, buzzer **activo o pasivo**, el **HC-SR04** (clon/versión, y si su `Echo` es de 5 V o de 3.3 V), y que el botón de paro sea **NC**.
- [ ] Prueba "todo junto": servo + motor moviéndose sin que el ESP32 se reinicie y con el RC522 leyendo [Repo: conexiones §7 paso 10]. Es la que valida el problema de caída de voltaje del avance.

### 3.2 Firmware del ESP32 (sistema final)

*(Donde la tabla dice "IR", leer "HC-SR04"; ver §1.1.)* El código actual es de **prueba**. Falta, para llegar al comportamiento descrito en el avance §5 y §11:

| Falta | Referencia | Observación |
|---|---|---|
| Leer **IR de inspección** como disparador del ciclo (hoy el disparador es la etiqueta entrando al lector o la tecla `p`) | avance §5 pasos 2 y 11 | El README de integración lo reconoce como limitación |
| Leer **IR de salida** para confirmar el paso (hoy es un tiempo fijo de 2 s, `TRANSITO_MS`) | avance §5 paso 6 y §11 "Servo sin retroalimentación" | Es la única forma indirecta de detectar que la compuerta falló |
| **Control del motor** (TB6612: solo PWMA y STBY; dirección fija por cableado) y detener/reanudar la banda de verdad (hoy solo imprime "Banda detenida") | avance §5 pasos 1–2, 6 | Pines: GPIO26 (PWMA) y GPIO25 (STBY); GPIO14/27 pasan a Trig |
| **Paro de emergencia por hardware + interrupción** (hoy es la tecla `e`) | avance §9, §11; `conexiones.md §5.7` | El diseño NC→STBY es propuesta de `conexiones.md`, **no confirmada con el profesor** |
| **Botón Restablecer** físico (hoy es la tecla `r`) | `conexiones.md §5.8` | GPIO36 |
| **OLED**: conteos, estado, avisos (no hay una sola línea de código OLED en el repo) | avance §5 paso 7 | Falta elegir librería y verificar dirección I2C |
| **LEDs y buzzer** | avance §9 | Falta saber si el buzzer es activo o pasivo |
| **Contenedor lleno** (contador por contenedor, pausa hasta confirmar vaciado) | avance §11 | El panel web ya envía `VACIADO`; el firmware y el protocolo UART real **no lo contemplan** |
| **Watchdog** | avance §11 "Caída de voltaje" | No aparece `wdt`/watchdog en el código |
| **Atasco con IR** (IR de inspección bloqueado > N s; hoy se simula con la etiqueta quieta en el lector 10 s) | avance §11 | Hay que definir N |
| **Piezas muy juntas con IR** (IR de inspección se reactiva durante la clasificación) | avance §11 | Hoy se simula con una segunda etiqueta |
| **Pieza sin etiqueta detectada por IR** (hoy se simula con `p`) | avance §11 | — |
| Comando de OLED/estado "sin supervisor" cuando la Pi no responde | avance §11 "Raspberry Pi apagada" | El enlace ya se marca caído a los 3 s; falta mostrarlo |
| Reemplazar el ciclo de **tiempos fijos** por tiempos **medidos** (`TRANSITO_MS`, `RETORNO_CENTRO_MS`, `ATASCO_MS`) | README de integración, "Limitaciones" | Dependen de la velocidad real de la banda y de las distancias (módulo A) |
| Revisar el conflicto de pines de reserva | `conexiones.md §3` | "Libres de reserva: ninguno cómodo" — cualquier función nueva exige una decisión de pines |

### 3.3 Raspberry Pi y panel web

| Falta | Referencia |
|---|---|
| **Servidor** en la Pi que lea la UART, valide y guarde (el script actual es de consola) | `web/README.md §7` |
| **Base de datos** (SQLite propuesta en `web/README.md §5.3`; hoy el historial es un CSV de prueba) | `web/README.md §5.3` |
| **API y canal en vivo** (propuesta: `/api/estado`, `/api/eventos` por SSE, `/api/comando`) | `web/README.md §7` |
| **Conectar `web/` a datos reales** cambiando la fuente en `main.js` y quitando/aislando `simulador.js` | `web/README.md §4` y §7 paso 3 |
| **Reconciliar el protocolo**: ver §4.2 de este documento (hay dos formatos distintos propuestos) | — |
| **Hora de los eventos**: el ESP32 no tiene RTC; propuesta de enviar `millis()` y que la Pi asigne fecha | `web/README.md §7` paso 4 |
| **Sincronización automática** de la tabla maestra al arrancar | `web/README.md §7` paso 6 (el script de prueba ya hace SYNC por versión) |
| Reinicio del ESP32 vs duplicados (`seq` vuelve a 1 si se borra la NVS) | README de integración, "Limitaciones" |
| Persistencia y arranque automático de la Pi (servicio que sobreviva al reinicio) | [Recomendación]: no está en ningún documento |
| Configuración de UART en la Pi (`raspi-config`, login shell **No**, hardware **Sí**) | `conexiones.md §4` |
| Cómo se accede al panel desde otro dispositivo (red WiFi existente, punto de acceso de la Pi, cable) | [Por confirmar] — el avance menciona "sin conexión WiFi de clientes" pero no define la red |

### 3.4 Mecánica

El avance (§7 problemas 1 y 5, §8 "Montaje mecánico") indica que el NFC obligó a **replantear** el montaje
y que la estructura aún debe construirse. El repo **no contiene ninguna medida**. Falta:

- [ ] Definir dimensiones de banda (largo, ancho), diámetro de poleas, material de la banda, montaje del motor TT.
- [ ] Definir **dónde va la antena del RC522** respecto de la pieza y la etiqueta: el límite documentado es **~2 cm o menos**, con la etiqueta siempre en la misma orientación, y evitando metal detrás de la antena [Repo: avance §7.1, conexiones §5.1].
- [ ] Posición y orientación de los dos **HC-SR04**: que detecten la pieza dentro de su rango útil, **no** la banda ni la estructura, y que no se apunten entre sí (§1.1). Sin potenciómetro de ajuste: el umbral se fija en el firmware. [Reemplaza `conexiones.md §5.3`, que era para FC-51.]
- [ ] Comprobar que la **distancia mínima del HC-SR04** es compatible con colocar la pieza a ~2 cm del RC522 (dato [Por confirmar]: medirlo con el módulo real).
- [ ] Distancia **lector → compuerta** y **compuerta → IR de salida** (de ellas dependen los tiempos del firmware).
- [ ] Compuerta del servo: forma, ángulos reales (los 0°/90°/180° de `servo_test` son provisionales, `conexiones.md §5.4`), y tres contenedores.
- [ ] Medir la **velocidad real** de la banda a distintos PWM.
- [ ] Prevención de atascos y acceso a cableado para mantenimiento.
- [ ] Qué objetos se van a clasificar (tamaño, peso, forma) y dónde se pega la etiqueta [Por confirmar].

### 3.5 Pruebas del sistema, errores y evidencias

Del avance §8:

- [ ] **Calibración NFC:** distintas distancias y ángulos; límite de 1.5 s; etiqueta ausente, ilegible o no registrada.
- [ ] **50 ciclos** con piezas A, B y sin etiqueta, registrando aciertos, rechazos, lecturas fallidas y tiempo de ciclo.
- [ ] **Pruebas de error provocadas:** atasco, etiqueta ausente, interrupción de alimentación, desconexión de la Pi.
- [ ] Los 13 casos de aceptación de `pruebas/integracion_test/README.md` (ya definidos con repeticiones; falta ejecutarlos).
- [ ] **Evidencias:** fotos del montaje, capturas de OLED y panel, registros de lecturas, mensajes UART y resultados.
- [ ] Llenar la tabla de verificación de `conexiones.md §9` solo con evidencia (foto, captura o medición).

### 3.6 Documentación, presupuesto y calendario

- [ ] **Presupuesto:** el avance fija $1,000 MXN como objetivo; el costo estimado y los precios reales **no están en el repo** y el propio avance pide confirmarlos. [Por confirmar]
- [ ] **Calendario:** el avance pide actualizar fechas; el repo no contiene el cronograma. [Por confirmar]
- [ ] **Inconsistencia a corregir en la entrega final:** el título del avance 2 dice "por color"; el contenido es por NFC. El avance y `conexiones.md` describen sensores **IR (FC-51)**, pero el equipo cambió a **HC-SR04**: hay que actualizar la lista de hardware, el diagrama de bloques, el título/contenido del avance. **Ya actualizados:** `conexiones.md` (§1, §2, §3, §5.3, §5.5, §6, §7, §8, §9), `pruebas/README.md`, `pruebas/integracion_test/README.md`, `web/README.md` y una nota al inicio del avance (sin reescribir el texto entregado). Las notas sobre "IR a 3.3 V con o sin divisor" ya no aplican; ahora el divisor es necesario en `Echo` (§1.1).
- [ ] Reporte final: manual de conexiones, explicación del código, protocolos de prueba, resultados y conclusiones (sin afirmar funciones no verificadas).
- [ ] Preparación para preguntas técnicas del profesor (cada integrante debe poder explicar su módulo **y** la interfaz con los demás).

---

## 4. Contratos que hay que congelar ANTES de dividir el trabajo

Esto es lo que permite que cuatro personas trabajen por separado y después unan. Si un contrato cambia, **se avisa a todos**
y se actualiza el archivo correspondiente en el mismo commit.

### 4.1 Contrato de pines [Repo: `docs/conexiones.md §3`]

Es el único mapa de pines del repo y está marcado como "propuesto". Antes de dividir, el equipo debe aceptarlo o corregirlo.
Resumen (no sustituye la tabla original):

| Función | GPIO | Función | GPIO |
|---|---|---|---|
| RC522 SS / SCK / MOSI / MISO / RST | 5 / 18 / 23 / 19 / **4** | TB6612 PWMA / AIN1 / AIN2 | 26 / 14 / 27 |
| OLED SDA / SCL | 21 / 22 | Paro: salida a STBY / lectura | 25 / 39 |
| HC-SR04 **Echo** inspección / salida (con divisor) | 34 / 35 |
| HC-SR04 **Trig** inspección / salida | **14 / 27** (decidido, §1.1) | Botón Restablecer | 36 |
| Servo | 13 | LED verde / amarillo / rojo | 32 / 33 / 2 |
| UART2 TX / RX | 17 / 16 | Buzzer (vía transistor) | 15 |

**Regla [Recomendación]:** solo el responsable del módulo B modifica este contrato; los demás piden el cambio.

### 4.2 Contrato del protocolo UART — hay una discrepancia que resolver

Hay **dos formatos distintos** documentados:

| | Formato A — implementado | Formato B — propuesto en `web/README.md §7` |
|---|---|---|
| Ejemplo | `PIEZA,42,04A31FB26C8000,A,OK,4630,0*HH` | `$PZ,42,04A31FB26C8000,A,-,4630*CS` |
| Checksum | XOR hexadecimal del cuerpo | XOR entre `$` y `*` (estilo NMEA) |
| Dónde existe | `esp32_integracion_test.ino` y `rpi_integracion_test.py`, descrito en `pruebas/integracion_test/README.md` | Solo como texto en el README del panel |

**[Recomendación]:** adoptar el **Formato A**, porque ya está escrito en ambos extremos y el lado de la Pi está probado
contra un ESP32 simulado; el Formato B no tiene código. Es una recomendación, no una decisión tomada.

Tramas del Formato A [Repo: `pruebas/integracion_test/README.md`]:

| Sentido | Trama |
|---|---|
| ESP32 → Pi | `PIEZA,seq,uid,cat,motivo,ciclo_ms,reenvio` · `ESTADO,nombre` · `LATIDO,pendientes,version_tabla` · `ALERTA,codigo` |
| Pi → ESP32 | `ACK,seq` · `VIVO,version` · `SYNC,version,uid=cat;uid=cat` · `REENVIAR,seq` |

**Huecos del contrato que hay que decidir juntos** (no están definidos en el código):

1. **`VACIADO`** (Pi → ESP32): el panel web lo emite (`web/js/estado.js`) pero no existe en el protocolo implementado.
2. **Alerta de contenedor lleno** (ESP32 → Pi): no está en la lista de códigos de alerta implementados (`REINICIO, ATASCO, PIEZAS_JUNTAS, EMERGENCIA, RESTABLECIDO, NFC_FALLA, PENDIENTES_PERDIDOS`). El panel sí maneja "Contenedor A lleno" en la simulación.
3. **Códigos de alerta para IR**, p. ej. pieza que no llega al IR de salida (el avance §11 pide error si no se activa a tiempo; el firmware de prueba quitó `SIN_SALIDA`).
4. **Nombres de estados:** el firmware usa `ESPERA, LECTURA_NFC, CLASIFICANDO, LIBERA_LECTOR, CONFIRMA_SALIDA, RETORNO, ERROR, EMERGENCIA`; el panel (`web/README.md §3 y §5.1`) usa `ESPERA, TRANSPORTE, LECTURA_NFC, CLASIFICANDO, CONFIRMA_SALIDA, PAUSA, PAUSA_CONTENEDOR, ERROR, EMERGENCIA`. Hay que unificar la lista.
5. **Marca de tiempo:** el panel espera un `ts`; el ESP32 no tiene reloj (propuesta: la Pi asigna la hora al recibir; para reenvíos, estimarla con `millis()`).
6. **Reinicio de `seq`:** definir cómo distingue la Pi un reinicio legítimo de duplicados (`web/README.md §7`, "Mejoras de funcionalidad").

Entregable del contrato: un archivo `docs/protocolo-uart.md` con **todas** las tramas, campos, valores válidos y ejemplos. [Recomendación]

### 4.3 Contrato de la máquina de estados

Un diagrama único (estados, transiciones, quién las dispara, qué actuadores cambian) en `docs/maquina-de-estados.md`.
Debe responder, como mínimo, estas preguntas que hoy el repo **no resuelve**:

- ¿Qué dispara el ciclo: el IR de inspección (avance) o el lector? En el sistema final es el IR [Repo: avance §5].
- ¿Cuántos segundos es "N" del atasco? ¿Cuánto tarda la compuerta? ¿Cuánto tiempo de tránsito esperar al IR de salida? (dependen del módulo A).
- ¿Qué pasa con una pieza **ya clasificada** cuando llega la emergencia? (la prueba actual decide que no se registra; confirmar si es lo deseado).
- ¿Qué LED/buzzer/OLED corresponde a cada estado?

---

## 5. División propuesta en 4 módulos [Recomendación]

La asignación de **personas** a módulos no se propone aquí: el repo no dice quién domina qué. Se llama A, B, C, D;
el equipo decide quién toma cada una.

> **Balance de carga (dicho con claridad):** C y D son los módulos con más trabajo de software; A y B tienen
> mucho trabajo físico y dependen de que lleguen componentes. Si el equipo prefiere otro reparto, lo importante
> es **conservar las fronteras** (las interfaces de §4).

### Cómo se evita el cuello de botella del hardware único

El avance lista una sola unidad de cada módulo clave. **[Por confirmar]** si algún integrante tiene otro ESP32,
RC522, servo o driver de uso personal; eso cambia mucho el plan. Mientras tanto:

- **A (mecánica)** casi no necesita electrónica: puede avanzar con el motor, los IR y el servo ya listos, o con una maqueta, sin ocupar el ESP32 por periodos largos.
- **D (Pi/web)** puede trabajar **sin ESP32**: con un emulador del ESP32 en Python que hable el protocolo (el que se usó para probar la Pi no está en el repo; habría que recrearlo y versionarlo) o con el simulador del propio panel web.
- **B (electrónica)** y **C (firmware)** son los que compiten por la placa. [Recomendación] Turnos explícitos: B usa la placa para sus verificaciones eléctricas; C escribe y compila sin la placa y reserva ventanas de prueba. Si es posible, comprar/prestar un segundo ESP32 (precio por verificar).

### Módulo A — Mecánica y calibración física

**Responsabilidad:** que exista una banda que mueva piezas de forma estable y que el NFC pueda leer en ella.

Tareas:
1. Diseñar y construir banda, poleas, soporte del motor TT, soportes de IR y del RC522, compuerta del servo y los tres contenedores.
2. **Experimento de alcance NFC:** con `nfc_test` y las etiquetas reales, medir lectura vs. distancia/ángulo/material; fijar la distancia final (el avance documenta ~2 cm como límite).
3. Montar y orientar los dos **HC-SR04** y determinar con B el umbral de distancia que distingue "pieza presente" de "banda libre" (junto con la prueba nueva de B; `ir_test` ya no aplica).
4. Medir velocidad de banda vs. PWM y las distancias lector→compuerta y compuerta→IR de salida, y **entregar una tabla de tiempos** a C.
5. Medir los ángulos reales del servo para A, B y centro.
6. Reunir el **presupuesto con precios reales** y el calendario actualizado (avance §8).
7. Fotografías y registro de evidencias mecánicas.

Entregables:
- Planos/medidas y fotos del montaje.
- `docs/mecanica.md` con distancia NFC final, distancias lector–compuerta–salida, velocidad por PWM, ángulos del servo.
- Presupuesto y calendario.

Criterios de aceptación (propuestos):
- La pieza con etiqueta se **lee en el 100 % de las pasadas de calibración** a la distancia fijada (definir con el equipo el número de pasadas; el avance no lo fija).
- Los HC-SR04 detectan la pieza con un umbral estable y **no** detectan banda ni estructura; el sensor de inspección no capta el eco del de salida (ni al revés).
- La compuerta lleva la pieza a cada contenedor sin atasco en las pruebas de calibración.

Depende de: nada para empezar. Entrega a: C (tiempos), B (posición de sensores), D (nada directo).

### Módulo B — Electrónica, alimentación y periféricos de seguridad

**Responsabilidad:** que el hardware sea eléctricamente confiable y que cada periférico tenga un **driver probado** que C pueda llamar.

Tareas:
1. Alimentación según `conexiones.md §2`: dos ramas desde la bornera, tierra común, capacitor de 1000 µF cerca del servo, 100 nF en el motor. Ejecutar el paso 0 (multímetro).
2. Confirmar los datos de `conexiones.md §1` (modelo de ESP32, diodo USB, buzzer, FC-51, botón NC).
3. Escribir y probar los **drivers + sketches de prueba** que faltan, uno por periférico:
   - Motor TB6612 (solo **PWMA + STBY**; AIN1/AIN2 fijos por cableado, sin reversa; §1.1): `motor_iniciar(pwm)`, `motor_detener()`. Ya no hay `freno()` ni cambio de sentido por software.
   - **Paro de emergencia**: circuito NC→STBY y lectura en GPIO39 con interrupción; probar que el motor se detiene **con el programa bloqueado** (criterio de `conexiones.md §7` paso 7).
   - **HC-SR04 ×2** (reemplaza a los IR): pulso en `Trig`, medición del eco, filtrado (p. ej. varias lecturas) y umbral en cm; mediciones **alternadas** entre sensores; manejo de "sin eco". Incluye el **divisor de voltaje en `Echo`** y alimentación desde la rama de 5 V. Resolver la asignación de pines de `Trig` (§1.1).
   - LEDs, buzzer, botón Restablecer.
   - OLED (escáner I2C → texto → pantalla de estado y conteos).
4. Prueba "todo junto" (servo + motor + RC522) vigilando reinicios; si falla, medir el voltaje en VIN al arrancar el motor.
5. Mantener el contrato de pines (§4.1) y la tabla de verificación `conexiones.md §9` **con evidencia**.
6. Documentar la limitación ya reconocida: el paro **no corta el servo por hardware**, solo el motor (`conexiones.md §5.7`).

Entregables:
- Un archivo por periférico en una carpeta nueva (p. ej. `firmware/drivers/`) con API documentada [Recomendación: ver §6] y su sketch de prueba en `pruebas/`.
- Diagrama de conexiones final y fotografías del cableado.
- Tabla de verificación de `conexiones.md §9` llena.

Criterios de aceptación (tomados de `conexiones.md §7`):
- 4.9–5.2 V en ambas ramas; sin corto +5V/GND.
- Motor cambia de velocidad en ambos sentidos sin reiniciar el ESP32.
- **Al presionar el paro, el motor se detiene aunque el programa no haga nada** y GPIO39 cambia a BAJO.
- OLED muestra el texto en `0x3C`/`0x3D`.
- HC-SR04 (criterio nuevo, propuesto): la distancia con la banda libre es estable (variación máxima a definir por el equipo), cambia de forma clara al pasar la pieza real, y el `Echo` llega al ESP32 **a ≤3.3 V** (medido con multímetro).
- Servo + motor simultáneos: el ESP32 no se reinicia y el RC522 sigue leyendo.

Depende de: componentes físicos y algo de ESP32. Entrega a: C (drivers y API), A (restricciones de montaje eléctrico).

### Módulo C — Firmware del ESP32 (núcleo)

**Responsabilidad:** la máquina de estados completa y la lógica de clasificación autónoma.

Tareas:
1. Partir de `pruebas/integracion_test/esp32_integracion_test/` (ya tiene RC522, servo, tabla local, NVS, protocolo con ACK/reenvío/SYNC/latido). Separarlo en módulos de código (tabla/NVS, protocolo, NFC, servo, máquina de estados) en lugar de un solo `.ino` de 822 líneas.
2. Reemplazar las simulaciones por entradas reales (cuando B las entregue): disparo por HC-SR04 de inspección, confirmación por HC-SR04 de salida (con umbral y filtrado de B), motor real, botón de paro por interrupción, Restablecer físico.
3. Implementar los casos de error de avance §11 que hoy se simulan o no existen: atasco por IR, piezas juntas, sin salida (servo sin retroalimentación), contenedor lleno + `VACIADO`, watchdog, aviso "sin supervisor".
4. Sustituir `TRANSITO_MS`, `ATASCO_MS`, `RETORNO_CENTRO_MS` por los valores **medidos** que entrega A.
5. Integrar la salida de OLED/LEDs/buzzer con los drivers de B.
6. Mantener el protocolo UART como lo fije `docs/protocolo-uart.md` y la lista de estados de `docs/maquina-de-estados.md`.
7. Ejecutar los 13 casos de `integracion_test/README.md` y adaptarlos al sistema final.

Entregables:
- Firmware modular compilable (indicar herramienta: los README actuales usan Arduino IDE/`arduino-cli`; los `platformio.ini` apuntan a `/dev/ttyUSB0`; **elegir una sola** [Por confirmar]).
- `docs/maquina-de-estados.md` y explicación del código para el reporte.

Criterios de aceptación (propuestos, derivados de los casos 1–13 ya redactados):
- Casos de clasificar A/B/rechazo correctos en el 100 % de las repeticiones definidas.
- Con la Pi desconectada, sigue clasificando y reenvía los pendientes al volver (sin huecos de `seq`).
- Sobrevive a un reinicio conservando conteos y `seq` (NVS).
- 20 piezas seguidas sin reinicios y con 0 tramas inválidas.

Depende de: B (drivers), A (tiempos), contrato de protocolo. Puede avanzar de inmediato en: máquina de estados, tabla/NVS, protocolo (todo sobre el código existente).

### Módulo D — Raspberry Pi y panel web real

**Responsabilidad:** convertir el prototipo visual en el sistema supervisor.

Tareas:
1. Preparar la Pi 3 B: SO, UART habilitado (`conexiones.md §4`), entorno Python, arranque automático del servicio. Confirmar que la Pi funciona y la microSD está en buen estado (el propio avance condiciona su disponibilidad a confirmación).
2. Servidor: hilo lector de UART con validación de checksum, ACK, detección de duplicados/huecos, `REENVIAR`, `SYNC` — **reutilizando** la lógica de `rpi_integracion_test.py`.
3. Persistencia (SQLite, propuesta de `web/README.md §5.3`): tablas `etiquetas`, `piezas`, `alertas`, `contenedores`.
4. API y canal en vivo para el panel (propuesta de `web/README.md §7`; si el equipo prefiere otra tecnología, justificarlo).
5. Conectar `web/` a datos reales (cambiar la fuente en `main.js`) y dejar el simulador solo como modo demostración.
6. Funciones del panel que dependen del protocolo: registro/edición de etiquetas, sincronización con ESP32, `VACIADO`, historial con exportación CSV (para las 50 pruebas), reconocimiento de alertas.
7. Emulador del ESP32 en Python (para trabajar sin la placa) y pruebas automáticas del servidor.
8. Resolver cómo se accede al panel (red) — [Por confirmar].

Entregables:
- Servidor en el repo (carpeta nueva, p. ej. `rpi/`), base de datos con esquema documentado y panel conectado.
- Instrucciones de instalación reproducibles en una Pi limpia.

Criterios de aceptación (propuestos):
- Un evento `PIEZA` enviado por el ESP32 (real o emulado) aparece en el panel y en la base de datos **una sola vez**, aunque se reenvíe.
- Tras apagar y encender la Pi, el historial sigue ahí y el servicio arranca solo.
- Registrar una etiqueta y sincronizar hace que el ESP32 (o su emulador) la clasifique con la nueva categoría.
- Con la Pi desconectada 30 s y reconectada, no faltan números de secuencia en el historial (equivale al caso 10 de integración).

Depende de: contrato de protocolo. Puede avanzar de inmediato sin hardware (emulador). Entrega a: C (protocolo estable y emulador sirven de contrapartida para las pruebas).

### Responsabilidades compartidas

| Tema | Quién |
|---|---|
| Pruebas de sistema (50 ciclos) y pruebas de error | Todos, en sesión conjunta con el hardware armado |
| Reporte final | Cada responsable redacta su módulo; un integrante lo une y revisa consistencia |
| Evidencias (fotos, capturas, logs) | Cada responsable de lo que produce |
| Preparación del profesor | Cada uno defiende su módulo y la interfaz con los vecinos |

---

## 6. Cómo trabajar por separado y después unir [Recomendación]

1. **Una carpeta por módulo**, sin pisarse:
   - `firmware/` (C; `firmware/drivers/` de B), `rpi/` (D), `web/` (D, ya existe), `mecanica/` (A), `docs/` (todos), `pruebas/` (cada quien su sketch de prueba).
2. **Ramas de git por módulo** (`mod-a-mecanica`, `mod-b-electronica`, `mod-c-firmware`, `mod-d-rpi-web`) y integración a `main` por revisión de al menos otro integrante. La rama de trabajo actual del repo es `zeus-master`; el equipo debe decidir cómo se relaciona con estas.
3. **Interfaz de los drivers de B** acordada **antes** de escribirlos. Ejemplo ilustrativo (los nombres exactos los definen B y C):
   ```
   motor_iniciar(pwm_0_100)   motor_detener()
   pieza_en_inspeccion()      pieza_en_salida()      (HC-SR04; devuelven bool, usan umbral)
   paro_activo()              restablecer_presionado()
   led_set(color, on)         buzzer(patron)
   oled_mostrar(estado, A, B, R, mensaje)
   ```
4. **Cada módulo se entrega con**: código, sketch/script de prueba aislado, criterios de aceptación **ejecutados** con evidencia, y una sección en su `.md`. Nada se marca "terminado" sin esa evidencia (misma regla de `conexiones.md §9`).
5. **Hitos de integración por pares, en este orden:**
   1. **Hito 0 — Contratos congelados** (§4): pines, protocolo UART, máquina de estados.
   2. **Hito 1 — Módulos aislados**: cada uno pasa sus criterios por separado.
   3. **Hito 2a — B + C**: drivers dentro de la máquina de estados (motor, IR, paro, OLED, LEDs).
   4. **Hito 2b — C + D**: UART real entre ESP32 y Pi con el protocolo final; pruebas de desconexión.
   5. **Hito 2c — A + B + C**: NFC sobre la banda real, con tiempos medidos.
   6. **Hito 3 — Sistema completo**: 50 ciclos + pruebas de error + evidencias.
   7. **Hito 4 — Reporte y ensayo de la defensa.**
6. **Sesión de integración presencial** en cada hito 2 y 3: es donde se necesita el hardware único y a varias personas juntas.

---

## 7. Mapa de dependencias

```
Contratos (pines · UART · estados)  ──────────────────────────────────────────┐
        │                                                                      │
        ├──► A Mecánica ──── tiempos, distancias, ángulos ───────► C Firmware ◄─┤
        │         │                                                  ▲   │      │
        ├──► B Electrónica ─── drivers + sketches ───────────────────┘   │      │
        │                                                                │ UART │
        └──► D Pi + web (con emulador, sin hardware) ◄───────────────────┘      │
                                                                                │
                      Hito 3: sistema completo ◄────────────────────────────────┘
```

Lo que **puede empezar hoy, sin esperar a nadie**: A (calibración NFC e IR, medidas), B (alimentación y pruebas
de periféricos), C (modularizar el código existente y definir estados), D (servidor con emulador).
Lo que **no** puede cerrarse hasta unir: tiempos del firmware (A→C), pruebas de caída de voltaje con todo
conectado (B+C), contenedor lleno de extremo a extremo (C+D).

---

## 8. Preguntas abiertas para el equipo (no se asumió ninguna respuesta)

1. **Fecha de la entrega final** y fechas intermedias. (El repo solo menciona la entrega parcial 2 del 27 de octubre de 2026.)
2. **¿Qué componentes ya están comprados/en mano?** El repo no lo registra. En particular: ¿ya tienen motor TT, driver, OLED, IR, servo, botón de paro, fuente de 3 A?
3. ¿Existe un **segundo ESP32** (o RC522/servo) para probar en paralelo?
4. **Presupuesto real:** ¿costo actual y desviación respecto de los $1,000 MXN? ¿Dónde está el desglose?
5. **Cronograma:** ¿dónde está el archivo? ¿Qué hitos exige el profesor?
6. ¿La Raspberry Pi 3 B funciona y tiene microSD en buen estado? ¿Qué SO tiene?
7. **Botón de paro:** ¿es NC? ¿El profesor acepta el diseño de paro por hardware de `conexiones.md §5.7`?
8. **Buzzer:** ¿activo o pasivo? **Modelo exacto del ESP32**. Los sensores ya son **HC-SR04** (confirmado): ¿el módulo trae `Echo` a 5 V (estándar) o a 3.3 V?
9. **Objetos a clasificar:** tamaño, peso, forma, material y dónde se pega la etiqueta. Con 7 etiquetas NTAG213 en la lista, ¿cuántos objetos físicos distintos se usarán en la demostración?
10. **¿Cómo se accede al panel web** (red de la universidad, punto de acceso de la Pi, cable)?
11. **Herramienta de firmware:** Arduino IDE / `arduino-cli` / PlatformIO (hoy conviven; los `platformio.ini` fijan `/dev/ttyUSB0`, que puede no coincidir en otras computadoras o sistemas).
12. ¿Se acepta el **Formato A** del protocolo (§4.2)?
13. ¿Los resultados de `servo_test`, `nfc_test`, `ir_test` y `uart_test` ya se corrieron en físico? Si sí, hay que **registrarlos** (el repo no los tiene) y marcar la tabla de `conexiones.md §9`.
14. ¿La capacidad de los contenedores (la simulación usa 15 piezas, un valor **inventado** según `web/README.md §6`) tiene un valor real definido por el equipo?
15. ~~Pines de `Trig`~~ **Resuelto:** GPIO14 y GPIO27; AIN1/AIN2 fijos (§1.1). El equipo confirmó que **no necesita reversa** de banda.
16. ¿Ya compraron/tienen los dos HC-SR04 y las resistencias del divisor? ¿Qué distancia mínima de detección midieron (si ya los probaron)?

---

## 9. Riesgos principales (ya mencionados en el repo, con su módulo responsable)

| Riesgo | Fuente | Módulo | Mitigación documentada |
|---|---|---|---|
| Lectura NFC falla por distancia/orientación | avance §7.1 | A | Distancia ~2 cm, orientación fija, sin metal detrás de la antena; experimento de alcance |
| Caída de voltaje reinicia el ESP32 | avance §7.2, `conexiones.md §2` | B | Dos ramas desde la bornera, capacitor, tierra común; prueba "todo junto" |
| Lecturas duplicadas / piezas juntas | avance §7.3 | C | Esperar a que el IR se libere, tiempos límite, estado de error |
| Falta de definición de tiempos de error | avance §7.4 | A + C | Medir velocidad y distancias (A) y parametrizar el firmware (C) |
| Servo sin retroalimentación | avance §11 | B + C | Confirmación indirecta con el IR de salida; reconocerlo como limitación |
| El paro no corta el servo por hardware | `conexiones.md §5.7` | B | Declarar la limitación (riesgo bajo con SG90) o botón de dos contactos/relé |
| Pérdida de datos por reinicio / NVS borrada | README de integración | C + D | `--reiniciar` en la Pi; definir manejo de reinicio de `seq` |
| Un solo juego de hardware para 4 personas | avance §9 | Todos | Emulador (D), turnos (B/C), segundo ESP32 si es viable |
| HC-SR04: `Echo` a 5 V dañaría el GPIO; zona ciega vs. 2 cm del NFC; eco perdido con piezas pequeñas/inclinadas; interferencia entre los dos sensores | §1.1 (hoja de datos típica; **sin verificar**) | A + B | Divisor en `Echo`, medir distancia mínima real, mediciones alternadas, umbral y filtrado en firmware |
| Banda sin reversa (AIN1/AIN2 fijos) y GPIO14 con pulso en el arranque | §1.1, `conexiones.md §3` | B + C | Verificar el sentido al cablear (intercambiar AO1/AO2 si hace falta); ignorar la medición de arranque; atasco se resuelve sin reversa |
| Protocolo con dos formatos distintos | `web/README.md` vs. integración | C + D | Congelar Formato A (§4.2) |

---

## 10. Siguiente paso concreto [Recomendación]

1. Reunión corta para contestar las preguntas de la sección 8 (en especial 1, 2, 3, 7, 9 y 12).
2. Congelar los tres contratos (§4) y crear `docs/protocolo-uart.md` y `docs/maquina-de-estados.md`.
3. Repartir A, B, C, D y crear las ramas/carpetas de §6.
4. Cada responsable ejecuta **primero** las pruebas que ya existen para su módulo y **registra el resultado** (foto, captura o medición) — hoy ninguna está registrada.
