/*
 * config.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#pragma once

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>

namespace Configuracion {

// --- Valores por defecto ---
constexpr int VELOCIDAD_POR_DEFECTO = 175;
constexpr int TONO_POR_DEFECTO = 50;
constexpr int VOLUMEN_POR_DEFECTO = 99;
constexpr bool LEER_ESTADO_POR_DEFECTO = true;
inline const std::string VOZ_POR_DEFECTO = "es";
inline const std::string IDIOMA_POR_DEFECTO = "es";
inline const std::string NOMBRE_ARCHIVO_CONFIG = "tts_config.txt";

struct AppConfig {
	std::string voz_actual = VOZ_POR_DEFECTO;
	std::string codigo_idioma = IDIOMA_POR_DEFECTO; // idioma base seleccionado, p.ej. "es"
	int velocidad = VELOCIDAD_POR_DEFECTO;
	int tono = TONO_POR_DEFECTO;
	int volumen = VOLUMEN_POR_DEFECTO;
	bool leer_estado = LEER_ESTADO_POR_DEFECTO;
};

// Ruta completa del archivo de configuración, guardado junto al propio
// ejecutable (p.ej. /ruta/a/SintetizadorVoz/tts_config.txt), para que la
// aplicación sea autocontenida y portable sin depender del directorio de
// configuración del usuario.
std::string obtener_ruta_config();

// Guarda la configuración en disco. Devuelve true si tuvo éxito.
bool guardar_configuracion(const AppConfig &config);

// Carga la configuración desde disco. Si el archivo no existe, o algún
// valor está corrupto, se usan los valores por defecto correspondientes.
AppConfig cargar_configuracion();

// Indica si ya existe un archivo de configuración guardado previamente
// por el usuario. Útil para distinguir la primera ejecución (donde
// conviene partir del idioma del sistema) de ejecuciones posteriores.
bool existe_configuracion_guardada();

// Intenta detectar el código de idioma (ISO 639-1, dos letras en
// minúsculas, p.ej. "es") configurado en el sistema, a partir de las
// variables de entorno de locale habituales (LC_ALL, LC_MESSAGES, LANG,
// LANGUAGE) — el mismo mecanismo que usa el comando `locale`. Devuelve
// una cadena vacía si no se pudo determinar ningún idioma.
std::string detectar_idioma_sistema();

} // namespace Configuracion
#endif /* CONFIG_HPP_ */
