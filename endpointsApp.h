#ifndef ENDPOINTS_APP_H
#define ENDPOINTS_APP_H

#include <WebServer.h>

// 👉 Referencia al servidor, debe ser el mismo objeto del archivo principal
extern WebServer server;

// 👉 Variables que quieres configurar
extern int porciones;
extern int intervaloHoras;

// 👉 Declaración de funciones que están en main.cpp
void guardarEEPROM();
void servirComida();

// 👉 Declaración de función para registrar las rutas
void configurarEndpointsApp();

// 👉 Implementación de las rutas
void configurarEndpointsApp() {
    // Ruta para configurar porciones y horas
    server.on("/configurar", HTTP_GET, []() {
        if (server.hasArg("porciones") && server.hasArg("intervalo")) {
            porciones = constrain(server.arg("porciones").toInt(), 1, 20);
            intervaloHoras = constrain(server.arg("intervalo").toInt(), 0, 24);
            guardarEEPROM();
            server.send(200, "text/plain", "Configuracion actualizada");
        } else {
            server.send(400, "text/plain", "Faltan parametros");
        }
    });

    // Ruta para servir comida
    server.on("/servir", HTTP_GET, []() {
        servirComida();
        server.send(200, "text/plain", "Comida servida");
    });
}

#endif
