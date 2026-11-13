# Plan de trabajo final — entrega el martes 24 de noviembre de 2026

> **Para qué sirve este documento.** Dice qué hacer cada día hasta la entrega, quién lo hace y cómo sabremos que
> quedó bien. Reemplaza el calendario del plan anterior ([plan-de-trabajo-equipo.md](plan-de-trabajo-equipo.md)),
> que se queda como referencia técnica.
>
> **Etiquetas:** **[Dato]** lo dijo el equipo o está en el repositorio. **[Suposición]** lo supuse yo y hay que
> confirmarlo. **[Por confirmar]** falta información.

---

## 1. Punto de partida (hoy, jueves 12 de noviembre)

Explicado sin tecnicismos:

- **El "cerebro" ya está escrito.** [Dato] El programa del ESP32, el servidor de la Raspberry Pi y el panel web existen y funcionan **en la computadora** con pruebas simuladas.
- **Nada se ha probado todavía con las piezas reales juntas.** [Dato] Es lo que falta: conectar, medir, ajustar y comprobar.
- **La banda se compra hecha y llega el martes 17.** [Dato] Incluye estructura, banda y motor amarillo. Esto **ahorra** semanas de construcción, pero llega justo el día de la sesión de armado: **no hay margen** si se retrasa un día. Llega a las 9:00, antes de la sesión. [Dato]
- **Quedan 12 días.** Los cuatro solo coincidimos martes, miércoles y jueves de 11:00 a 13:00. [Dato] Fuera de ese horario solo pueden juntarse 2.

> **Lo más importante del plan:** el tiempo juntos es muy poco (unas 8 horas en total), así que lo usamos **solo** para lo que requiere a los cuatro y la banda. Todo lo demás se hace en parejas o por separado.

---

## 2. Calendario

| Día | Quiénes | Qué se hace | Tipo |
|---|---|---|---|
| **Jue 12** (hoy) | Los 4, 11–13 (si aún se puede) | Confirmar la compra de la banda y su fecha de llegada; repartir A–D; revisar qué componentes ya tenemos | Grupal |
| **Vie 13 – Lun 16** | Parejas o solos | Preparar todo lo que no necesita la banda (ver §4, Fase 1) | Parejas |
| **Mar 17** | Los 4, 11–13 | **Llega la banda.** Recibirla, revisar el motor y empezar el armado | Grupal |
| **Mié 18** | Los 4, 11–13 | **Montaje y ajuste:** todo sobre la banda, primeras pasadas con piezas | Grupal |
| **Jue 19** | Los 4, 11–13 | **Sistema completo:** ESP32 + Raspberry Pi + panel con la banda real | Grupal |
| **Vie 20 – Dom 22** | Parejas | Pruebas repetidas (las 50 pasadas), arreglos, fotos, reporte | Parejas |
| **Lun 23** | Parejas o todos | **Congelar:** no se cambia nada más. Ensayo de la demostración | Parejas |
| **Mar 24** | Los 4 | **Entrega** [Por confirmar: ¿es demostración, reporte o ambos? ¿a qué hora?] | Grupal |

**Regla de oro:** el lunes 23 se congela el sistema. Cualquier cosa que no funcione ese día se documenta como limitación, no se arregla de última hora.

---

## 3. Quién hace qué

Se mantienen los cuatro módulos del plan anterior; solo cambia el módulo A porque la banda se compra.

| Módulo                                       | Responsabilidad                                                                          | En palabras sencillas                                   |
| -------------------------------------------- | ---------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| **A — Montaje y medidas** (antes "Mecánica") | Instalar sensores, lector NFC, compuerta y contenedores sobre la banda comprada, y medir | "Que cada pieza quede en el lugar correcto de la banda" |
| **B — Electrónica**                          | Cables, alimentación y pruebas de cada componente                                        | "Que todo se encienda sin quemarse ni reiniciarse"      |
| **C — Programa del ESP32**                   | Cargar el programa y ajustar sus tiempos con las medidas reales                          | "Que la banda decida y clasifique bien"                 |
| **D — Raspberry Pi y panel**                 | Poner la Pi a funcionar, conectarla al ESP32 y probar el panel                           | "Que los resultados se vean y se guarden"               |

*Quién toma cada letra lo decide el equipo hoy.* [Por confirmar]

### Parejas sugeridas para los días sin el grupo completo

| Pareja    | Cuándo sirve           | Para qué                                                                  |
| --------- | ---------------------- | ------------------------------------------------------------------------- |
| **B + C** | Casi siempre           | Probar el programa sobre el cableado real (comparten el único ESP32)      |
| **A + B** | Cuando llegue la banda | Fijar sensores y cables sobre la estructura                               |
| **C + D** | Jue 19 en adelante     | Conectar el ESP32 con la Raspberry Pi y probar fallas                     |
| **A + D** | Fines de semana        | Pruebas repetidas, fotos y reporte (no necesitan el ESP32 todo el tiempo) |

