/*
 * interfaz_grafica.cpp
 *
 *  Created on: 28 jul 2026
 *      Author: usuario001
 */

#include "interfaz_grafica.hpp"

#include "config.hpp"
#include "lector_archivos.hpp"
#include "motor_voz.hpp"

#include <cctype>
#include <iostream>
#include <thread>
#include <vector>

namespace InterfazGrafica {
namespace {

// --- Estado global de la interfaz ---
// Mantenido como en el resto de aplicaciones GTK del autor: un único
// struct con los widgets y datos que necesitan compartir los manejadores
// de señales, ya que estos deben tener la firma que exige GLib.
struct EstadoInterfaz {
	GtkTextView *vista_texto = nullptr;
	GtkTextBuffer *buffer_texto = nullptr;
	GtkLabel *etiqueta_estado = nullptr;
	GtkWindow *ventana_principal = nullptr;
	GtkDropDown *desplegable_variante_voz = nullptr;
	Configuracion::AppConfig config;
	std::vector<MotorVoz::VozInfo> voces_disponibles;
	std::vector<MotorVoz::VozInfo> voces_idioma_actual;
};

EstadoInterfaz g_estado;

enum class TipoParametro {
	Velocidad, Tono,
};

// --- Declaraciones adelantadas (definidas más abajo, en orden lógico) ---
void actualizar_estado(const std::string &mensaje, bool leer_en_voz_alta = false);
void actualizar_desplegable_variantes(const std::string &codigo_idioma, const std::string &voz_preferida = "");
GtkWidget* crear_desplegable_configuracion(const std::vector<std::string> &opciones, const std::string &valor_por_defecto, GCallback manejador, GtkWidget *caja_padre,
		gpointer datos_usuario = nullptr);
void al_pulsar_hablar(GtkWidget *widget, gpointer datos_usuario);
void al_pulsar_detener(GtkWidget *widget, gpointer datos_usuario);
void al_pulsar_abrir_archivo(GtkWidget *widget, gpointer datos_usuario);
void al_pulsar_exportar_wav(GtkWidget *widget, gpointer datos_usuario);
void al_cambiar_variante_voz(GtkDropDown *desplegable, GParamSpec *pspec, gpointer datos_usuario);
void aplicar_y_guardar_voz_seleccionada(bool anunciar);

// --- Notificación de estado desde hilos secundarios ---

struct DatosActualizacionEstado {
	std::string mensaje;
	bool leer_en_voz_alta;
};

gboolean actualizar_estado_en_hilo_principal(gpointer datos) {
	auto *info = static_cast<DatosActualizacionEstado*>(datos);
	if (g_estado.etiqueta_estado) {
		gtk_label_set_text(g_estado.etiqueta_estado, info->mensaje.c_str());
	}
	if (info->leer_en_voz_alta && g_estado.config.leer_estado && !MotorVoz::esta_hablando()) {
		std::thread(MotorVoz::leer_mensaje_estado, info->mensaje).detach();
	}
	delete info;
	return G_SOURCE_REMOVE;
}

// Actualiza la etiqueta de estado de forma segura desde cualquier hilo.
// Si `leer_en_voz_alta` es true y la opción de accesibilidad está
// activada, el mensaje también se anuncia por voz.
void actualizar_estado(const std::string &mensaje, bool leer_en_voz_alta) {
	auto *datos = new DatosActualizacionEstado { mensaje, leer_en_voz_alta };
	g_idle_add(actualizar_estado_en_hilo_principal, datos);
}

// --- Apertura de archivos ---

void al_completar_apertura_archivo(GObject *origen, GAsyncResult *resultado, gpointer datos_usuario) {
	(void) datos_usuario;
	GtkFileDialog *dialogo = GTK_FILE_DIALOG(origen);
	GError *error = nullptr;
	GFile *archivo = gtk_file_dialog_open_finish(dialogo, resultado, &error);
	if (archivo) {
		char *ruta = g_file_get_path(archivo);
		LectorArchivos::ResultadoLectura lectura = LectorArchivos::leer_archivo_texto(ruta);
		if (lectura.exito) {
			gtk_text_buffer_set_text(g_estado.buffer_texto, lectura.contenido.c_str(), -1);
			actualizar_estado("📄 Archivo cargado correctamente.", true);
		} else {
			std::cerr << lectura.mensaje_error << std::endl;
			actualizar_estado("❌ Error al abrir el archivo.", true);
		}
		g_free(ruta);
		g_object_unref(archivo);
	} else if (error) {
		if (error->code != GTK_DIALOG_ERROR_DISMISSED) {
			std::cerr << "Error al abrir archivo: " << error->message << std::endl;
			actualizar_estado("❌ Error al seleccionar archivo.", true);
		}
		g_error_free(error);
	}
}

// --- Exportación a WAV ---

void al_completar_guardado_wav(GObject *origen, GAsyncResult *resultado, gpointer datos_usuario) {
	(void) datos_usuario;
	GtkFileDialog *dialogo = GTK_FILE_DIALOG(origen);
	GError *error = nullptr;
	GFile *archivo = gtk_file_dialog_save_finish(dialogo, resultado, &error);
	if (archivo) {
		char *ruta = g_file_get_path(archivo);
		std::string ruta_str(ruta);
		g_free(ruta);
		g_object_unref(archivo);

		GtkTextIter inicio, fin;
		gtk_text_buffer_get_bounds(g_estado.buffer_texto, &inicio, &fin);
		char *texto = gtk_text_buffer_get_text(g_estado.buffer_texto, &inicio, &fin, FALSE);
		std::string texto_str(texto);
		g_free(texto);

		if (texto_str.empty()) {
			actualizar_estado("⚠️ No hay texto para exportar.", true);
			return;
		}
		std::thread(MotorVoz::exportar_a_wav, texto_str, ruta_str, actualizar_estado).detach();
	} else if (error) {
		if (error->code != GTK_DIALOG_ERROR_DISMISSED) {
			std::cerr << "Error al guardar archivo WAV: " << error->message << std::endl;
			actualizar_estado("❌ Error al seleccionar destino.", true);
		}
		g_error_free(error);
	}
}

// --- Manejadores de botones ---

void al_pulsar_hablar(GtkWidget *widget, gpointer datos_usuario) {
	(void) widget;
	(void) datos_usuario;
	GtkTextIter inicio, fin;
	gtk_text_buffer_get_bounds(g_estado.buffer_texto, &inicio, &fin);
	char *texto = gtk_text_buffer_get_text(g_estado.buffer_texto, &inicio, &fin, FALSE);
	std::string texto_str(texto);
	g_free(texto);
	if (texto_str.empty()) {
		actualizar_estado("⚠️ No hay texto para sintetizar.", true);
		return;
	}
	std::thread(MotorVoz::hablar, texto_str, actualizar_estado).detach();
}

void al_pulsar_detener(GtkWidget *widget, gpointer datos_usuario) {
	(void) widget;
	(void) datos_usuario;
	if (MotorVoz::esta_hablando()) {
		MotorVoz::detener();
	} else {
		actualizar_estado("ℹ️ No hay síntesis en curso para detener.", true);
	}
}

void al_pulsar_limpiar(GtkWidget *widget, gpointer datos_usuario) {
	(void) widget;
	(void) datos_usuario;
	gtk_text_buffer_set_text(g_estado.buffer_texto, "", -1);
	actualizar_estado("🗑️ Texto borrado.", true);
}

void al_pulsar_abrir_archivo(GtkWidget *widget, gpointer datos_usuario) {
	(void) widget;
	(void) datos_usuario;
	GtkFileDialog *dialogo = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialogo, "Abrir archivo de texto");
	GtkFileFilter *filtro = gtk_file_filter_new();
	gtk_file_filter_set_name(filtro, "Archivos de texto");
	gtk_file_filter_add_mime_type(filtro, "text/plain");
	gtk_file_filter_add_pattern(filtro, "*.txt");
	GListStore *filtros = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filtros, filtro);
	gtk_file_dialog_set_filters(dialogo, G_LIST_MODEL(filtros));
	g_object_unref(filtros);
	g_object_unref(filtro);
	gtk_file_dialog_open(dialogo, g_estado.ventana_principal, NULL, al_completar_apertura_archivo, NULL);
	g_object_unref(dialogo);
}

