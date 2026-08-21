/*
 * motor_voz.cpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

// Contenido principal modificado para añadir soporte PortAudio y parámetros de audio.
// Nota: requiere libportaudio (pa) en tiempo de compilación si se usa backend "portaudio".
#include "motor_voz.hpp"
#include "config.hpp"

#include <espeak-ng/speak_lib.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

namespace MotorVoz {

namespace {

std::atomic<bool> g_hablando(false);
std::atomic<bool> g_debe_detener(false);

/* Parámetros de audio configurables */
int g_sample_rate = 22050;
int g_channels = 1;
int g_audio_buffer_frames = 256;
std::string g_audio_backend = "espeak_playback";

/* Exportación a WAV (como antes) */
std::vector<short> *g_buffer_exportacion = nullptr;
std::mutex g_mutex_exportacion;

/* Para backend PortAudio: buffer de reproducción y sincronización */
std::vector<short> g_playback_buffer;
std::mutex g_playback_mutex;
std::condition_variable g_playback_cv;
std::thread g_playback_thread;
std::atomic<bool> g_playback_thread_running(false);

#ifdef HAVE_PORTAUDIO
PaStream *g_pa_stream = nullptr;
bool g_pa_initialized = false;
#endif

/* Escribe una cabecera WAV (soporta mono/stereo según g_channels) */
bool escribir_wav(const std::string &ruta, const std::vector<short> &muestras, int frecuencia, int canales) {
	std::ofstream archivo(ruta, std::ios::binary);
	if (!archivo.is_open()) {
		return false;
	}

	const uint32_t datos_bytes = static_cast<uint32_t>(muestras.size() * sizeof(short));
	const uint32_t tamano_riff = 36 + datos_bytes;
	const uint16_t canales_u16 = static_cast<uint16_t>(canales);
	const uint32_t frecuencia_u32 = static_cast<uint32_t>(frecuencia);
	const uint32_t bytes_por_segundo = frecuencia_u32 * canales_u16 * sizeof(short);
	const uint16_t bloque_align = canales_u16 * sizeof(short);
	const uint16_t bits_por_muestra = 16;
	const uint32_t tamano_fmt = 16;
	const uint16_t formato_pcm = 1;

	archivo.write("RIFF", 4);
	archivo.write(reinterpret_cast<const char*>(&tamano_riff), 4);
	archivo.write("WAVE", 4);
	archivo.write("fmt ", 4);
	archivo.write(reinterpret_cast<const char*>(&tamano_fmt), 4);
	archivo.write(reinterpret_cast<const char*>(&formato_pcm), 2);
	archivo.write(reinterpret_cast<const char*>(&canales_u16), 2);
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

/* Convierte buffer mono -> stereo duplicando muestras si channels == 2 */
static std::vector<short> convertir_a_canales(const std::vector<short> &in, int canales) {
	if (canales <= 1) {
		return in;
	}
	std::vector<short> out;
	out.reserve(in.size() * canales);
	for (size_t i = 0; i < in.size(); ++i) {
		short s = in[i];
		for (int c = 0; c < canales; ++c) {
			out.push_back(s);
		}
	}
	return out;
}

/* Callback de eSpeak: se invoca con bloques PCM 16-bit mono */
int al_recibir_muestras_sintesis(short *muestras, int cantidad, espeak_EVENT *eventos) {
	(void) eventos;
	if (g_debe_detener) {
		return 1;
	}
	// Captura para exportación si corresponde
	{
		std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
		if (g_buffer_exportacion != nullptr && muestras != nullptr && cantidad > 0) {
			g_buffer_exportacion->insert(g_buffer_exportacion->end(), muestras, muestras + cantidad);
		}
	}

	// Si estamos usando PortAudio, empujar al buffer de reproducción
	if (g_audio_backend == "portaudio" && muestras != nullptr && cantidad > 0) {
		std::lock_guard<std::mutex> lock(g_playback_mutex);
		g_playback_buffer.insert(g_playback_buffer.end(), muestras, muestras + cantidad);
		g_playback_cv.notify_one();
	}

	return 0;
}

#ifdef HAVE_PORTAUDIO
/* Hilo que consume g_playback_buffer y escribe a PortAudio */
void hilo_playback_portaudio() {
	if (!g_pa_initialized || g_pa_stream == nullptr) {
		return;
	}
	g_playback_thread_running = true;
	while (g_playback_thread_running) {
		std::vector<short> chunk;
		{
			std::unique_lock<std::mutex> lock(g_playback_mutex);
			g_playback_cv.wait(lock, []{ return !g_playback_buffer.empty() || !g_playback_thread_running; });
			if (!g_playback_thread_running) break;
			// copiar y vaciar buffer para minimizar tiempo de bloqueo
			chunk.swap(g_playback_buffer);
		}
		if (!chunk.empty()) {
			// Si configurado stereo pero eSpeak genera mono, convertir
			std::vector<short> out = convertir_a_canales(chunk, g_channels);
			// Escribir en stream (bloqueante) — se puede mejorar con ring buffer
			PaError err = Pa_WriteStream(g_pa_stream, out.data(), static_cast<unsigned long>(out.size() / g_channels));
			if (err != paNoError) {
				std::cerr << "PortAudio WriteStream error: " << Pa_GetErrorText(err) << std::endl;
				// intentar continuar; si falla gravemente, salir
			}
		}
	}
}
#endif

} // namespace

