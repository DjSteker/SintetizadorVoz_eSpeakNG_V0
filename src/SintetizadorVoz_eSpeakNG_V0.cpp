
//============================================================================
// Name        : SintetizadorVoz_eSpeakNG_Dinamico.cpp
// Author      :
// Version     : 1.6
// Description : Sintetizador de voz con detección dinámica de voces y variantes
//============================================================================
#include <gtk/gtk.h>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <atomic>
#include <cstring>
#include <espeak-ng/speak_lib.h>

const int DEFAULT_RATE = 175;
const int DEFAULT_PITCH = 50;
const int DEFAULT_VOLUME = 99;
const std::string DEFAULT_VOICE = "es";
const bool DEFAULT_SPEAK_STATUS = true;
const std::string CONFIG_FILE = "tts_config.txt";

struct AppConfig {
	std::string current_voice = DEFAULT_VOICE;
	int rate = DEFAULT_RATE;
	int pitch = DEFAULT_PITCH;
	int volume = DEFAULT_VOLUME;
	bool speak_status = DEFAULT_SPEAK_STATUS;
};

GtkTextView *text_view;
GtkTextBuffer *text_buffer;
GtkLabel *status_label;
GtkWindow *main_window;
GtkDropDown *voice_variant_dropdown;
AppConfig app_config;
std::atomic<bool> is_speaking(false);
std::atomic<bool> should_stop(false);
std::vector<std::pair<std::string, std::string>> available_voices;
std::vector<std::pair<std::string, std::string>> current_language_voices;

void update_status_from_thread(const char *message, bool speak = false) {
	if (status_label) {
	g_idle_add((GSourceFunc)[](gpointer data) -> gboolean {
				auto *params = (std::pair<const char*, bool>*)data;
				gtk_label_set_text(status_label, params->first);
				if (params->second && app_config.speak_status && !is_speaking) {
					std::thread([](std::string msg) {
								std::string clean_msg = msg;
								size_t pos = clean_msg.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
								if (pos != std::string::npos) clean_msg = clean_msg.substr(pos);
								if (!clean_msg.empty()) {
									espeak_Synth(clean_msg.c_str(), clean_msg.length() + 1, 0, POS_CHARACTER, 0, espeakCHARS_AUTO, NULL, NULL);
									espeak_Synchronize();
								}
							}, std::string(params->first)).detach();
				}
				g_free((gpointer)params->first);
				delete params;
				return G_SOURCE_REMOVE;
			}, new std::pair<const char*, bool>(g_strdup(message), speak));
}
}

void save_config() {
std::ofstream file(CONFIG_FILE);
if (file.is_open()) {
	file << "voice=" << app_config.current_voice << "\n";
	file << "rate=" << app_config.rate << "\n";
	file << "pitch=" << app_config.pitch << "\n";
	file << "volume=" << app_config.volume << "\n";
	file << "speak_status=" << (app_config.speak_status ? "1" : "0") << "\n";
	file.close();
	update_status_from_thread(" Configuración guardada.", false);
}
}

void load_config() {
std::ifstream file(CONFIG_FILE);
if (file.is_open()) {
	std::string line;
	while (std::getline(file, line)) {
		size_t pos = line.find('=');
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);
			if (key == "voice") {
				app_config.current_voice = value;
			} else if (key == "rate") {
				app_config.rate = std::stoi(value);
			} else if (key == "pitch") {
				app_config.pitch = std::stoi(value);
			} else if (key == "volume") {
				app_config.volume = std::stoi(value);
			} else if (key == "speak_status") {
				app_config.speak_status = (value == "1");
			}
		}
	}
	file.close();
}
}

std::vector<std::pair<std::string, std::string>> get_available_voices() {
std::vector<std::pair<std::string, std::string>> voices;
const espeak_VOICE **espeak_voices = espeak_ListVoices(NULL);
if (!espeak_voices) {
	std::cerr << "Error: No se pudieron obtener las voces de eSpeak-NG." << std::endl;
	return voices;
}
for (const espeak_VOICE **voice_ptr = espeak_voices; *voice_ptr; ++voice_ptr) {
	const espeak_VOICE *voice = *voice_ptr;
	if (voice->name) {
		std::string voice_name = voice->name;
		std::string display_name = std::string(voice->name) + " (" + std::string(voice->languages) + ")";
		voices.emplace_back(display_name, voice_name);
	}
}
return voices;
}