void al_pulsar_exportar_wav(GtkWidget *widget, gpointer datos_usuario) {
	(void) widget;
	(void) datos_usuario;
	if (MotorVoz::esta_hablando()) {
		actualizar_estado("⚠️ Ya hay una síntesis en curso.", true);
		return;
	}
	GtkFileDialog *dialogo = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialogo, "Exportar a archivo WAV");
	gtk_file_dialog_set_initial_name(dialogo, "sintesis.wav");
	gtk_file_dialog_save(dialogo, g_estado.ventana_principal, NULL, al_completar_guardado_wav, NULL);
	g_object_unref(dialogo);
}

// --- Manejadores de los desplegables de voz e idioma ---

// Aplica y guarda la voz actualmente seleccionada en el desplegable de
// variantes: la fija en el motor de síntesis, la persiste en la
// configuración y, opcionalmente, la anuncia en la etiqueta de estado.
// Se llama tanto tras una interacción manual del usuario como tras
// repoblar el desplegable de variantes de forma programática, para que
// CUALQUIER cambio de idioma o voz quede siempre aplicado y guardado.
void aplicar_y_guardar_voz_seleccionada(bool anunciar) {
	guint seleccionado = gtk_drop_down_get_selected(g_estado.desplegable_variante_voz);
	if (seleccionado == GTK_INVALID_LIST_POSITION || seleccionado >= g_estado.voces_idioma_actual.size()) {
		return;
	}
	const auto &voz = g_estado.voces_idioma_actual[seleccionado];
	g_estado.config.voz_actual = voz.nombre_interno;
	g_estado.config.codigo_idioma = MotorVoz::codigo_idioma_base(voz.codigo_idioma);
	MotorVoz::aplicar_parametros(g_estado.config.voz_actual, g_estado.config.velocidad, g_estado.config.tono, g_estado.config.volumen);
	if (anunciar) {
		actualizar_estado("🎤 Voz seleccionada: " + voz.nombre_visible, true);
	}
	if (!Configuracion::guardar_configuracion(g_estado.config)) {
		actualizar_estado("⚠️ No se pudo guardar la configuración en disco.", true);
	}
}

