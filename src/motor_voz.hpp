/*
 * motor_voz.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

/**
 * @file src/motor_voz.hpp
 * @brief Interfaz de alto nivel con el motor de síntesis eSpeak‑NG y control de backends de audio.
 *
 * Proporciona funciones para inicializar y finalizar el motor, listar voces,
 * aplicar parámetros de síntesis, reproducir texto, exportar WAV y configurar
 * parámetros de audio (sample rate, canales, buffer y backend).
 *
 * Soporta dos modos de reproducción:
 *  - "espeak_playback": salida directa gestionada por eSpeak‑NG (AUDIO_OUTPUT_PLAYBACK).
 *  - "portaudio": recuperación de PCM desde eSpeak (AUDIO_OUTPUT_RETRIEVAL) y
 *                 reproducción mediante PortAudio (si se compila con soporte).
 *
 * Nota: las operaciones que realizan síntesis son bloqueantes y deberían
 * ejecutarse en hilos secundarios desde la interfaz de usuario.
 *
 * @author DjSteker
 * @date 2026-07-28
 */

#ifndef MOTOR_VOZ_HPP_
#define MOTOR_VOZ_HPP_

#include "config.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MotorVoz {

/**
 * @brief Devuelve la parte de idioma base de un código completo.
 *
 * Ejemplos:
 *  - "es-419" -> "es"
 *  - "en-US"  -> "en"
 *  - "es"     -> "es"
 *
 * Esta función está definida en motor_voz.cpp y se declara aquí para que
 * otros módulos (p. ej. la interfaz gráfica) puedan usarla.
 *
 * @param codigo_idioma Código de idioma completo.
 * @return Parte base del idioma (antes del guion) o el mismo string si no hay guion.
 */
std::string codigo_idioma_base(const std::string &codigo_idioma);

/**
 * @brief Información sobre una voz disponible en eSpeak‑NG.
 *
 * - nombre_visible: etiqueta para mostrar en la UI.
 * - nombre_interno: identificador pasado a espeak_SetVoiceByName.
 * - codigo_idioma: código real de idioma (p. ej. "es", "es-419") extraído de la voz.
 */
struct VozInfo {
	std::string nombre_visible;
	std::string nombre_interno;
	std::string codigo_idioma;
};

/**
 * @brief Callback usado para notificar estado a la interfaz.
 *
 * @param mensaje Texto descriptivo del estado (puede incluir emojis).
 * @param leer_en_voz_alta Si true, la UI puede decidir anunciar el mensaje mediante TTS.
 */
using CallbackEstado = std::function<void(const std::string &mensaje, bool leer_en_voz_alta)>;

/**
 * @brief Inicializa el motor eSpeak‑NG y la infraestructura de audio.
 *
 * Debe llamarse antes de usar cualquier otra función del módulo.
 * Inicializa eSpeak en modo por defecto (AUDIO_OUTPUT_PLAYBACK) y registra
 * el callback de síntesis.
 *
 * @return true si la inicialización fue correcta, false en caso contrario.
 */
bool inicializar();

/**
 * @brief Libera recursos del motor y finaliza la infraestructura de audio.
 *
 * Debe llamarse al cerrar la aplicación para detener hilos, streams y eSpeak‑NG.
 */
void finalizar();

/**
 * @brief Devuelve todas las voces instaladas en el sistema.
 *
 * @return Vector con la información de cada voz encontrada.
 */
std::vector<VozInfo> listar_voces_disponibles();

/**
 * @brief Devuelve las voces cuyo código de idioma base coincide con @p codigo_idioma.
 *
 * Ejemplo: pasar "es" devolverá voces "es", "es-419", etc.
 *
 * @param codigo_idioma Código base de idioma (p. ej. "es", "en").
 * @return Vector con las voces filtradas por idioma.
 */
std::vector<VozInfo> listar_voces_por_idioma(const std::string &codigo_idioma);

/**
 * @brief Aplica la voz y parámetros de síntesis (velocidad, tono, volumen).
 *
 * Internamente llama a espeak_SetVoiceByName y espeak_SetParameter.
 *
 * @param nombre_voz Nombre interno de la voz (identificador de eSpeak).
 * @param velocidad Velocidad en palabras por minuto (WPM).
 * @param tono Valor de tono (según escala del motor).
 * @param volumen Nivel de volumen (0-99).
 */
void aplicar_parametros(const std::string &nombre_voz, int velocidad, int tono, int volumen);

/**
 * @brief Sintetiza @p texto y lo reproduce por el dispositivo de audio.
 *
 * Operación bloqueante: ejecutar en hilo secundario desde la UI.
 * Notifica progreso/errores mediante @p callback_estado.
 *
 * @param texto Texto a sintetizar (se admite SSML si el motor lo soporta).
 * @param callback_estado Callback para notificar estado/errores.
 */
void hablar(const std::string &texto, const CallbackEstado &callback_estado);

/**
 * @brief Sintetiza @p texto y lo exporta a un archivo WAV en @p ruta_salida.
 *
 * La exportación respeta la configuración actual de sample rate y canales:
 * si eSpeak genera mono y la configuración pide stereo, las muestras se duplican.
 * Operación bloqueante: ejecutar en hilo secundario.
 *
 * @param texto Texto a sintetizar.
 * @param ruta_salida Ruta donde se escribirá el WAV.
 * @param callback_estado Callback para notificar progreso/errores.
 * @return true si el WAV se escribió correctamente, false en caso contrario.
 */
bool exportar_a_wav(const std::string &texto, const std::string &ruta_salida, const CallbackEstado &callback_estado);

/**
 * @brief Detiene la síntesis en curso (reproducción o exportación).
 *
 * Seguro para llamar desde la UI; marca cancelación y llama a espeak_Cancel().
 */
void detener();

/**
 * @brief Indica si hay una síntesis en curso actualmente.
 *
 * @return true si el motor está hablando o exportando; false en caso contrario.
 */
bool esta_hablando();

/**
 * @brief Lee un mensaje corto en voz alta inmediatamente.
 *
 * Diseñado para anunciar notificaciones o estados; elimina símbolos/emojis iniciales
 * para aumentar naturalidad. Bloqueante.
 *
 * @param mensaje Mensaje a anunciar.
 */
void leer_mensaje_estado(const std::string &mensaje);

/**
 * @brief Configura parámetros de audio desde la estructura AppConfig.
 *
 * Lee campos relevantes de @p config:
 *  - sample_rate: frecuencia objetivo (Hz).
 *  - channels: número de canales (1=mono, 2=stereo).
 *  - audio_buffer_frames: frames por buffer (latencia/estabilidad).
 *  - audio_backend: "espeak_playback" o "portaudio".
 *
 * Si se selecciona "portaudio", el módulo intentará inicializar PortAudio y
 * reconfigurar eSpeak para recuperar PCM (AUDIO_OUTPUT_RETRIEVAL). Si PortAudio
 * no está disponible o falla, se realiza fallback a "espeak_playback".
 *
 * @param config Estructura de configuración de la aplicación (Configuracion::AppConfig).
 */
void configurar_desde_appconfig(const Configuracion::AppConfig &config);

} // namespace MotorVoz

#endif /* MOTOR_VOZ_HPP_ */
