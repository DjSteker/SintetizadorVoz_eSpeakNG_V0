/*
 * motor_voz.cpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#include "motor_voz.hpp"

#include <espeak-ng/speak_lib.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>

namespace MotorVoz {

namespace {

std::atomic<bool> g_hablando(false);
std::atomic<bool> g_debe_detener(false);
int g_frecuencia_muestreo = 22050;

// Buffer donde se capturan las muestras PCM cuando se está exportando a
// WAV. Cuando vale nullptr, la síntesis en curso es una reproducción
// normal y las muestras no se capturan.
std::vector<short> *g_buffer_exportacion = nullptr;
std::mutex g_mutex_exportacion;

// Escribe una cabecera WAV (PCM, mono, 16 bits) seguida de las muestras
// capturadas durante una síntesis.
bool escribir_wav(const std::string &ruta, const std::vector<short> &muestras, int frecuencia) {
	std::ofstream archivo(ruta, std::ios::binary);
	if (!archivo.is_open()) {
		return false;
	}

	const uint32_t datos_bytes = static_cast<uint32_t>(muestras.size() * sizeof(short));
	const uint32_t tamano_riff = 36 + datos_bytes;
	const uint16_t canales = 1;
	const uint32_t frecuencia_u32 = static_cast<uint32_t>(frecuencia);
	const uint32_t bytes_por_segundo = frecuencia_u32 * canales * sizeof(short);
	const uint16_t bloque_align = canales * sizeof(short);
	const uint16_t bits_por_muestra = 16;
	const uint32_t tamano_fmt = 16;
	const uint16_t formato_pcm = 1;

	archivo.write("RIFF", 4);
	archivo.write(reinterpret_cast<const char*>(&tamano_riff), 4);
	archivo.write("WAVE", 4);
	archivo.write("fmt ", 4);
	archivo.write(reinterpret_cast<const char*>(&tamano_fmt), 4);
	archivo.write(reinterpret_cast<const char*>(&formato_pcm), 2);
	archivo.write(reinterpret_cast<const char*>(&canales), 2);
	archivo.write(reinterpret_cast<const char*>(&frecuencia_u32), 4);
	archivo.write(reinterpret_cast<const char*>(&bytes_por_segundo), 4);
	archivo.write(reinterpret_cast<const char*>(&bloque_align), 2);
	archivo.write(reinterpret_cast<const char*>(&bits_por_muestra), 2);
	archivo.write("data", 4);
	archivo.write(reinterpret_cast<const char*>(&datos_bytes), 4);
	if (!muestras.empty()) {
		archivo.write(reinterpret_cast<const char*>(muestras.data()), datos_bytes);
	}
	return archivo.good();
}

// Callback de síntesis de eSpeak-NG: se invoca repetidamente con bloques
// de muestras PCM de 16 bits mientras se sintetiza el texto. Cuando hay
// una exportación a WAV en curso, acumula las muestras en el buffer
// correspondiente; en reproducción normal no hace nada más que permitir
// que eSpeak-NG continúe reproduciendo por su cuenta.
int al_recibir_muestras_sintesis(short *muestras, int cantidad, espeak_EVENT *eventos) {
	(void) eventos;
	if (g_debe_detener) {
		return 1; // Un valor distinto de 0 solicita a eSpeak-NG detener la síntesis.
	}
	std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
	if (g_buffer_exportacion != nullptr && muestras != nullptr && cantidad > 0) {
		g_buffer_exportacion->insert(g_buffer_exportacion->end(), muestras, muestras + cantidad);
	}
	return 0;
}

// Extrae el código de idioma real de un espeak_VOICE. El campo
// `languages` NO es un string plano: son pares de (1 byte de prioridad +
// código de idioma terminado en \0), repetidos si la voz cubre varios
// idiomas. Aquí solo se toma el primer par, saltando el byte de
// prioridad inicial.
std::string extraer_codigo_idioma(const espeak_VOICE *voz) {
	if (!voz->languages || voz->languages[0] == '\0') {
		return "";
	}
	return std::string(voz->languages + 1);
}

} // namespace

std::string codigo_idioma_base(const std::string &codigo_idioma) {
	size_t pos_guion = codigo_idioma.find('-');
	return (pos_guion == std::string::npos) ? codigo_idioma : codigo_idioma.substr(0, pos_guion);
}

bool inicializar() {
	int resultado = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
	if (resultado < 0) {
		std::cerr << "Error: no se pudo inicializar eSpeak-NG. Código: " << resultado << std::endl;
		return false;
	}
	g_frecuencia_muestreo = resultado;
	espeak_SetSynthCallback(al_recibir_muestras_sintesis);
	return true;
}

void finalizar() {
	espeak_Terminate();
}

std::vector<VozInfo> listar_voces_disponibles() {
	std::vector<VozInfo> voces;
	const espeak_VOICE **voces_espeak = espeak_ListVoices(NULL);
	if (!voces_espeak) {
		std::cerr << "Error: no se pudieron obtener las voces de eSpeak-NG." << std::endl;
		return voces;
	}
	for (const espeak_VOICE **voz_ptr = voces_espeak; *voz_ptr; ++voz_ptr) {
		const espeak_VOICE *voz = *voz_ptr;
		if (!voz->name) {
			continue;
		}
		VozInfo info;
		info.nombre_interno = voz->identifier ? voz->identifier : voz->name;
		info.codigo_idioma = extraer_codigo_idioma(voz);
		info.nombre_visible = std::string(voz->name) + " (" + (info.codigo_idioma.empty() ? "?" : info.codigo_idioma) + ")";
		voces.push_back(info);
	}
	return voces;
}

std::vector<VozInfo> listar_voces_por_idioma(const std::string &idioma_buscado) {
	std::vector<VozInfo> voces_filtradas;
	for (const auto &voz : listar_voces_disponibles()) {
		if (voz.codigo_idioma.empty()) {
			continue;
		}
		if (codigo_idioma_base(voz.codigo_idioma) == idioma_buscado) {
			voces_filtradas.push_back(voz);
		}
	}
	return voces_filtradas;
}

void aplicar_parametros(const std::string &nombre_voz, int velocidad, int tono, int volumen) {
	espeak_SetVoiceByName(nombre_voz.c_str());
	espeak_SetParameter(espeakRATE, velocidad, 0);
	espeak_SetParameter(espeakPITCH, tono, 0);
	espeak_SetParameter(espeakVOLUME, volumen, 0);
}

void hablar(const std::string &texto, const CallbackEstado &callback_estado) {
	if (g_hablando) {
		if (callback_estado) {
			callback_estado("⚠️ Ya hay una síntesis en curso.", true);
		}
		return;
	}
	g_hablando = true;
	g_debe_detener = false;
	{
		std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
		g_buffer_exportacion = nullptr; // Reproducción normal: no se capturan muestras.
	}

	if (callback_estado) {
		callback_estado("🔊 Hablando...", false);
	}

	const int flags = espeakCHARS_AUTO | espeakSSML;
	int resultado = espeak_Synth(texto.c_str(), texto.length() + 1, 0, POS_CHARACTER, 0, flags, NULL, NULL);
	if (resultado != EE_OK) {
		if (callback_estado) {
			callback_estado("❌ Error de síntesis. Código: " + std::to_string(resultado), true);
		}
	} else {
		espeak_Synchronize();
		if (callback_estado) {
			if (!g_debe_detener) {
				callback_estado("✅ Síntesis completada.", true);
			} else {
				callback_estado("⏹️ Síntesis detenida.", true);
			}
		}
	}
	g_hablando = false;
}

bool exportar_a_wav(const std::string &texto, const std::string &ruta_salida, const CallbackEstado &callback_estado) {
	if (g_hablando) {
		if (callback_estado) {
			callback_estado("⚠️ Ya hay una síntesis en curso.", true);
		}
		return false;
	}
	g_hablando = true;
	g_debe_detener = false;

	std::vector<short> muestras;
	{
		std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
		g_buffer_exportacion = &muestras;
	}

	if (callback_estado) {
		callback_estado("💾 Exportando a WAV...", false);
	}

	const int flags = espeakCHARS_AUTO | espeakSSML;
	int resultado = espeak_Synth(texto.c_str(), texto.length() + 1, 0, POS_CHARACTER, 0, flags, NULL, NULL);
	bool exito = false;
	if (resultado != EE_OK) {
		if (callback_estado) {
			callback_estado("❌ Error de síntesis. Código: " + std::to_string(resultado), true);
		}
	} else {
		espeak_Synchronize();
		{
			std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
			g_buffer_exportacion = nullptr;
		}
		exito = escribir_wav(ruta_salida, muestras, g_frecuencia_muestreo);
		if (callback_estado) {
			if (exito) {
				callback_estado("✅ Archivo WAV exportado correctamente.", true);
			} else {
				callback_estado("❌ Error al escribir el archivo WAV.", true);
			}
		}
	}
	g_hablando = false;
	return exito;
}

void detener() {
	if (g_hablando) {
		g_debe_detener = true;
		espeak_Cancel();
	}
}

bool esta_hablando() {
	return g_hablando;
}

void leer_mensaje_estado(const std::string &mensaje) {
	// Se elimina cualquier emoji/símbolo inicial para que la síntesis
	// suene natural (eSpeak-NG no sabe pronunciar la mayoría de emojis).
	std::string mensaje_limpio = mensaje;
	size_t pos = mensaje_limpio.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	if (pos != std::string::npos) {
		mensaje_limpio = mensaje_limpio.substr(pos);
	}
	if (mensaje_limpio.empty()) {
		return;
	}
	espeak_Synth(mensaje_limpio.c_str(), mensaje_limpio.length() + 1, 0, POS_CHARACTER, 0,
	espeakCHARS_AUTO, NULL, NULL);
	espeak_Synchronize();
}

} // namespace MotorVoz
