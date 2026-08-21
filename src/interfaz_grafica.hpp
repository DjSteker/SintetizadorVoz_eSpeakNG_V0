/*
 * interfaz_grafica.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: DjSteker
 */

/**
 * @file interfaz_grafica.hpp
 * @brief Declaraciones públicas de la interfaz gráfica (GTK).
 *
 * Este módulo construye y muestra la ventana principal de la aplicación y
 * expone la función para conectar con la señal "activate" de GtkApplication.
 *
 * @author usuario001
 * @date 28 jul 2026
 */

#ifndef INTERFAZ_GRAFICA_HPP_
#define INTERFAZ_GRAFICA_HPP_

#include <gtk/gtk.h>

namespace InterfazGrafica {

/**
 * @brief Construye y muestra la ventana principal.
 *
 * Diseñada para conectarse directamente a la señal "activate" de GtkApplication:
 * @code
 * g_signal_connect(app, "activate", G_CALLBACK(InterfazGrafica::activar), NULL);
 * @endcode
 *
 * @param app Puntero a GtkApplication que activa la ventana.
 * @param datos_usuario Puntero de datos de usuario (no usado, puede ser nullptr).
 */
void activar(GtkApplication *app, gpointer datos_usuario);

} // namespace InterfazGrafica

#endif /* INTERFAZ_GRAFICA_HPP_ */