std::string codigo_idioma_base(const std::string &codigo_idioma) {
	size_t pos_guion = codigo_idioma.find('-');
	return (pos_guion == std::string::npos) ? codigo_idioma : codigo_idioma.substr(0, pos_guion);
}

bool inicializar() {
	// Inicialización por defecto: usa salida de reproducción de eSpeak
	int resultado = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
	if (resultado < 0) {
		std::cerr << "Error: no se pudo inicializar eSpeak-NG. Código: " << resultado << std::endl;
		return false;
	}
	g_sample_rate = resultado;
	espeak_SetSynthCallback(al_recibir_muestras_sintesis);
	return true;
}

void finalizar() {
	// Detener hilo playback si existe
#ifdef HAVE_PORTAUDIO
	if (g_playback_thread_running) {
		g_playback_thread_running = false;
		g_playback_cv.notify_one();
		if (g_playback_thread.joinable()) g_playback_thread.join();
	}
	if (g_pa_stream) {
		Pa_CloseStream(g_pa_stream);
		g_pa_stream = nullptr;
	}
	if (g_pa_initialized) {
		Pa_Terminate();
		g_pa_initialized = false;
	}
#endif
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
		if (!voz->name) continue;
		VozInfo info;
		info.nombre_interno = voz->identifier ? voz->identifier : voz->name;
		if (voz->languages && voz->languages[0] != '\0') {
			info.codigo_idioma = std::string(voz->languages + 1);
		}
		info.nombre_visible = std::string(voz->name) + " (" + (info.codigo_idioma.empty() ? "?" : info.codigo_idioma) + ")";
		voces.push_back(info);
	}
	return voces;
}

