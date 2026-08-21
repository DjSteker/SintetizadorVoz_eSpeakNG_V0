/*
 * lector_archivos.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

/**
 * @file lector_archivos.hpp
 * @brief Utilidades para lectura de archivos de texto.
 *
 * Proporciona funciones simples y seguras para leer el contenido completo de
 * un archivo de texto en memoria, devolviendo un resultado con estado y
 * mensaje de error en caso de fallo.
 *
 * @author usuario001
 * @date 28 jul 2026
 */

#ifndef LECTOR_ARCHIVOS_HPP_
#define LECTOR_ARCHIVOS_HPP_

#include <string>

namespace LectorArchivos {

/**
 * @brief Resultado de una operación de lectura de archivo.
 *
 * @var ResultadoLectura::exito true si la lectura fue satisfactoria.
 * @var ResultadoLectura::contenido Contenido completo del archivo (binario/texto).
 * @var ResultadoLectura::mensaje_error Mensaje descriptivo en caso de fallo.
 */
struct ResultadoLectura {
	bool exito = false; /**< Indica si la lectura tuvo éxito */
	std::string contenido; /**< Contenido del archivo */
	std::string mensaje_error; /**< Mensaje de error en caso de fallo */
};

/**
 * @brief Lee el contenido completo de un archivo de texto en `ruta`.
 *
 * Se abre el archivo en modo binario para preservar saltos de línea y
 * codificaciones; la función devuelve el contenido tal cual.
 *
 * @param ruta Ruta al archivo a leer.
 * @return ResultadoLectura con los datos o el mensaje de error.
 */
ResultadoLectura leer_archivo_texto(const std::string &ruta);

} // namespace LectorArchivos

#endif /* LECTOR_ARCHIVOS_HPP_ */
