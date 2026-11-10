"""Protocolo UART entre el ESP32 y la Raspberry Pi (Formato A ampliado).

Una trama por linea:  <cuerpo>*HH\\n   donde HH es el XOR de los caracteres del cuerpo en hex.
Especificacion completa: docs/protocolo-uart.md. El lado del ESP32 esta en
firmware/banda_clasificadora/protocolo.cpp; ambos deben cambiar juntos.
"""

import re

CATEGORIAS = ("A", "B", "R")
MOTIVOS = ("OK", "SIN_LECTURA", "NO_REGISTRADA")
MAX_LINEA = 700          # tamano del bufer de recepcion del ESP32 (incluye *HH)
MAX_ETIQUETAS = 32       # MAX_ETIQUETAS del firmware
MAX_UID = 20

_RE_UID = re.compile(r"^[0-9A-F]{1,%d}$" % MAX_UID)
_RE_NOMBRE = re.compile(r"^[A-Z][A-Z0-9_]*$")


class ErrorProtocolo(ValueError):
    pass


# --------------------------------------------------------------------------- tramas

def checksum(cuerpo):
    x = 0
    for b in cuerpo.encode("ascii", errors="replace"):
        x ^= b
    return x


def trama(cuerpo):
    """Bytes listos para escribir en el puerto: b'<cuerpo>*HH\\n'."""
    linea = f"{cuerpo}*{checksum(cuerpo):02X}\n"
    if len(linea) > MAX_LINEA:
        raise ErrorProtocolo(f"trama de {len(linea)} caracteres; el ESP32 acepta hasta {MAX_LINEA}")
    return linea.encode("ascii")


def validar(linea):
    """Devuelve el cuerpo si el checksum es correcto, o None."""
    linea = linea.strip()
    if len(linea) < 4 or linea[-3] != "*":
        return None
    cuerpo, hh = linea[:-3], linea[-2:]
    try:
        return cuerpo if int(hh, 16) == checksum(cuerpo) else None
    except ValueError:
        return None


def normalizar_uid(texto):
    """'04:a1:b2 ...' -> '04A1B2...'. Lanza ErrorProtocolo si no es hexadecimal valido."""
    uid = re.sub(r"[\s:\-]", "", str(texto)).upper()
    if not _RE_UID.match(uid):
        raise ErrorProtocolo(f"UID invalido: {texto!r} (hexadecimal, 1 a {MAX_UID} caracteres)")
    return uid


def _entero(texto):
    if not texto.isdigit() or len(texto) > 10:
        raise ErrorProtocolo(f"numero invalido: {texto!r}")
    v = int(texto)
    if v > 0xFFFFFFFF:
        raise ErrorProtocolo(f"numero fuera de rango: {texto!r}")
    return v


# --------------------------------------------------------------------------- ESP32 -> Pi

def interpretar(cuerpo):
    """Convierte el cuerpo de una trama del ESP32 en un dict. Lanza ErrorProtocolo si no es valido.

    PIEZA        {'tipo','seq','uid'|None,'categoria','motivo'|None,'ciclo_ms','reenvio'}
    ESTADO       {'tipo','estado'}
    LATIDO       {'tipo','pendientes','version_tabla','ultimo_seq'|None}
    ALERTA       {'tipo','codigo','detalle'|None}
    CONTENEDORES {'tipo','contenido':{A,B,R},'capacidad':{A,B,R}}
    """
    campos = cuerpo.split(",")
    tipo = campos[0]

    if tipo == "PIEZA":
        if len(campos) != 7:
            raise ErrorProtocolo("PIEZA necesita 7 campos")
        _, seq, uid, cat, motivo, ciclo, reenvio = campos
        if cat not in CATEGORIAS or motivo not in MOTIVOS or reenvio not in ("0", "1"):
            raise ErrorProtocolo(f"PIEZA con valores invalidos: {cuerpo}")
        return {
            "tipo": "PIEZA",
            "seq": _entero(seq),
            "uid": None if uid == "-" else normalizar_uid(uid),
            "categoria": cat,
            "motivo": None if motivo == "OK" else motivo,
            "ciclo_ms": _entero(ciclo),
            "reenvio": reenvio == "1",
        }

    if tipo == "ESTADO":
        if len(campos) != 2 or not _RE_NOMBRE.match(campos[1]):
            raise ErrorProtocolo("ESTADO invalido")
        return {"tipo": "ESTADO", "estado": campos[1]}

    if tipo == "LATIDO":
        if len(campos) not in (3, 4):   # 3 campos: firmware de pruebas/integracion_test
            raise ErrorProtocolo("LATIDO necesita 3 o 4 campos")
        return {
            "tipo": "LATIDO",
            "pendientes": _entero(campos[1]),
            "version_tabla": _entero(campos[2]),
            "ultimo_seq": _entero(campos[3]) if len(campos) == 4 else None,
        }

    if tipo == "ALERTA":
        if len(campos) not in (2, 3) or not _RE_NOMBRE.match(campos[1]):
            raise ErrorProtocolo("ALERTA invalida")
        return {"tipo": "ALERTA", "codigo": campos[1], "detalle": campos[2] if len(campos) == 3 else None}

    if tipo == "CONTENEDORES":
        if len(campos) != 7:
            raise ErrorProtocolo("CONTENEDORES necesita 7 campos")
        n = [_entero(c) for c in campos[1:]]
        return {
            "tipo": "CONTENEDORES",
            "contenido": dict(zip(CATEGORIAS, n[:3])),
            "capacidad": dict(zip(CATEGORIAS, n[3:])),
        }

    raise ErrorProtocolo(f"tipo de trama desconocido: {tipo!r}")