std::vector<VozInfo> listar_voces_por_idioma(const std::string &idioma_buscado) {
	std::vector<VozInfo> voces_filtradas;
	for (const auto &voz : listar_voces_disponibles()) {
		if (voz.codigo_idioma.empty()) continue;
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

/* Nueva: configurar desde AppConfig */
void configurar_desde_appconfig(const Configuracion::AppConfig &config) {
	g_sample_rate = config.sample_rate;
	g_channels = config.channels <= 0 ? 1 : config.channels;
	g_audio_buffer_frames = config.audio_buffer_frames > 0 ? config.audio_buffer_frames : 256;
	g_audio_backend = config.audio_backend.empty() ? std::string("espeak_playback") : config.audio_backend;

	// Si se pide backend portaudio, re-inicializar eSpeak en modo retrieval y arrancar PortAudio
	if (g_audio_backend == "portaudio") {
#ifdef HAVE_PORTAUDIO
		// Terminar eSpeak actual y re-inicializar en retrieval
		espeak_Terminate();
		int resultado = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 0, NULL, 0);
		if (resultado < 0) {
			std::cerr << "Error: no se pudo inicializar eSpeak-NG en modo retrieval. Código: " << resultado << std::endl;
		} else {
			g_sample_rate = resultado;
			espeak_SetSynthCallback(al_recibir_muestras_sintesis);
		}

		PaError err = Pa_Initialize();
		if (err != paNoError) {
			std::cerr << "Error: PortAudio no pudo inicializar: " << Pa_GetErrorText(err) << std::endl;
			g_audio_backend = "espeak_playback"; // fallback
		} else {
			g_pa_initialized = true;
			PaStreamParameters outParams;
			outParams.device = Pa_GetDefaultOutputDevice();
			if (outParams.device == paNoDevice) {
				std::cerr << "Error: no hay dispositivo de salida por defecto en PortAudio." << std::endl;
			} else {
				outParams.channelCount = g_channels;
				outParams.sampleFormat = paInt16;
				outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
				outParams.hostApiSpecificStreamInfo = NULL;
				err = Pa_OpenStream(&g_pa_stream, NULL, &outParams, static_cast<double>(g_sample_rate),
									g_audio_buffer_frames, paNoFlag, NULL, NULL);
				if (err != paNoError) {
					std::cerr << "Error: Pa_OpenStream failed: " << Pa_GetErrorText(err) << std::endl;
					Pa_Terminate();
					g_pa_initialized = false;
					g_audio_backend = "espeak_playback"; // fallback
				} else {
					err = Pa_StartStream(g_pa_stream);
					if (err != paNoError) {
						std::cerr << "Error: Pa_StartStream failed: " << Pa_GetErrorText(err) << std::endl;
						Pa_CloseStream(g_pa_stream);
						g_pa_stream = nullptr;
						Pa_Terminate();
						g_pa_initialized = false;
						g_audio_backend = "espeak_playback";
					} else {
						// arrancar hilo de playback
						g_playback_thread = std::thread(hilo_playback_portaudio);
					}
				}
			}
		}
#else
		std::cerr << "PortAudio no está disponible en esta compilación. Recompile con HAVE_PORTAUDIO definido y libportaudio instalada.\n";
		g_audio_backend = "espeak_playback";
#endif
	} else {
		// En modo espeak_playback, asegurarnos de inicializar eSpeak en playback
		espeak_Terminate();
		int resultado = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
		if (resultado >= 0) {
			g_sample_rate = resultado;
			espeak_SetSynthCallback(al_recibir_muestras_sintesis);
		}
	}
}

void hablar(const std::string &texto, const CallbackEstado &callback_estado) {
	if (g_hablando) {
		if (callback_estado) callback_estado("⚠️ Ya hay una síntesis en curso.", true);
		return;
	}
	g_hablando = true;
	g_debe_detener = false;
	{
		std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
		g_buffer_exportacion = nullptr;
	}
	if (callback_estado) callback_estado("🔊 Hablando...", false);

	const int flags = espeakCHARS_AUTO | espeakSSML;
	int resultado = espeak_Synth(texto.c_str(), texto.length() + 1, 0, POS_CHARACTER, 0, flags, NULL, NULL);
	if (resultado != EE_OK) {
		if (callback_estado) {
			callback_estado("❌ Error de síntesis. Código: " + std::to_string(resultado), true);
		}
	} else {
		espeak_Synchronize();
		if (callback_estado) {
			if (!g_debe_detener) callback_estado("✅ Síntesis completada.", true);
			else callback_estado("⏹️ Síntesis detenida.", true);
		}
	}
	g_hablando = false;
}

bool exportar_a_wav(const std::string &texto, const std::string &ruta_salida, const CallbackEstado &callback_estado) {
	if (g_hablando) {
		if (callback_estado) callback_estado("⚠️ Ya hay una síntesis en curso.", true);
		return false;
	}
	g_hablando = true;
	g_debe_detener = false;

	std::vector<short> muestras;
	{
		std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
		g_buffer_exportacion = &muestras;
	}

	if (callback_estado) callback_estado("💾 Exportando a WAV...", false);

	const int flags = espeakCHARS_AUTO | espeakSSML;
	int resultado = espeak_Synth(texto.c_str(), texto.length() + 1, 0, POS_CHARACTER, 0, flags, NULL, NULL);
	bool exito = false;
	if (resultado != EE_OK) {
		if (callback_estado) callback_estado("❌ Error de síntesis. Código: " + std::to_string(resultado), true);
	} else {
		espeak_Synchronize();
		{
			std::lock_guard<std::mutex> guardia(g_mutex_exportacion);
			g_buffer_exportacion = nullptr;
		}
		// Si se configuró stereo y eSpeak generó mono, duplicar
		std::vector<short> out = convertir_a_canales(muestras, g_channels);
		exito = escribir_wav(ruta_salida, out, g_sample_rate, g_channels);
		if (callback_estado) {
			if (exito) callback_estado("✅ Archivo WAV exportado correctamente.", true);
			else callback_estado("❌ Error al escribir el archivo WAV.", true);
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
	std::string mensaje_limpio = mensaje;
	size_t pos = mensaje_limpio.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	if (pos != std::string::npos) {
		mensaje_limpio = mensaje_limpio.substr(pos);
	}
	if (mensaje_limpio.empty()) return;
	espeak_Synth(mensaje_limpio.c_str(), mensaje_limpio.length() + 1, 0, POS_CHARACTER, 0, espeakCHARS_AUTO, NULL, NULL);
	espeak_Synchronize();
}

} // namespace MotorVoz