---

## 4. Las fases

### Fase 1 — Prepararse sin la banda (jue 12 – lun 16)

**Idea:** adelantar todo lo que no depende de que llegue la banda.

| Módulo | Tareas                                                                                                                                                                                                                                                                                                                 | ¿Cómo sabemos que quedó?                                                   |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| **A**  | Pedir al vendedor las **medidas de la banda** y la **ficha del motor amarillo** (voltaje y corriente). Con eso, dibujar dónde irá el lector NFC (debe quedar a **2 cm o menos** de la etiqueta). Dejar listos soportes, la compuerta del servo y los tres contenedores (cartón o impresión) para instalarlos el martes | Hay medidas, ficha del motor y un dibujo simple de dónde va cada pieza     |
| **B**  | Cablear sobre mesa (protoboard) y probar una por una: fuente, servo, lector NFC, pantalla, sensores de distancia, motor, paro de emergencia, luces y buzzer. **Medir con multímetro el divisor de voltaje** de los sensores antes de conectarlos                                                                       | Tabla de `docs/conexiones.md` §9 con foto o medición de cada prueba        |
| **C**  | Esperar el hardware de B; mientras tanto, repasar los valores provisionales de `firmware/banda_clasificadora/config.h` y preparar las pruebas de error                                                                                                                                                                 | Lista de qué valores hay que medir                                         |
| **D**  | Instalar el sistema de la Raspberry Pi, activar su puerto serie y arrancar `rpi/servidor.py` con el ESP32 simulado                                                                                                                                                                                                     | El panel muestra piezas del simulador en un navegador real y en un celular |

**Nota para B y C:** hay un solo ESP32. Acuerden turnos: B lo usa primero para cablear y probar; C solo
necesita la placa cuando B le avise que un componente ya pasó su prueba.

### Fase 2 — Llega la banda (martes 17)

**Idea:** como las piezas ya se probaron sobre la mesa en la Fase 1, el martes se usa para **recibir la banda,
conectarle el motor y ver que todo siga funcionando junto**.

> **A las 9:00 llega la banda:** alguien (idealmente A) debe estar para recibirla, revisarla y tener el motor
> conectado al driver antes de las 11:00, para que la sesión grupal empiece con la banda lista.
>
> **Antes de las 11:00:** B debe tener ya probadas las piezas individuales. Si no, el martes se pierde
> probando cosas sueltas en vez de armar.

- B termina las pruebas pendientes y completa la tabla de evidencias.
- A recibe la banda, revisa que no venga dañada y que el motor coincida con la ficha. Con B lo conectan al driver y miden **qué tan rápido avanza** a distintas potencias.
- C carga por primera vez `firmware/banda_clasificadora/` con todo conectado y revisa que no se reinicie al mover motor y servo a la vez.
- D confirma que la Pi recibe datos reales del ESP32 (aunque sea una sola pieza).

**Criterio de éxito del día:** el ESP32 mueve el motor y el servo, lee una etiqueta y la Pi la recibe, sin
reinicios inesperados.

### Fase 3 — Montaje y ajuste sobre la banda (miércoles 18)

**Idea:** pasar de "funciona en la mesa" a "funciona sobre la banda".

1. A y B fijan lector NFC, los dos sensores de distancia, la compuerta y los contenedores en su posición final.
2. A mide las distancias (lector → compuerta → salida) y el tamaño de las piezas; pasa los números a C.
3. C actualiza `config.h` con esos números (tiempos, velocidad, ángulos del servo, umbrales de los sensores).
4. Se hacen **10 pasadas** de prueba con piezas A, B y una sin etiqueta.

**Criterio de éxito del día:** al menos 8 de 10 piezas llegan al contenedor correcto. Lo que falle se anota para
ajustar el jueves.

> **Confirmar hoy mismo:** el programa supone que **todas las piezas pasan por el sensor de salida antes de la
> compuerta** (`docs/maquina-de-estados.md` §4). Si en la banda comprada el sensor tiene que ir en otro lugar,
> C debe cambiar el código el miércoles, no después.

### Fase 4 — Sistema completo (jueves 19)

**Idea:** probar todo junto, incluyendo lo que sale mal a propósito.

- Pruebas de falla (una vez cada una): pieza sin etiqueta, etiqueta no registrada, pieza atorada, dos piezas muy juntas, contenedor lleno, botón de emergencia, desconectar la Raspberry Pi y reconectarla, quitar la corriente y volver a encender.
- Registrar las etiquetas reales desde el panel y comprobar que el ESP32 las usa.
- Primera tanda larga: **20 pasadas seguidas** sin intervenir.

**Criterio de éxito del día:** la banda clasifica correctamente 18 de 20 y todas las fallas se detectan y se muestran en pantalla y panel.

### Fase 5 — Pruebas finales y reporte (vie 20 – lun 23)

