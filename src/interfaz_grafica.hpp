/*
 * interfaz_grafica.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#pragma once

#ifndef INTERFAZ_GRAFICA_HPP_
#define INTERFAZ_GRAFICA_HPP_

#include <gtk/gtk.h>

namespace InterfazGrafica {

// Construye y muestra la ventana principal. Pensada para conectarse
// directamente a la señal "activate" de GtkApplication.
void activar(GtkApplication *app, gpointer datos_usuario);

} // namespace InterfazGrafica

#endif /* INTERFAZ_GRAFICA_HPP_ */
