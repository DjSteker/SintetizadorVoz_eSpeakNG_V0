//============================================================================
// Name        : SintetizadorVoz_eSpeakNG_V2.cpp
// Version     : 1.8
// Description : Sintetizador de voz con variantes
//$(shell pkg-config --cflags gtk4 espeak-ng) -mfpmath=sse -msse -msse2
//$(shell pkg-config --libs gtk4 espeak-ng)
//============================================================================

#include "config.hpp"
#include "interfaz_grafica.hpp"
#include "motor_voz.hpp"

#include <gtk/gtk.h>

#include <iostream>

int main(int argc, char **argv) {
	if (!MotorVoz::inicializar()) {
		std::cerr << "Fallo fatal en la inicialización de eSpeak-NG." << std::endl;
		return 1;
	}

	GtkApplication *app = gtk_application_new("com.example.GtkEspeakTTS.Accessible", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(InterfazGrafica::activar), NULL);
	int estado = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	MotorVoz::finalizar();
	return estado;
}

