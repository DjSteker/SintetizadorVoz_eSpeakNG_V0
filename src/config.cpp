/*
 * config.cpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#include "config.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <unistd.h>

namespace Configuracion {

namespace {

// Devuelve el directorio que contiene el ejecutable en curso, leyendo el
// enlace simbólico /proc/self/exe (específico de Linux). Si por algún
// motivo no se puede resolver, se usa el directorio de trabajo actual
// como último recurso, para que la aplicación nunca deje de funcionar.
std::string obtener_directorio_ejecutable() {
	char buffer[PATH_MAX];
	ssize_t longitud = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (longitud <= 0) {
		std::cerr << "Aviso: no se pudo resolver /proc/self/exe; se usará el directorio actual "
					"para la configuración." << std::endl;
		return ".";
	}
	buffer[longitud] = '\0';
	std::string ruta_ejecutable(buffer);
	size_t pos_barra = ruta_ejecutable.find_last_of('/');
	if (pos_barra == std::string::npos) {
		return ".";
	}
	return ruta_ejecutable.substr(0, pos_barra);
}

} // namespace

std::string obtener_ruta_config() {
	return obtener_directorio_ejecutable() + "/" + NOMBRE_ARCHIVO_CONFIG;
}

bool guardar_configuracion(const AppConfig &config) {
	std::string ruta = obtener_ruta_config();
	std::ofstream archivo(ruta);
	if (!archivo.is_open()) {
		std::cerr << "Error: no se pudo abrir '" << ruta << "' para escritura." << std::endl;
		return false;
	}
	archivo << "voice=" << config.voz_actual << "\n";
	archivo << "language=" << config.codigo_idioma << "\n";
	archivo << "rate=" << config.velocidad << "\n";
	archivo << "pitch=" << config.tono << "\n";
	archivo << "volume=" << config.volumen << "\n";
	archivo << "speak_status=" << (config.leer_estado ? "1" : "0") << "\n";
	return true;
}

AppConfig cargar_configuracion() {
	AppConfig config;
	std::string ruta = obtener_ruta_config();
	std::ifstream archivo(ruta);
	if (!archivo.is_open()) {
		return config;
	}

	std::string linea;
	while (std::getline(archivo, linea)) {
		size_t pos = linea.find('=');
		if (pos == std::string::npos) {
			continue;
		}
		std::string clave = linea.substr(0, pos);
		std::string valor = linea.substr(pos + 1);
		try {
			if (clave == "voice") {
				config.voz_actual = valor;
			} else if (clave == "language") {
				config.codigo_idioma = valor;
			} else if (clave == "rate") {
				config.velocidad = std::stoi(valor);
			} else if (clave == "pitch") {
				config.tono = std::stoi(valor);
			} else if (clave == "volume") {
				config.volumen = std::stoi(valor);
			} else if (clave == "speak_status") {
				config.leer_estado = (valor == "1");
			}
		} catch (const std::exception &e) {
			std::cerr << "Aviso: valor inválido para '" << clave << "' en " << ruta << " ("
					<< e.what() << "). Se usará el valor por defecto." << std::endl;
		}
	}
	return config;
}

bool existe_configuracion_guardada() {
	std::ifstream archivo(obtener_ruta_config());
	return archivo.is_open();
}

std::string detectar_idioma_sistema() {
	// Mismo orden de precedencia que usa `locale`/gettext: LC_ALL manda
	// sobre LC_MESSAGES, que a su vez manda sobre LANG; LANGUAGE es una
	// extensión de GNU con una lista de preferencias separadas por ':'.
	static const char *variables_entorno[] = {"LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"};
	for (const char *variable : variables_entorno) {
		const char *valor = std::getenv(variable);
		if (!valor || valor[0] == '\0') {
			continue;
		}
		std::string locale_str(valor);
		if (locale_str == "C" || locale_str == "POSIX") {
			continue; // No indica un idioma real.
		}
		size_t pos_separador = locale_str.find(':');
		if (pos_separador != std::string::npos) {
			locale_str = locale_str.substr(0, pos_separador);
		}
		if (locale_str.length() < 2) {
			continue;
		}
		std::string codigo = locale_str.substr(0, 2);
		for (char &c : codigo) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return codigo;
	}
	return "";
}

} // namespace Configuracion