// Rellena el desplegable de variantes para el idioma base `codigo_idioma`
// (p.ej. "es"). Si `voz_preferida` coincide con alguna de las variantes
// encontradas, esa es la que queda seleccionada; si no, se selecciona la
// primera. La señal del desplegable se bloquea mientras se repuebla para
// no disparar el manejador dos veces (una aquí y otra al reconstruir el
// modelo); el llamador es responsable de invocar
// aplicar_y_guardar_voz_seleccionada() justo después.
void actualizar_desplegable_variantes(const std::string &codigo_idioma, const std::string &voz_preferida) {
	g_estado.voces_idioma_actual = MotorVoz::listar_voces_por_idioma(codigo_idioma);
	if (g_estado.voces_idioma_actual.empty()) {
		gtk_drop_down_set_model(g_estado.desplegable_variante_voz, NULL);
		actualizar_estado("⚠️ No se encontraron variantes de voz para el idioma '" + codigo_idioma + "'.", true);
		return;
	}

	GtkStringList *lista_cadenas = gtk_string_list_new(NULL);
	guint indice_preferido = 0;
	for (guint i = 0; i < g_estado.voces_idioma_actual.size(); ++i) {
		const auto &voz = g_estado.voces_idioma_actual[i];
		gtk_string_list_append(lista_cadenas, voz.nombre_visible.c_str());
		if (!voz_preferida.empty() && voz.nombre_interno == voz_preferida) {
			indice_preferido = i;
		}
	}

	g_signal_handlers_block_by_func(g_estado.desplegable_variante_voz, reinterpret_cast<gpointer>(al_cambiar_variante_voz), NULL);
	gtk_drop_down_set_model(g_estado.desplegable_variante_voz, G_LIST_MODEL(lista_cadenas));
	gtk_drop_down_set_selected(g_estado.desplegable_variante_voz, indice_preferido);
	g_signal_handlers_unblock_by_func(g_estado.desplegable_variante_voz, reinterpret_cast<gpointer>(al_cambiar_variante_voz), NULL);
}

