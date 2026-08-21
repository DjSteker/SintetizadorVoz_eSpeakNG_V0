/*
 * config.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

/**
 * @file src/config.hpp
 * @brief Definiciones y funciones para la configuración de la aplicación TTS.
 *
 * Declara valores por defecto, la estructura AppConfig (incluye parámetros de audio)
 * y funciones para guardar/cargar la configuración desde un archivo simple
 * de pares clave=valor ubicado junto al ejecutable.
 *
 * Formato del archivo de configuración (ejemplo):
 *   voice=es
 *   language=es
 *   rate=175
 *   pitch=50
 *   volume=99
 *   speak_status=1
 *   sample_rate=22050
 *   channels=1
 *   audio_buffer_frames=256
 *   audio_backend=espeak_playback
 *
 * @author DjSteker
 * @date 2026-07-28
 */

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>

namespace Configuracion {

/** @name Valores por defecto
 * Constantes usadas cuando no existe configuración previa.
 * @{ */
constexpr int VELOCIDAD_POR_DEFECTO = 175; /**< Velocidad por defecto (WPM). */
constexpr int TONO_POR_DEFECTO = 50; /**< Tono por defecto. */
constexpr int VOLUMEN_POR_DEFECTO = 99; /**< Volumen por defecto (0-99). */
constexpr bool LEER_ESTADO_POR_DEFECTO = true; /**< Leer mensajes de estado por defecto. */
inline const std::string VOZ_POR_DEFECTO = "es"; /**< Voz por defecto (identificador). */
inline const std::string IDIOMA_POR_DEFECTO = "es"; /**< Código de idioma por defecto. */
inline const std::string NOMBRE_ARCHIVO_CONFIG = "tts_config.txt"; /**< Nombre del archivo de configuración. */
/** @} */

/**
 * @brief Estructura que representa la configuración de la aplicación.
 *
 * Campos relacionados con síntesis:
 *  - voz_actual: nombre interno de la voz.
 *  - codigo_idioma: código ISO (p. ej. "es").
 *  - velocidad: palabras por minuto (WPM).
 *  - tono: valor de tono para el sintetizador.
 *  - volumen: 0-99.
 *  - leer_estado: si se anunciarán mensajes de estado por TTS.
 *
 * Campos de audio:
 *  - sample_rate: frecuencia objetivo para WAV y reproducción (Hz).
 *  - channels: 1 = mono, 2 = stereo.
 *  - audio_buffer_frames: frames por buffer para backend de reproducción.
 *  - audio_backend: "espeak_playback" o "portaudio" (si está disponible).
 */
struct AppConfig {
	std::string voz_actual = VOZ_POR_DEFECTO;
	std::string codigo_idioma = IDIOMA_POR_DEFECTO;
	int velocidad = VELOCIDAD_POR_DEFECTO; /**< WPM */
	int tono = TONO_POR_DEFECTO;
	int volumen = VOLUMEN_POR_DEFECTO;
	bool leer_estado = LEER_ESTADO_POR_DEFECTO;

	/* Parámetros de audio */
	int sample_rate = 22050; /**< Frecuencia objetivo en Hz (usar la devuelta por eSpeak idealmente). */
	int channels = 1; /**< 1=mono, 2=stereo. */
	int audio_buffer_frames = 256; /**< Frames por buffer para la reproducción (latencia). */
	std::string audio_backend = "espeak_playback"; /**< "espeak_playback" o "portaudio". */
};

/**
 * @brief Obtiene la ruta completa del archivo de configuración.
 *
 * El archivo se persiste junto al ejecutable para facilidad de despliegue.
 *
 * @return Ruta completa al archivo de configuración.
 */
std::string obtener_ruta_config();

/**
 * @brief Guarda la configuración en disco en formato clave=valor.
 *
 * Se sobrescribe el archivo existente.
 *
 * @param config Configuración a persistir.
 * @return true si la operación tuvo éxito, false en caso de error.
 */
bool guardar_configuracion(const AppConfig &config);

/**
 * @brief Carga la configuración desde disco.
 *
 * Si el archivo no existe o alguna clave tiene un valor inválido, se usan
 * los valores por defecto para esos campos.
 *
 * @return AppConfig con los valores cargados o por defecto.
 */
AppConfig cargar_configuracion();

/**
 * @brief Indica si existe una configuración guardada y legible.
 *
 * @return true si el archivo de configuración existe y es legible.
 */
bool existe_configuracion_guardada();

/**
 * @brief Detecta el idioma configurado en el sistema usando variables de entorno.
 *
 * Orden de precedencia: LC_ALL, LC_MESSAGES, LANG, LANGUAGE. Devuelve el
 * código en minúsculas (p. ej. "es") o cadena vacía si no se detecta.
 *
 * @return Código de idioma en minúsculas o "" si no se pudo detectar.
 */
std::string detectar_idioma_sistema();

} // namespace Configuracion

#endif /* CONFIG_HPP_ */