# --------------------------------------------------------------------------- Pi -> ESP32

def cmd_ack(seq):
    return f"ACK,{int(seq)}"


def cmd_vivo(version):
    return f"VIVO,{int(version)}"


def cmd_reenviar(seq):
    return f"REENVIAR,{int(seq)}"


def cmd_sync(version, tabla):
    """tabla: dict uid -> categoria. Se ordena para que la trama sea siempre la misma."""
    if len(tabla) > MAX_ETIQUETAS:
        raise ErrorProtocolo(f"la tabla tiene {len(tabla)} etiquetas; el ESP32 admite {MAX_ETIQUETAS}")
    normalizada = {}
    for uid, cat in tabla.items():
        if cat not in CATEGORIAS:
            raise ErrorProtocolo(f"categoria invalida para {uid}: {cat!r}")
        normalizada[normalizar_uid(uid)] = cat
    partes = [f"{uid}={cat}" for uid, cat in sorted(normalizada.items())]
    cuerpo = f"SYNC,{int(version)}," + ";".join(partes)
    trama(cuerpo)   # valida la longitud
    return cuerpo


def cmd_vaciado(categoria):
    if categoria not in CATEGORIAS:
        raise ErrorProtocolo(f"categoria invalida: {categoria!r}")
    return f"VACIADO,{categoria}"


def cmd_capacidad(categoria, n):
    if categoria not in CATEGORIAS:
        raise ErrorProtocolo(f"categoria invalida: {categoria!r}")
    n = int(n)
    if not 1 <= n <= 999:
        raise ErrorProtocolo("la capacidad debe estar entre 1 y 999")
    return f"CAPACIDAD,{categoria},{n}"


def cmd_paro():
    return "PARO"


# --------------------------------------------------------------------------- alertas

# codigo -> (nivel, mensaje). nivel: critico | advertencia | info
ALERTAS = {
    "REINICIO": ("advertencia", "El ESP32 arranco ({detalle})."),
    "ARRANQUE": ("info", "Banda puesta en marcha con el boton Restablecer."),
    "ATASCO": ("critico", "Atasco ({detalle}): banda detenida. Retira la pieza y presiona Restablecer."),
    "SIN_SALIDA": ("critico", "La pieza no llego al sensor de salida a tiempo: banda detenida. "
                              "Revisa la compuerta y presiona Restablecer."),
    "PIEZAS_JUNTAS": ("advertencia", "Llego otra pieza antes de terminar la anterior."),
    "SALIDA_INESPERADA": ("advertencia", "Paso una pieza por la salida sin detectarse en inspeccion (no se registro)."),
    "EMERGENCIA": ("critico", "Paro de emergencia ({detalle}). Suelta el paro y presiona Restablecer."),
    "RESTABLECIDO": ("info", "El operador restablecio el sistema."),
    "CONTENEDOR_LLENO": ("advertencia", "Contenedor {detalle} lleno: banda en pausa hasta marcarlo como vaciado."),
    "VACIADO": ("info", "Contenedor {detalle} marcado como vaciado con el boton de la banda."),
    "NFC_FALLA": ("critico", "El ESP32 no detecta el lector RC522: todas las piezas iran a rechazo."),
    "PENDIENTES_PERDIDOS": ("critico", "Se lleno la cola de eventos del ESP32: se perdio el evento seq {detalle}."),
    "SEQ_NO_DISPONIBLE": ("advertencia", "El ESP32 ya no tiene el evento seq {detalle}: no se puede recuperar."),
    "SYNC_OK": ("info", "Tabla local del ESP32 actualizada (v{detalle})."),
    "SYNC_INVALIDO": ("advertencia", "El ESP32 rechazo la tabla enviada (formato invalido)."),
}


def describir_alerta(codigo, detalle):
    nivel, plantilla = ALERTAS.get(codigo, ("advertencia", "Alerta del ESP32: {codigo} {detalle}"))
    if codigo == "REINICIO" and detalle == "BROWNOUT":
        return "critico", "El ESP32 se reinicio por CAIDA DE VOLTAJE (brownout). Revisa la alimentacion."
    if codigo == "REINICIO" and detalle == "WATCHDOG":
        return "critico", "El ESP32 se reinicio por el watchdog (el programa se trabo)."
    return nivel, plantilla.format(codigo=codigo, detalle=detalle or "-")