// --- Obtener voces para un idioma específico ---
std::vector<std::pair<std::string, std::string>> get_voices_for_language(const std::string &language_code) {
std::vector<std::pair<std::string, std::string>> filtered_voices;
const espeak_VOICE **espeak_voices = espeak_ListVoices(NULL);
if (!espeak_voices) {
	std::cerr << "Error: No se pudieron obtener las voces de eSpeak-NG." << std::endl;
	return filtered_voices;
}
for (const espeak_VOICE **voice_ptr = espeak_voices; *voice_ptr; ++voice_ptr) {
	const espeak_VOICE *voice = *voice_ptr;
	if (voice->name && std::string(voice->languages).find(language_code) != std::string::npos) {
		std::string display_name = std::string(voice->name) + " (" + std::string(voice->languages) + ")";
		filtered_voices.emplace_back(display_name, voice->name);
	}
}
return filtered_voices;
}

void update_voice_variant_dropdown(const std::string &language_code) {
current_language_voices = get_voices_for_language(language_code);
if (current_language_voices.empty()) {
	gtk_drop_down_set_model(voice_variant_dropdown, NULL);
	update_status_from_thread(" No se encontraron variantes de voz para este idioma.", true);
	return;
}

GtkStringList *string_list = gtk_string_list_new(NULL);
for (const auto &voice : current_language_voices) {
	gtk_string_list_append(string_list, voice.first.c_str());
}

gtk_drop_down_set_model(voice_variant_dropdown, G_LIST_MODEL(string_list));
gtk_drop_down_set_selected(voice_variant_dropdown, 0);
}

void update_espeak_settings() {
espeak_SetVoiceByName(app_config.current_voice.c_str());
espeak_SetParameter(espeakRATE, app_config.rate, 0);
espeak_SetParameter(espeakPITCH, app_config.pitch, 0);
espeak_SetParameter(espeakVOLUME, app_config.volume, 0);
}

void synthesize_speech(std::string text) {
if (is_speaking) {
	update_status_from_thread(" Ya hay una síntesis en curso.", true);
	return;
}
is_speaking = true;
should_stop = false;
update_status_from_thread(" Hablando...");
int flags = espeakCHARS_AUTO | espeakSSML;
int ret = espeak_Synth(text.c_str(), text.length() + 1, 0, POS_CHARACTER, 0, flags, NULL, NULL);
if (ret != EE_OK) {
	update_status_from_thread((" Error de síntesis. Código: " + std::to_string(ret)).c_str(), true);
} else {
	espeak_Synchronize();
	if (!should_stop) {
		update_status_from_thread(" Síntesis completada.", true);
	} else {
		update_status_from_thread("Síntesis detenida.", true);
	}
}
is_speaking = false;
}

void stop_speech() {
if (is_speaking) {
	should_stop = true;
	espeak_Cancel();
} else {
	update_status_from_thread("No hay síntesis en curso para detener.", true);
}
}

static void on_speak_button_clicked(GtkWidget *widget, gpointer data) {
GtkTextIter start, end;
gtk_text_buffer_get_bounds(text_buffer, &start, &end);
char *text = gtk_text_buffer_get_text(text_buffer, &start, &end, FALSE);
std::string text_str(text);
g_free(text);
if (text_str.empty()) {
	update_status_from_thread(" No hay texto para sintetizar.", true);
	return;
}
std::thread(synthesize_speech, text_str).detach();
}

static void on_stop_button_clicked(GtkWidget *widget, gpointer data) {
stop_speech();
}

static void on_clear_button_clicked(GtkWidget *widget, gpointer data) {
gtk_text_buffer_set_text(text_buffer, "", -1);
update_status_from_thread("Texto borrado.", true);
}

