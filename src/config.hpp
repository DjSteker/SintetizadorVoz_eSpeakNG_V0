/*
 * config.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: DjSteker
 */

/**
 * @file config.hpp
 * @brief Definiciones y funciones para la configuración de la aplicación.
 *
 * Este fichero declara los valores por defecto, la estructura que representa
 * la configuración de la aplicación y funciones para guardar/cargar y detectar
 * el idioma del sistema. La configuración se persiste junto al ejecutable.
 *
 * @author usuario001
 * @date 28 jul 2026
 */

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>

namespace Configuracion {

/** @name Valores por defecto
 * Constantes usadas cuando no existe configuración previa.
 * @{ */
constexpr int VELOCIDAD_POR_DEFECTO = 175; /**< Velocidad en WPM por defecto. */
constexpr int TONO_POR_DEFECTO = 50; /**< Tono por defecto. */
constexpr int VOLUMEN_POR_DEFECTO = 99; /**< Volumen por defecto (0-99). */
constexpr bool LEER_ESTADO_POR_DEFECTO = true; /**< Lectura de estado por defecto. */
inline const std::string VOZ_POR_DEFECTO = "es"; /**< Identificador de voz por defecto. */
inline const std::string IDIOMA_POR_DEFECTO = "es";/**< Código de idioma por defecto. */
inline const std::string NOMBRE_ARCHIVO_CONFIG = "tts_config.txt"; /**< Nombre de archivo de configuración. */
/** @} */

/**
 * @brief Estructura que contiene la configuración de la aplicación.
 *
 * Campos:
 *  - voz_actual: nombre interno de la voz seleccionada (para espeak_SetVoiceByName).
 *  - codigo_idioma: código ISO 639-1 o extendido (p.ej. "es", "es-419").
 *  - velocidad: palabras por minuto.
 *  - tono: valor de tono para el sintetizador.
 *  - volumen: 0-99.
 *  - leer_estado: si se deben anunciar mensajes de estado por voz.
 */
struct AppConfig {
	std::string voz_actual = VOZ_POR_DEFECTO;
	std::string codigo_idioma = IDIOMA_POR_DEFECTO; /**< Idioma base seleccionado, p.ej. "es" */
	int velocidad = VELOCIDAD_POR_DEFECTO; /**< Velocidad en WPM */
	int tono = TONO_POR_DEFECTO; /**< Tono (valor interno del motor) */
	int volumen = VOLUMEN_POR_DEFECTO; /**< Volumen (0-99) */
	bool leer_estado = LEER_ESTADO_POR_DEFECTO; /**< Activar lectura de mensajes de estado */
};

/**
 * @brief Obtiene la ruta completa del archivo de configuración.
 *
 * El archivo de configuración se guarda junto al ejecutable para facilidad
 * de despliegue. Ejemplo: /ruta/a/SintetizadorVoz/tts_config.txt
 *
 * @return Ruta completa al archivo de configuración (cadena).
 */
std::string obtener_ruta_config();

/**
 * @brief Guarda la configuración en disco.
 *
 * El formato es simple de pares clave=valor por línea.
 *
 * @param config Configuración a persistir.
 * @return true si la operación tuvo éxito, false en caso contrario.
 */
bool guardar_configuracion(const AppConfig &config);

/**
 * @brief Carga la configuración desde disco.
 *
 * Si el archivo no existe o contiene valores inválidos, se devuelven
 * los valores por defecto en la estructura.
 *
 * @return AppConfig con los valores leídos o por defecto.
 */
AppConfig cargar_configuracion();

/**
 * @brief Indica si existe una configuración guardada previamente.
 *
 * @return true si el archivo de configuración existe y es legible.
 */
bool existe_configuracion_guardada();

/**
 * @brief Detecta el idioma configurado en el sistema (ISO 639-1 preferente).
 *
 * Utiliza las variables de entorno de locale (LC_ALL, LC_MESSAGES, LANG,
 * LANGUAGE) siguiendo la precedencia habitual. Devuelve cadena vacía si no
 * se pudo detectar un idioma válido.
 *
 * @return Código de idioma en minúsculas (p.ej. "es") o "" si no se detectó.
 */
std::string detectar_idioma_sistema();

} // namespace Configuracion

#endif /* CONFIG_HPP_ */
