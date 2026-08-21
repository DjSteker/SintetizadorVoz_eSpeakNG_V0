/*
 * motor_voz.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: DjSteker
 */

/**
 * @file motor_voz.hpp
 * @brief Interfaz de alto nivel con el motor eSpeak-NG.
 *
 * Provee funciones para inicializar y controlar la síntesis TTS, listar
 * voces instaladas, aplicar parámetros y exportar síntesis a WAV.
 *
 * Nota: las operaciones de síntesis (hablar, exportar_a_wav, leer_mensaje_estado)
 * son bloqueantes y deben ejecutarse en hilos secundarios cuando se usan desde
 * la interfaz gráfica.
 *
 * @author usuario001
 * @date 28 jul 2026
 */

#ifndef MOTOR_VOZ_HPP_
#define MOTOR_VOZ_HPP_

#include <functional>
#include <string>
#include <vector>

namespace MotorVoz {

/**
 * @brief Información sobre una voz disponible en eSpeak-NG.
 *
 * - nombre_visible: etiqueta amistosa para mostrar en la UI.
 * - nombre_interno: identificador usado por espeak_SetVoiceByName.
 * - codigo_idioma: código de idioma real (p.ej. "es", "es-419").
 */
struct VozInfo {
	std::string nombre_visible; /**< p.ej. "Spanish (Latin America) (es-419)" */
	std::string nombre_interno; /**< identificador único para espeak_SetVoiceByName */
	std::string codigo_idioma; /**< código de idioma real (voice->languages) */
};

/**
 * @brief Devuelve la parte de idioma base de un código completo.
 *
 * Ejemplos:
 *  - "es-419" -> "es"
 *  - "en-us"  -> "en"
 *  - "es"     -> "es"
 *
 * @param codigo_idioma Código de idioma completo.
 * @return Parte base del idioma (dos letras) o el mismo string si no hay guion.
 */
std::string codigo_idioma_base(const std::string &codigo_idioma);

/**
 * @brief Callback para informar estado a la UI.
 *
 * @param mensaje Texto descriptivo del estado.
 * @param leer_en_voz_alta Si true, la UI puede optar por anunciar el mensaje por TTS.
 */
using CallbackEstado = std::function<void(const std::string &mensaje, bool leer_en_voz_alta)>;

/**
 * @brief Inicializa el motor eSpeak-NG.
 *
 * Debe llamarse antes de cualquier otra operación del motor. Reserva/abre
 * recursos globales internos.
 *
 * @return true si se inicializó correctamente, false en caso de error.
 */
bool inicializar();

/**
 * @brief Libera recursos y finaliza el uso del motor.
 *
 * Llamar al cerrar la aplicación.
 */
void finalizar();

/**
 * @brief Devuelve todas las voces instaladas en el sistema.
 *
 * @return Vector con la información de cada voz encontrada.
 */
std::vector<VozInfo> listar_voces_disponibles();

/**
 * @brief Devuelve las voces cuyo código de idioma coincide con `codigo_idioma`.
 *
 * @param codigo_idioma Código base de idioma (p.ej. "es").
 * @return Vector con las voces filtradas por idioma.
 */
std::vector<VozInfo> listar_voces_por_idioma(const std::string &codigo_idioma);

/**
 * @brief Aplica la voz y los parámetros de síntesis (velocidad, tono, volumen).
 *
 * @param nombre_voz Nombre interno de la voz (pasado a espeak_SetVoiceByName).
 * @param velocidad Palabras por minuto.
 * @param tono Valor de tono.
 * @param volumen Nivel de volumen (0-99).
 */
void aplicar_parametros(const std::string &nombre_voz, int velocidad, int tono, int volumen);

/**
 * @brief Sintetiza `texto` y lo reproduce por el dispositivo de audio.
 *
 * Bloqueante: ejecutar en hilo secundario desde la UI.
 *
 * @param texto Texto a sintetizar (puede contener SSML si el motor lo admite).
 * @param callback_estado Callback para notificar progreso/errores.
 */
void hablar(const std::string &texto, const CallbackEstado &callback_estado);

/**
 * @brief Sintetiza `texto` y lo vuelca a un archivo WAV en `ruta_salida`.
 *
 * Bloqueante: ejecutar en hilo secundario. Si la operación falla, se
 * notifica mediante el callback.
 *
 * @param texto Texto a sintetizar.
 * @param ruta_salida Ruta del archivo WAV de salida.
 * @param callback_estado Callback para notificar progreso/errores.
 * @return true si se escribió el WAV correctamente, false en caso contrario.
 */
bool exportar_a_wav(const std::string &texto, const std::string &ruta_salida, const CallbackEstado &callback_estado);

/**
 * @brief Detiene la síntesis en curso (reproducción o exportación).
 *
 * Es seguro llamar desde la UI para cancelar una operación activa.
 */
void detener();

/**
 * @brief Indica si hay una síntesis en curso actualmente.
 *
 * @return true si el motor está hablando/exportando, false en caso contrario.
 */
bool esta_hablando();

/**
 * @brief Lee un mensaje corto en voz alta inmediatamente.
 *
 * Diseñado para anunciar mensajes de estado a usuarios con discapacidad
 * visual. Es bloqueante mientras dura la reproducción.
 *
 * @param mensaje Mensaje a anunciar.
 */
void leer_mensaje_estado(const std::string &mensaje);

} // namespace MotorVoz

#endif /* MOTOR_VOZ_HPP_ */