static void on_open_file_clicked(GtkWidget *widget, gpointer data) {
GtkFileDialog *dialog = gtk_file_dialog_new();
gtk_file_dialog_set_title(dialog, "Abrir archivo de texto");
GtkFileFilter *filter = gtk_file_filter_new();
gtk_file_filter_set_name(filter, "Archivos de texto");
gtk_file_filter_add_mime_type(filter, "text/plain");
gtk_file_filter_add_pattern(filter, "*.txt");
GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
g_list_store_append(filters, filter);
gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
gtk_file_dialog_open(dialog, main_window, NULL, +[](GObject *source, GAsyncResult *result, gpointer user_data) {
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);
	if (file) {
		char *filename = g_file_get_path(file);
		std::ifstream input_file(filename);
		if (input_file.is_open()) {
			std::stringstream buffer;
			buffer << input_file.rdbuf();
			std::string content = buffer.str();
			gtk_text_buffer_set_text(text_buffer, content.c_str(), -1);
			update_status_from_thread("Archivo cargado correctamente.", true);
		} else {
			update_status_from_thread("Error al abrir el archivo.", true);
		}
		g_free(filename);
		g_object_unref(file);
	} else if (error) {
		if (error->code != GTK_DIALOG_ERROR_DISMISSED) {
			std::cerr << "Error al abrir archivo: " << error->message << std::endl;
			update_status_from_thread(" Error al seleccionar archivo.", true);
		}
		g_error_free(error);
	}
}, NULL);
g_object_unref(dialog);
}

static void on_voice_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
guint selected = gtk_drop_down_get_selected(dropdown);
if (selected != GTK_INVALID_LIST_POSITION) {
	GtkStringObject *selected_obj = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dropdown));
	const char *selected_display_name = gtk_string_object_get_string(selected_obj);
	if (selected_display_name) {
		for (const auto &voice : available_voices) {
			if (voice.first == selected_display_name) {
				// Extraer el código de idioma (ej: "es" de "es-es")
				std::string language_code = voice.second.substr(0, 2);
				update_voice_variant_dropdown(language_code);
				break;
			}
		}
	}
}
}

static void on_voice_variant_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
guint selected = gtk_drop_down_get_selected(dropdown);
if (selected != GTK_INVALID_LIST_POSITION) {
	GtkStringObject *selected_obj = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dropdown));
	const char *selected_display_name = gtk_string_object_get_string(selected_obj);
	if (selected_display_name) {
		for (const auto &voice : current_language_voices) {
			if (voice.first == selected_display_name) {
				app_config.current_voice = voice.second;
				update_espeak_settings();
				update_status_from_thread((" Voz seleccionada: " + voice.first).c_str(), true);
				save_config();
				break;
			}
		}
	}
}
}

static void on_parameter_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
const char *param_type = static_cast<const char*>(user_data);
if (!param_type)
	return;
guint selected = gtk_drop_down_get_selected(dropdown);
if (selected != GTK_INVALID_LIST_POSITION) {
	GtkStringObject *selected_obj = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dropdown));
	const char *selected_text = gtk_string_object_get_string(selected_obj);
	if (selected_text) {
		try {
			int value = std::stoi(selected_text);
			std::string status_msg;
			if (std::strcmp(param_type, "rate") == 0) {
				app_config.rate = value;
				status_msg = "⚡ Velocidad: " + std::to_string(value) + " WPM";
			} else if (std::strcmp(param_type, "pitch") == 0) {
				app_config.pitch = value;
				status_msg = "Tono: " + std::to_string(value);
			}
			update_espeak_settings();
			update_status_from_thread(status_msg.c_str(), true);
			save_config();
		} catch (const std::exception &e) {
			std::cerr << "Error al parsear parámetro: " << e.what() << std::endl;
		}
	}
}
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
int volume = (int) gtk_range_get_value(range);
app_config.volume = volume;
update_espeak_settings();
update_status_from_thread(("Volumen: " + std::to_string(volume)).c_str());
save_config();
}