void al_cambiar_idioma(GtkDropDown *desplegable, GParamSpec *pspec, gpointer datos_usuario) {
	(void) pspec;
	(void) datos_usuario;
	guint seleccionado = gtk_drop_down_get_selected(desplegable);
	if (seleccionado == GTK_INVALID_LIST_POSITION) {
		return;
	}
	GtkStringObject *objeto = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(desplegable));
	const char *nombre_visible = gtk_string_object_get_string(objeto);
	if (!nombre_visible) {
		return;
	}
	for (const auto &voz : g_estado.voces_disponibles) {
		if (voz.nombre_visible == nombre_visible) {
			std::string idioma_base = MotorVoz::codigo_idioma_base(voz.codigo_idioma);
			actualizar_desplegable_variantes(idioma_base);
			aplicar_y_guardar_voz_seleccionada(true);
			break;
		}
	}
}

void al_cambiar_variante_voz(GtkDropDown *desplegable, GParamSpec *pspec, gpointer datos_usuario) {
	(void) desplegable;
	(void) pspec;
	(void) datos_usuario;
	aplicar_y_guardar_voz_seleccionada(true);
}

// --- Manejadores de velocidad y tono ---

void al_cambiar_parametro(GtkDropDown *desplegable, GParamSpec *pspec, gpointer datos_usuario) {
	(void) pspec;
	TipoParametro tipo = static_cast<TipoParametro>(GPOINTER_TO_INT(datos_usuario));
	guint seleccionado = gtk_drop_down_get_selected(desplegable);
	if (seleccionado == GTK_INVALID_LIST_POSITION) {
		return;
	}
	GtkStringObject *objeto = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(desplegable));
	const char *texto_seleccionado = gtk_string_object_get_string(objeto);
	if (!texto_seleccionado) {
		return;
	}
	try {
		int valor = std::stoi(texto_seleccionado);
		std::string mensaje_estado;
		if (tipo == TipoParametro::Velocidad) {
			g_estado.config.velocidad = valor;
			mensaje_estado = "⚡ Velocidad: " + std::to_string(valor) + " WPM";
		} else {
			g_estado.config.tono = valor;
			mensaje_estado = "🎵 Tono: " + std::to_string(valor);
		}
		MotorVoz::aplicar_parametros(g_estado.config.voz_actual, g_estado.config.velocidad, g_estado.config.tono, g_estado.config.volumen);
		actualizar_estado(mensaje_estado, true);
		Configuracion::guardar_configuracion(g_estado.config);
	} catch (const std::exception &e) {
		std::cerr << "Error al interpretar el parámetro: " << e.what() << std::endl;
	}
}

void al_cambiar_volumen(GtkRange *rango, gpointer datos_usuario) {
	(void) datos_usuario;
	int volumen = static_cast<int>(gtk_range_get_value(rango));
	g_estado.config.volumen = volumen;
	MotorVoz::aplicar_parametros(g_estado.config.voz_actual, g_estado.config.velocidad, g_estado.config.tono, g_estado.config.volumen);
	actualizar_estado("🔊 Volumen: " + std::to_string(volumen));
	Configuracion::guardar_configuracion(g_estado.config);
}

void al_cambiar_lectura_estado(GtkCheckButton *boton, gpointer datos_usuario) {
	(void) datos_usuario;
	g_estado.config.leer_estado = gtk_check_button_get_active(boton);
	if (g_estado.config.leer_estado) {
		actualizar_estado("✔️ Lectura de estado activada.", true);
	} else {
		actualizar_estado("❌ Lectura de estado desactivada.", false);
	}
	Configuracion::guardar_configuracion(g_estado.config);
}

