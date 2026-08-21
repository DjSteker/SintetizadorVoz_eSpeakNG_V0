/*
 * motor_voz.hpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#pragma once

#ifndef MOTOR_VOZ_HPP_
#define MOTOR_VOZ_HPP_

#include <functional>
#include <string>
#include <vector>

namespace MotorVoz {

struct VozInfo {
	std::string nombre_visible; // p.ej. "Spanish (Latin America) (es-419)"
	std::string nombre_interno; // identificador único para espeak_SetVoiceByName (voice->identifier)
	std::string codigo_idioma;  // código de idioma real, p.ej. "es" o "es-419"
															// (voice->name es un nombre descriptivo, NO un código:
															// el código de idioma correcto sale de voice->languages)
};

// Devuelve la parte de idioma base de un código completo: "es-419" -> "es",
// "en-us" -> "en", "es" -> "es".
std::string codigo_idioma_base(const std::string &codigo_idioma);

// Callback invocado para reportar mensajes de estado a la interfaz.
// `leer_en_voz_alta` indica si conviene anunciar el mensaje por voz
// (a criterio del llamador, según la preferencia de accesibilidad).
using CallbackEstado = std::function<void(const std::string &mensaje, bool leer_en_voz_alta)>;

// Inicializa el motor eSpeak-NG. Debe llamarse una única vez al arrancar
// la aplicación, antes de crear la ventana principal. Devuelve false si
// falla la inicialización.
bool inicializar();

// Libera los recursos del motor. Debe llamarse al cerrar la aplicación.
void finalizar();

// Devuelve todas las voces instaladas en el sistema.
std::vector<VozInfo> listar_voces_disponibles();

// Devuelve las voces cuyo código de idioma coincide con `codigo_idioma`
// (p.ej. "es").
std::vector<VozInfo> listar_voces_por_idioma(const std::string &codigo_idioma);

// Aplica la voz y los parámetros de síntesis (velocidad, tono, volumen).
void aplicar_parametros(const std::string &nombre_voz, int velocidad, int tono, int volumen);

// Sintetiza `texto` y lo reproduce por el dispositivo de audio
// predeterminado. Es una llamada BLOQUEANTE: debe ejecutarse en un hilo
// secundario. `callback_estado` se invoca para notificar el progreso.
void hablar(const std::string &texto, const CallbackEstado &callback_estado);

// Sintetiza `texto` y lo vuelca a un archivo WAV en `ruta_salida` en vez
// de reproducirlo. También es bloqueante y está pensada para ejecutarse
// en un hilo secundario. Devuelve true si el archivo se escribió con éxito.
bool exportar_a_wav(const std::string &texto, const std::string &ruta_salida, const CallbackEstado &callback_estado);

// Detiene la síntesis en curso (reproducción o exportación).
void detener();

// Indica si hay una síntesis en curso actualmente.
bool esta_hablando();

// Lee un mensaje corto en voz alta de inmediato (pensado para anunciar
// mensajes de estado a usuarios con discapacidad visual). No debe usarse
// para el texto principal del usuario; para eso está hablar(). Es
// bloqueante mientras dura el mensaje: llamar desde un hilo secundario.
void leer_mensaje_estado(const std::string &mensaje);

} // namespace MotorVoz

#endif /* MOTOR_VOZ_HPP_ */