static void on_speak_status_toggled(GtkCheckButton *check_button, gpointer user_data) {
app_config.speak_status = gtk_check_button_get_active(check_button);
if (app_config.speak_status) {
	update_status_from_thread("Lectura de estado activada.", true);
} else {
	update_status_from_thread(" Lectura de estado desactivada.", false);
}
save_config();
}

static gboolean on_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
	on_speak_button_clicked(NULL, NULL);
	return TRUE;
}
if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_s) {
	on_stop_button_clicked(NULL, NULL);
	return TRUE;
}
if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_o) {
	on_open_file_clicked(NULL, NULL);
	return TRUE;
}
return FALSE;
}

GtkWidget* create_config_dropdown(const std::vector<std::string> &options, const std::string &default_value, GCallback handler, GtkWidget *parent_box, gpointer user_data = NULL) {
GtkStringList *string_list = gtk_string_list_new(NULL);
guint default_index = 0;
for (guint i = 0; i < options.size(); ++i) {
	gtk_string_list_append(string_list, options[i].c_str());
	if (options[i].find(default_value) != std::string::npos) {
		default_index = i;
	}
}
GtkDropDown *dropdown = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(string_list), NULL));
gtk_drop_down_set_selected(dropdown, default_index);
g_signal_connect(dropdown, "notify::selected", handler, user_data);
gtk_box_append(GTK_BOX(parent_box), GTK_WIDGET(dropdown));
return GTK_WIDGET(dropdown);
}

static void activate_gtk(GtkApplication *app, gpointer user_data) {
load_config();
available_voices = get_available_voices();
if (available_voices.empty()) {
	update_status_from_thread("No se encontraron voces instaladas. Usando valores por defecto.", true);
	available_voices.emplace_back("Español (España) [Fallback]", "es");
	app_config.current_voice = "es";
}

GtkCssProvider *css_provider = gtk_css_provider_new();
gtk_css_provider_load_from_string(css_provider, "textview { font-size: 14pt; }"
		"textview text { padding: 10px; }"

		/* Estilos generales de botones */
		"button.pill { "
		"  min-height: 40px; "
		"  font-size: 12pt; "
		"  border-radius: 20px; "
		"  padding: 8px 16px; "
		"}"

		/* Botón de acción sugerida (verde) */
		"button.suggested-action { "
		"  background-image: linear-gradient(to bottom, #4CAF50, #45a049); "
		"  color: white; "
		"  border: none; "
		"  box-shadow: 0 2px 4px rgba(0,0,0,0.2); "
		"}"
		"button.suggested-action:hover { "
		"  background-image: linear-gradient(to bottom, #5CBF60, #4CAF50); "
		"}"
		"button.suggested-action label { "
		"  color: white; "
		"  font-weight: bold; "
		"}"

		/* Botón de acción destructiva (rojo) */
		"button.destructive-action { "
		"  background-image: linear-gradient(to bottom, #f44336, #e53935); "
		"  color: white; "
		"  border: none; "
		"  box-shadow: 0 2px 4px rgba(0,0,0,0.2); "
		"}"
		"button.destructive-action:hover { "
		"  background-image: linear-gradient(to bottom, #ff5545, #f44336); "
		"}"
		"button.destructive-action label { "
		"  color: white; "
		"  font-weight: bold; "
		"}"

		/* Botones normales */
		"button.pill:not(.suggested-action):not(.destructive-action) { "
		"  background-image: linear-gradient(to bottom, #e0e0e0, #d0d0d0); "
		"  color: #333; "
		"  border: 1px solid #999; "
		"  box-shadow: 0 1px 3px rgba(0,0,0,0.1); "
		"}"
		"button.pill:not(.suggested-action):not(.destructive-action):hover { "
		"  background-image: linear-gradient(to bottom, #f0f0f0, #e0e0e0); "
		"}"
		"button.pill:not(.suggested-action):not(.destructive-action) label { "
		"  color: #333; "
		"  font-weight: 600; "
		"}"

		/* Título */
		"label.title-1 { "
		"  font-size: 18pt; "
		"  font-weight: bold; "
		"  color: #2c3e50; "
		"}");
gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
g_object_unref(css_provider);

GtkWidget *window = gtk_application_window_new(app);
main_window = GTK_WINDOW(window);
gtk_window_set_title(main_window, "eSpeak-NG - Sintetizador de Voz Accesible");
gtk_window_set_default_size(main_window, 800, 650);

GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
gtk_widget_set_margin_start(vbox, 20);
gtk_widget_set_margin_end(vbox, 20);
gtk_widget_set_margin_top(vbox, 20);
gtk_widget_set_margin_bottom(vbox, 20);
gtk_window_set_child(main_window, vbox);

GtkWidget *title_label = gtk_label_new(" Conversor de Texto a Voz");
gtk_widget_add_css_class(title_label, "title-1");
gtk_box_append(GTK_BOX(vbox), title_label);

GtkWidget *config_grid = gtk_grid_new();
gtk_grid_set_column_spacing(GTK_GRID(config_grid), 10);
gtk_grid_set_row_spacing(GTK_GRID(config_grid), 10);
gtk_box_append(GTK_BOX(vbox), config_grid);

gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new("Idioma:"), 0, 0, 1, 1);
GtkWidget *voice_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
std::vector<std::string> voice_names;
for (const auto &voice : available_voices) {
	voice_names.push_back(voice.first);
}
int default_voice_index = 0;
for (size_t i = 0; i < available_voices.size(); ++i) {
	if (available_voices[i].second == app_config.current_voice) {
		default_voice_index = i;
		break;
	}
}
create_config_dropdown(voice_names, voice_names[default_voice_index], G_CALLBACK(on_voice_changed), voice_box);
gtk_grid_attach(GTK_GRID(config_grid), voice_box, 1, 0, 1, 1);

gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new(" Variante de voz:"), 2, 0, 1, 1);
GtkWidget *voice_variant_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
voice_variant_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
gtk_box_append(GTK_BOX(voice_variant_box), GTK_WIDGET(voice_variant_dropdown));
gtk_grid_attach(GTK_GRID(config_grid), voice_variant_box, 3, 0, 1, 1);
g_signal_connect(voice_variant_dropdown, "notify::selected", G_CALLBACK(on_voice_variant_changed), NULL);

gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new("⚡ Velocidad:"), 0, 1, 1, 1);
GtkWidget *rate_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
std::vector<std::string> rates = { "100", "140", "175", "220", "280" };
create_config_dropdown(rates, std::to_string(app_config.rate), G_CALLBACK(on_parameter_changed), rate_box, (gpointer) "rate");
gtk_grid_attach(GTK_GRID(config_grid), rate_box, 1, 1, 1, 1);

gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new(" Tono:"), 2, 1, 1, 1);
GtkWidget *pitch_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
std::vector<std::string> pitches = { "20", "50", "70", "90" };
create_config_dropdown(pitches, std::to_string(app_config.pitch), G_CALLBACK(on_parameter_changed), pitch_box, (gpointer) "pitch");
gtk_grid_attach(GTK_GRID(config_grid), pitch_box, 3, 1, 1, 1);

GtkWidget *volume_label = gtk_label_new("Volumen (0-99):");
gtk_widget_set_halign(volume_label, GTK_ALIGN_START);
gtk_grid_attach(GTK_GRID(config_grid), volume_label, 0, 2, 1, 1);
GtkWidget *volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 99.0, 1.0);
gtk_range_set_value(GTK_RANGE(volume_scale), (double) app_config.volume);
gtk_widget_set_hexpand(volume_scale, TRUE);
gtk_scale_set_draw_value(GTK_SCALE(volume_scale), TRUE);
g_signal_connect(volume_scale, "value-changed", G_CALLBACK(on_volume_changed), NULL);
gtk_grid_attach(GTK_GRID(config_grid), volume_scale, 1, 2, 3, 1);