| Tarea                                                                       | Quién            | Producto                          |
| --------------------------------------------------------------------------- | ---------------- | --------------------------------- |
| **50 pasadas** con piezas A, B y sin etiqueta; exportar el CSV del panel    | B + C o A + D    | Archivo CSV y tabla de resultados |
| Arreglar lo que falló el jueves (solo si es sencillo)                       | C                | Cambios en `config.h` o código    |
| Tomar fotos y capturas (montaje, pantalla, panel)                           | A                | Carpeta de evidencias             |
| Escribir el reporte: cada quien su módulo                                   | Cada responsable | Secciones del reporte             |
| Unir y revisar el reporte; actualizar el presupuesto real                   | Un integrante    | Reporte final                     |
| Corregir el avance 2: dice "por color" y "IR", ahora es NFC y HC-SR04       | D                | Documento al día                  |
| **Lunes 23:** ensayo completo de la demostración, cada uno explica su parte | Todos            | Demostración ensayada             |

---

## 5. Qué es indispensable y qué es opcional

Con tan poco tiempo, definimos de antemano qué se sacrifica si algo se atrasa.

| Prioridad | Qué | Si se atrasa… |
|---|---|---|
| **Indispensable** | Banda avanza, lee etiqueta, la compuerta manda la pieza al contenedor correcto, pantalla muestra el estado, botón de emergencia detiene el motor | No se negocia: todo el tiempo se dirige aquí |
| **Muy importante** | Panel web con resultados reales y la Pi guardando el historial | Se puede demostrar con el panel en modo simulado, avisando al profesor |
| **Importante** | Casos de falla (atasco, contenedor lleno, sin etiqueta) | Se prueban los más sencillos y los demás se explican como limitación |
| **Deseable** | 50 pasadas completas | Bajar a 30 y decirlo en el reporte (no inventar resultados) |
| **Deseable** | Arranque automático de la Pi, acceso desde el celular | Se deja manual |

---

## 6. Qué puede salir mal

| Riesgo | Por qué importa | Qué hacemos |
|---|---|---|
| **La banda llega tarde o dañada** (llega el 17, sin margen) | Es la más grave: sin ella no hay ajuste ni pruebas físicas | Pedir al vendedor el número de guía y confirmar el envío el lunes 16. Si no está en nuestras manos **el miércoles 18 a más tardar**, usar plan B: una pista improvisada (carril corto con el motor, o mover las piezas a mano frente al lector) para poder demostrar la clasificación |
| **El motor amarillo no es compatible con el driver** | Si pide más voltaje o corriente de lo que soporta el driver, no gira bien o se calienta | Revisar la ficha esta semana (A). Si no coincide, hay tiempo de pedir otro driver solo antes del viernes 13 |
| El lector NFC no lee con la banda en movimiento | Alcance corto (2 cm) y orientación de la etiqueta | Fijar un solo lugar para pegar la etiqueta en las piezas; probar el miércoles temprano; si es necesario, bajar la velocidad de la banda |
| Se reinicia el ESP32 al mover el motor | Caída de voltaje | B lo prueba el martes; usar fuente separada para el motor |
| Un solo ESP32 para cuatro personas | Se estorban | Turnos acordados; C programa sin la placa y la reserva por horas |
| Poco tiempo juntos | Solo 4 sesiones de 2 horas | Llegar a cada sesión con la tarea previa lista (la tabla de §4) |
| Cambios de último minuto rompen lo que ya funcionaba | Pasa mucho antes de una entrega | Congelar el lunes 23 |

---

## 7. Cómo trabajamos para no estorbarnos

1. **Una rama de git por módulo** y una carpeta propia, como en el plan anterior (§6 de ese documento).
2. **Al terminar cada sesión**, cada quien anota en una línea qué quedó probado y qué falta (en el chat del equipo).
3. **Nada se marca como terminado sin evidencia:** una foto, captura o medición.
4. **Si cambia algo que afecta a otros** (un pin, el formato de los mensajes, la posición de un sensor), se avisa  al grupo y se actualiza el archivo correspondiente.

---

## 8. Preguntas que hay que contestar hoy

1. ~~¿Cuándo llega la banda?~~ **Resuelto: martes 17 a las 9:00.**
2. ¿El martes 24 es **demostración, entrega de reporte, o ambos**, y a qué hora?
3. ¿Quién toma cada módulo (A, B, C, D)?
4. ¿Qué componentes ya están en mano? ¿Hay un segundo ESP32?
5. ¿Qué objetos se van a clasificar (tamaño y material) y dónde se pega la etiqueta?
6. ¿El motor amarillo de la banda comprada se puede manejar con el driver que ya tenemos (TB6612)? Hay que revisar su voltaje y corriente en cuanto se tenga el modelo. [Por confirmar]
7. ¿Cómo se verá el panel el día de la entrega (red de la universidad, cable o punto de acceso de la Pi)?
