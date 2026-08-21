/*
 * lector_archivos.cpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#include "lector_archivos.hpp"

#include <fstream>
#include <sstream>

namespace LectorArchivos {

ResultadoLectura leer_archivo_texto(const std::string &ruta) {
	ResultadoLectura resultado;
	std::ifstream archivo(ruta, std::ios::in | std::ios::binary);
	if (!archivo.is_open()) {
		resultado.exito = false;
		resultado.mensaje_error = "No se pudo abrir el archivo: " + ruta;
		return resultado;
	}

	std::stringstream buffer;
	buffer << archivo.rdbuf();
	resultado.contenido = buffer.str();
	resultado.exito = true;
	return resultado;
}

} // namespace LectorArchivos

