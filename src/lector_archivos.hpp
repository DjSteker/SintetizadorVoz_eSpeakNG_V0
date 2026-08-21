/*
 * lector_archivos.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#pragma once

#ifndef LECTOR_ARCHIVOS_HPP_
#define LECTOR_ARCHIVOS_HPP_

#include <string>

namespace LectorArchivos {

struct ResultadoLectura {
	bool exito = false;
	std::string contenido;
	std::string mensaje_error;
};

// Lee el contenido completo de un archivo de texto plano en `ruta`.
ResultadoLectura leer_archivo_texto(const std::string &ruta);

} // namespace LectorArchivos

#endif /* LECTOR_ARCHIVOS_HPP_ */
