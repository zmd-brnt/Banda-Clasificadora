// Estadistica de un grupo de mediciones (en archivo aparte para que el preprocesador de
// Arduino conozca el tipo antes de generar los prototipos de las funciones del .ino).
#pragma once

struct Estadistica {
  float min, max, suma;
  int validas, sinEco;
};