// --- Atajos de teclado ---

gboolean al_pulsar_tecla(GtkEventControllerKey *controlador, guint tecla, guint codigo_tecla, GdkModifierType estado, gpointer datos_usuario) {
	(void) controlador;
	(void) codigo_tecla;
	(void) datos_usuario;
	if ((estado & GDK_CONTROL_MASK) && (tecla == GDK_KEY_Return || tecla == GDK_KEY_KP_Enter)) {
		al_pulsar_hablar(nullptr, nullptr);
		return TRUE;
	}
	if ((estado & GDK_CONTROL_MASK) && (estado & GDK_SHIFT_MASK) && tecla == GDK_KEY_S) {
		al_pulsar_exportar_wav(nullptr, nullptr);
		return TRUE;
	}
	if ((estado & GDK_CONTROL_MASK) && tecla == GDK_KEY_s) {
		al_pulsar_detener(nullptr, nullptr);
		return TRUE;
	}
	if ((estado & GDK_CONTROL_MASK) && tecla == GDK_KEY_o) {
		al_pulsar_abrir_archivo(nullptr, nullptr);
		return TRUE;
	}
	return FALSE;
}

// --- Construcción de controles ---

GtkWidget* crear_desplegable_configuracion(const std::vector<std::string> &opciones, const std::string &valor_por_defecto, GCallback manejador, GtkWidget *caja_padre, gpointer datos_usuario) {
	GtkStringList *lista_cadenas = gtk_string_list_new(NULL);
	guint indice_por_defecto = 0;
	for (guint i = 0; i < opciones.size(); ++i) {
		gtk_string_list_append(lista_cadenas, opciones[i].c_str());
		if (opciones[i].find(valor_por_defecto) != std::string::npos) {
			indice_por_defecto = i;
		}
	}
	GtkDropDown *desplegable = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(lista_cadenas), NULL));
	gtk_drop_down_set_selected(desplegable, indice_por_defecto);
	g_signal_connect(desplegable, "notify::selected", manejador, datos_usuario);
	gtk_box_append(GTK_BOX(caja_padre), GTK_WIDGET(desplegable));
	return GTK_WIDGET(desplegable);
}

void aplicar_estilos_css() {
	GtkCssProvider *proveedor_css = gtk_css_provider_new();
	gtk_css_provider_load_from_string(proveedor_css, "textview { font-size: 14pt; }"
			"textview text { padding: 10px; }"

			"button.pill { min-height: 40px; font-size: 12pt; border-radius: 20px; padding: 8px 16px; }"

			"button.suggested-action { background-image: linear-gradient(to bottom, #4CAF50, #45a049); "
			"color: white; border: none; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }"
			"button.suggested-action:hover { background-image: linear-gradient(to bottom, #5CBF60, #4CAF50); }"
			"button.suggested-action label { color: white; font-weight: bold; }"

			"button.destructive-action { background-image: linear-gradient(to bottom, #f44336, #e53935); "
			"color: white; border: none; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }"
			"button.destructive-action:hover { background-image: linear-gradient(to bottom, #ff5545, #f44336); }"
			"button.destructive-action label { color: white; font-weight: bold; }"

			"button.pill:not(.suggested-action):not(.destructive-action) { "
			"background-image: linear-gradient(to bottom, #e0e0e0, #d0d0d0); color: #333; "
			"border: 1px solid #999; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }"
			"button.pill:not(.suggested-action):not(.destructive-action):hover { "
			"background-image: linear-gradient(to bottom, #f0f0f0, #e0e0e0); }"
			"button.pill:not(.suggested-action):not(.destructive-action) label { color: #333; font-weight: 600; }"

			"label.title-1 { font-size: 18pt; font-weight: bold; color: #2c3e50; }");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(proveedor_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(proveedor_css);
}

} // namespace