GtkWidget *speak_status_check = gtk_check_button_new_with_label("Leer mensajes de estado");
gtk_check_button_set_active(GTK_CHECK_BUTTON(speak_status_check), app_config.speak_status);
gtk_widget_set_tooltip_text(speak_status_check, "Si está marcado, la aplicación leerá en voz alta los mensajes de éxito/error.");
g_signal_connect(speak_status_check, "toggled", G_CALLBACK(on_speak_status_toggled), NULL);
gtk_widget_set_halign(speak_status_check, GTK_ALIGN_END);
gtk_grid_attach(GTK_GRID(config_grid), speak_status_check, 4, 2, 2, 1);

GtkWidget *text_label = gtk_label_new(" Texto a sintetizar (Ctrl+Enter para hablar):");
gtk_widget_set_halign(text_label, GTK_ALIGN_START);
gtk_box_append(GTK_BOX(vbox), text_label);
GtkWidget *scrolled_window = gtk_scrolled_window_new();
gtk_widget_set_vexpand(scrolled_window, TRUE);
gtk_widget_set_size_request(scrolled_window, -1, 200);
text_view = GTK_TEXT_VIEW(gtk_text_view_new());
text_buffer = gtk_text_view_get_buffer(text_view);
gtk_text_view_set_wrap_mode(text_view, GTK_WRAP_WORD_CHAR);
gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), GTK_WIDGET(text_view));
gtk_box_append(GTK_BOX(vbox), scrolled_window);

GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
gtk_box_append(GTK_BOX(vbox), button_box);
GtkWidget *open_button = gtk_button_new_with_label(" Abrir Archivo (Ctrl+O)");
gtk_widget_add_css_class(open_button, "pill");
g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_file_clicked), NULL);
gtk_box_append(GTK_BOX(button_box), open_button);
GtkWidget *speak_button = gtk_button_new_with_label(" Hablar (Ctrl+Enter)");
gtk_widget_add_css_class(speak_button, "suggested-action");
gtk_widget_add_css_class(speak_button, "pill");
g_signal_connect(speak_button, "clicked", G_CALLBACK(on_speak_button_clicked), NULL);
gtk_box_append(GTK_BOX(button_box), speak_button);
GtkWidget *stop_button = gtk_button_new_with_label(" Detener (Ctrl+S)");
gtk_widget_add_css_class(stop_button, "destructive-action");
gtk_widget_add_css_class(stop_button, "pill");
g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_button_clicked), NULL);
gtk_box_append(GTK_BOX(button_box), stop_button);
GtkWidget *clear_button = gtk_button_new_with_label("Limpiar");
gtk_widget_add_css_class(clear_button, "pill");
g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), NULL);
gtk_box_append(GTK_BOX(button_box), clear_button);

status_label = GTK_LABEL(gtk_label_new("Iniciando..."));
gtk_widget_add_css_class(GTK_WIDGET(status_label), "dim-label");
gtk_box_append(GTK_BOX(vbox), GTK_WIDGET(status_label));

GtkEventController *key_controller = gtk_event_controller_key_new();
g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
gtk_widget_add_controller(window, key_controller);

gtk_window_present(main_window);
update_espeak_settings();
update_status_from_thread("Listo. Usa Ctrl+O para abrir archivo o escribe texto.", true);
}

bool init_espeak() {
int sampleRate = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
if (sampleRate < 0) {
	std::cerr << "Error: No se pudieron inicializar eSpeak-NG. Código: " << sampleRate << std::endl;
	return false;
}
std::cout << "eSpeak-NG inicializado (Sample Rate: " << sampleRate << " Hz)" << std::endl;
return true;
}

int main(int argc, char **argv) {
if (!init_espeak()) {
	std::cerr << "Fallo fatal en la inicialización de eSpeak-NG." << std::endl;
	return 1;
}
GtkApplication *app = gtk_application_new("com.example.GtkEspeakTTS.Accessible", G_APPLICATION_DEFAULT_FLAGS);
g_signal_connect(app, "activate", G_CALLBACK(activate_gtk), NULL);
int status = g_application_run(G_APPLICATION(app), argc, argv);
g_object_unref(app);
espeak_Terminate();
return status;
}