void activar(GtkApplication *app, gpointer datos_usuario) {
	(void) datos_usuario;
	bool config_existia = Configuracion::existe_configuracion_guardada();
	g_estado.config = Configuracion::cargar_configuracion();
	g_estado.voces_disponibles = MotorVoz::listar_voces_disponibles();
	if (g_estado.voces_disponibles.empty()) {
		actualizar_estado("⚠️ No se encontraron voces instaladas. Usando valores por defecto.", true);
		g_estado.voces_disponibles.push_back( { "Español (España) [Fallback]", "es", "es" });
		g_estado.config.voz_actual = "es";
		g_estado.config.codigo_idioma = "es";
	} else if (!config_existia) {
		// Primera ejecución (todavía no hay tts_config.txt guardado): se
		// intenta partir del idioma configurado en el sistema en lugar
		// del valor por defecto fijo.
		std::string codigo_idioma_sistema = Configuracion::detectar_idioma_sistema();
		if (!codigo_idioma_sistema.empty()) {
			for (const auto &voz : g_estado.voces_disponibles) {
				if (MotorVoz::codigo_idioma_base(voz.codigo_idioma) == codigo_idioma_sistema) {
					g_estado.config.voz_actual = voz.nombre_interno;
					g_estado.config.codigo_idioma = codigo_idioma_sistema;
					actualizar_estado("🌍 Idioma del sistema detectado: " + codigo_idioma_sistema, false);
					break;
				}
			}
		}
	}

	aplicar_estilos_css();

	GtkWidget *ventana = gtk_application_window_new(app);
	g_estado.ventana_principal = GTK_WINDOW(ventana);
	gtk_window_set_title(g_estado.ventana_principal, "🎙️ eSpeak-NG - Sintetizador de Voz Accesible");
	gtk_window_set_default_size(g_estado.ventana_principal, 800, 650);

	GtkWidget *caja_principal = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
	gtk_widget_set_margin_start(caja_principal, 20);
	gtk_widget_set_margin_end(caja_principal, 20);
	gtk_widget_set_margin_top(caja_principal, 20);
	gtk_widget_set_margin_bottom(caja_principal, 20);
	gtk_window_set_child(g_estado.ventana_principal, caja_principal);

	GtkWidget *etiqueta_titulo = gtk_label_new("🎙️ Conversor de Texto a Voz");
	gtk_widget_add_css_class(etiqueta_titulo, "title-1");
	gtk_box_append(GTK_BOX(caja_principal), etiqueta_titulo);

	GtkWidget *rejilla_config = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(rejilla_config), 10);
	gtk_grid_set_row_spacing(GTK_GRID(rejilla_config), 10);
	gtk_box_append(GTK_BOX(caja_principal), rejilla_config);

	// Fila 0: idioma y variante de voz
	gtk_grid_attach(GTK_GRID(rejilla_config), gtk_label_new("🌍 Idioma:"), 0, 0, 1, 1);
	GtkWidget *caja_voz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	std::vector<std::string> nombres_voces;
	for (const auto &voz : g_estado.voces_disponibles) {
		nombres_voces.push_back(voz.nombre_visible);
	}
	size_t indice_voz_defecto = 0;
	bool coincidencia_exacta = false;
	for (size_t i = 0; i < g_estado.voces_disponibles.size(); ++i) {
		if (g_estado.voces_disponibles[i].nombre_interno == g_estado.config.voz_actual) {
			indice_voz_defecto = i;
			coincidencia_exacta = true;
			break;
		}
	}
	if (!coincidencia_exacta && !g_estado.config.codigo_idioma.empty()) {
		for (size_t i = 0; i < g_estado.voces_disponibles.size(); ++i) {
			if (MotorVoz::codigo_idioma_base(g_estado.voces_disponibles[i].codigo_idioma) == g_estado.config.codigo_idioma) {
				indice_voz_defecto = i;
				break;
			}
		}
	}
	crear_desplegable_configuracion(nombres_voces, nombres_voces[indice_voz_defecto], G_CALLBACK(al_cambiar_idioma), caja_voz);
	gtk_grid_attach(GTK_GRID(rejilla_config), caja_voz, 1, 0, 1, 1);

	gtk_grid_attach(GTK_GRID(rejilla_config), gtk_label_new("🎤 Variante de voz:"), 2, 0, 1, 1);
	GtkWidget *caja_variante_voz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	g_estado.desplegable_variante_voz = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
	gtk_box_append(GTK_BOX(caja_variante_voz), GTK_WIDGET(g_estado.desplegable_variante_voz));
	gtk_grid_attach(GTK_GRID(rejilla_config), caja_variante_voz, 3, 0, 1, 1);
	g_signal_connect(g_estado.desplegable_variante_voz, "notify::selected", G_CALLBACK(al_cambiar_variante_voz), NULL);

	// El desplegable de idioma ya se creó con una selección inicial, pero
	// como la señal "notify::selected" se conecta DESPUÉS de fijarla, no
	// se dispara sola: hay que rellenar aquí el desplegable de variantes
	// a mano, y aplicar/guardar la voz resultante, para que la app arranque
	// ya con el idioma y la voz correctos (no solo el primero de la lista).
	std::string idioma_inicial = !g_estado.config.codigo_idioma.empty() ? g_estado.config.codigo_idioma : MotorVoz::codigo_idioma_base(g_estado.voces_disponibles[indice_voz_defecto].codigo_idioma);
	actualizar_desplegable_variantes(idioma_inicial, g_estado.config.voz_actual);
	aplicar_y_guardar_voz_seleccionada(false);

	// Fila 1: velocidad y tono
	gtk_grid_attach(GTK_GRID(rejilla_config), gtk_label_new("⚡ Velocidad:"), 0, 1, 1, 1);
	GtkWidget *caja_velocidad = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	std::vector<std::string> velocidades = { "100", "140", "175", "220", "280" };
	crear_desplegable_configuracion(velocidades, std::to_string(g_estado.config.velocidad), G_CALLBACK(al_cambiar_parametro), caja_velocidad,
			GINT_TO_POINTER(static_cast<int>(TipoParametro::Velocidad)));
	gtk_grid_attach(GTK_GRID(rejilla_config), caja_velocidad, 1, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(rejilla_config), gtk_label_new("🎵 Tono:"), 2, 1, 1, 1);
	GtkWidget *caja_tono = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	std::vector<std::string> tonos = { "20", "50", "70", "90" };
	crear_desplegable_configuracion(tonos, std::to_string(g_estado.config.tono), G_CALLBACK(al_cambiar_parametro), caja_tono, GINT_TO_POINTER(static_cast<int>(TipoParametro::Tono)));
	gtk_grid_attach(GTK_GRID(rejilla_config), caja_tono, 3, 1, 1, 1);

	// Fila 2: volumen y accesibilidad
	GtkWidget *etiqueta_volumen = gtk_label_new("🔊 Volumen (0-99):");
	gtk_widget_set_halign(etiqueta_volumen, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(rejilla_config), etiqueta_volumen, 0, 2, 1, 1);
	GtkWidget *control_volumen = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 99.0, 1.0);
	gtk_range_set_value(GTK_RANGE(control_volumen), static_cast<double>(g_estado.config.volumen));
	gtk_widget_set_hexpand(control_volumen, TRUE);
	gtk_scale_set_draw_value(GTK_SCALE(control_volumen), TRUE);
	g_signal_connect(control_volumen, "value-changed", G_CALLBACK(al_cambiar_volumen), NULL);
	gtk_grid_attach(GTK_GRID(rejilla_config), control_volumen, 1, 2, 3, 1);

	GtkWidget *casilla_lectura_estado = gtk_check_button_new_with_label("📣 Leer mensajes de estado");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(casilla_lectura_estado), g_estado.config.leer_estado);
	gtk_widget_set_tooltip_text(casilla_lectura_estado, "Si está marcado, la aplicación leerá en voz alta los mensajes de éxito/error.");
	g_signal_connect(casilla_lectura_estado, "toggled", G_CALLBACK(al_cambiar_lectura_estado), NULL);
	gtk_widget_set_halign(casilla_lectura_estado, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(rejilla_config), casilla_lectura_estado, 4, 2, 2, 1);

	// Área de texto
	GtkWidget *etiqueta_texto = gtk_label_new("📝 Texto a sintetizar (Ctrl+Enter para hablar):");
	gtk_widget_set_halign(etiqueta_texto, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(caja_principal), etiqueta_texto);
	GtkWidget *ventana_desplazamiento = gtk_scrolled_window_new();
	gtk_widget_set_vexpand(ventana_desplazamiento, TRUE);
	gtk_widget_set_size_request(ventana_desplazamiento, -1, 200);
	g_estado.vista_texto = GTK_TEXT_VIEW(gtk_text_view_new());
	g_estado.buffer_texto = gtk_text_view_get_buffer(g_estado.vista_texto);
	gtk_text_view_set_wrap_mode(g_estado.vista_texto, GTK_WRAP_WORD_CHAR);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ventana_desplazamiento), GTK_WIDGET(g_estado.vista_texto));
	gtk_box_append(GTK_BOX(caja_principal), ventana_desplazamiento);

	// Botones de acción
	GtkWidget *caja_botones = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_halign(caja_botones, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(caja_principal), caja_botones);

	GtkWidget *boton_abrir = gtk_button_new_with_label("📂 Abrir Archivo (Ctrl+O)");
	gtk_widget_add_css_class(boton_abrir, "pill");
	g_signal_connect(boton_abrir, "clicked", G_CALLBACK(al_pulsar_abrir_archivo), NULL);
	gtk_box_append(GTK_BOX(caja_botones), boton_abrir);

	GtkWidget *boton_hablar = gtk_button_new_with_label("🎙️ Hablar (Ctrl+Enter)");
	gtk_widget_add_css_class(boton_hablar, "suggested-action");
	gtk_widget_add_css_class(boton_hablar, "pill");
	g_signal_connect(boton_hablar, "clicked", G_CALLBACK(al_pulsar_hablar), NULL);
	gtk_box_append(GTK_BOX(caja_botones), boton_hablar);

	GtkWidget *boton_detener = gtk_button_new_with_label("⏹️ Detener (Ctrl+S)");
	gtk_widget_add_css_class(boton_detener, "destructive-action");
	gtk_widget_add_css_class(boton_detener, "pill");
	g_signal_connect(boton_detener, "clicked", G_CALLBACK(al_pulsar_detener), NULL);
	gtk_box_append(GTK_BOX(caja_botones), boton_detener);

	GtkWidget *boton_exportar = gtk_button_new_with_label("💾 Exportar WAV (Ctrl+Shift+S)");
	gtk_widget_add_css_class(boton_exportar, "pill");
	g_signal_connect(boton_exportar, "clicked", G_CALLBACK(al_pulsar_exportar_wav), NULL);
	gtk_box_append(GTK_BOX(caja_botones), boton_exportar);

	GtkWidget *boton_limpiar = gtk_button_new_with_label("🗑️ Limpiar");
	gtk_widget_add_css_class(boton_limpiar, "pill");
	g_signal_connect(boton_limpiar, "clicked", G_CALLBACK(al_pulsar_limpiar), NULL);
	gtk_box_append(GTK_BOX(caja_botones), boton_limpiar);

	// Etiqueta de estado
	g_estado.etiqueta_estado = GTK_LABEL(gtk_label_new("Iniciando..."));
	gtk_widget_add_css_class(GTK_WIDGET(g_estado.etiqueta_estado), "dim-label");
	gtk_box_append(GTK_BOX(caja_principal), GTK_WIDGET(g_estado.etiqueta_estado));

	// Atajos de teclado
	GtkEventController *controlador_teclado = gtk_event_controller_key_new();
	g_signal_connect(controlador_teclado, "key-pressed", G_CALLBACK(al_pulsar_tecla), NULL);
	gtk_widget_add_controller(ventana, controlador_teclado);

	gtk_window_present(g_estado.ventana_principal);
	actualizar_estado("✅ Listo. Usa Ctrl+O para abrir archivo o escribe texto.", true);
}

} // namespace InterfazGrafica
