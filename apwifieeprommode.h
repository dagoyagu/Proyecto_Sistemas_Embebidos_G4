#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

extern Adafruit_SSD1306 display;

// EEPROM WiFi
#define EEPROM_ADDR_SSID1 40
#define EEPROM_ADDR_PASS1 90
#define EEPROM_ADDR_SSID2 140
#define EEPROM_ADDR_PASS2 190
#define EEPROM_ADDR_FLAG 300


WebServer server(80);

String leerStringDeEEPROM(int direccion)
{
    String cadena = "";
    char caracter = EEPROM.read(direccion);
    int i = 0;
    while (caracter != '\0' && i < 100)
    {
        cadena += caracter;
        i++;
        caracter = EEPROM.read(direccion + i);
    }
    return cadena;
}


// Función para limpiar solo las redes WiFi guardadas
void limpiarEEPROMWiFi() {
    for (int i = 40; i < 300; i++) { // Solo limpia el espacio reservado para las redes
        EEPROM.write(i, 0);
    }
    EEPROM.write(EEPROM_ADDR_FLAG, 0); // Limpia la bandera también
    EEPROM.commit();
    Serial.println("Zona WiFi limpiada.");
}


void limpiarEEPROM(int direccion, int longitud) {
    for (int i = 0; i < longitud; i++) {
        EEPROM.write(direccion + i, 0);
    }
    EEPROM.commit();
}




void escribirStringEnEEPROM(int direccion, String cadena) {
    int longitudCadena = cadena.length();
    
    for (int i = 0; i < longitudCadena; i++) {
        EEPROM.write(direccion + i, cadena[i]);
    }
    EEPROM.write(direccion + longitudCadena, '\0'); // Importante: escribir el fin de cadena

    EEPROM.commit(); // 🔥 Muy importante: guardar realmente los cambios
}


void mostrarEnPantalla(String linea1, String linea2 = "", String linea3 = "", String linea4 = "", String linea5 = "")
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(linea1);
    if (linea2 != "")
    {
        display.println(linea2);
    }
    if (linea3 != "")
    {
        display.println(linea3);
    }
    if (linea4 != "")
    {
        display.println(linea4);
    }
    if (linea5 != "")
    {
        display.println(linea5);
    }
    display.display();
}

int posW = 50;

void handleRoot()
{
    String html = "<html><body>";
    html += "<form method='POST' action='/wifi'>";
    html += "Red Wi-Fi: <input type='text' name='ssid'><br>";
    html += "Contrasena: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Conectar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

void handleWifi() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    Serial.print("Conectando a la red Wi-Fi: ");
    Serial.println(ssid);
    mostrarEnPantalla("Conectando a:", ssid);

    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str(), 6);

    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED && cnt < 8) {
        delay(1000);
        Serial.print(".");
        mostrarEnPantalla("Conectando a:", ssid, "Esperando...");
        cnt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        // 🔸 Leer bandera para saber dónde guardar
        String varsave = leerStringDeEEPROM(EEPROM_ADDR_FLAG);
        if (varsave == "a") {
            posW = EEPROM_ADDR_SSID1; // Guarda en primera posición
            escribirStringEnEEPROM(EEPROM_ADDR_FLAG, "b"); // Cambia la bandera
        } else {
            posW = EEPROM_ADDR_SSID2; // Guarda en segunda posición
            escribirStringEnEEPROM(EEPROM_ADDR_FLAG, "a"); // Cambia la bandera
        }

        // 🔸 Limpia posiciones antes de guardar
        limpiarEEPROM(posW, 50);           // Limpia SSID
        limpiarEEPROM(posW + 50, 50);      // Limpia contraseña

        // 🔸 Guarda nueva red
        escribirStringEnEEPROM(posW, ssid);
        escribirStringEnEEPROM(posW + 50, password);

        Serial.println("\nConexion establecida");
        Serial.println("IP: " + WiFi.localIP().toString());

        mostrarEnPantalla("Conectado a:", ssid, "IP: " + WiFi.localIP().toString());
        server.send(200, "text/plain", "Conexion establecida: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nNo se pudo conectar");
        mostrarEnPantalla("Error de conexion", "Intente de nuevo");
        server.send(200, "text/plain", "Error de conexion");
    }
}



bool lastRed() {
    String ultima = leerStringDeEEPROM(EEPROM_ADDR_FLAG);
    int pos = (ultima == "a") ? EEPROM_ADDR_SSID1 : EEPROM_ADDR_SSID2;

    String ssid = leerStringDeEEPROM(pos);
    String password = leerStringDeEEPROM(pos + 50);

    // Validación de datos leídos
    if (ssid.length() < 2 || password.length() < 8) {
        Serial.println("Datos inválidos en la última red.");
        return false;
    }

    Serial.println("Intentando conectar a: " + ssid);
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str(), 6);

    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED && cnt < 5) {
        delay(1000);
        Serial.print(".");
        cnt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado exitosamente.");
        Serial.println("IP: " + WiFi.localIP().toString());
        return true;
    }

    return false; // No se pudo conectar
}



void initAP(const char *apSsid, const char *apPassword)
{
    Serial.println("Iniciando modo AP");
    mostrarEnPantalla("Modo AP activado","BUSQUE LA RED", "SSID: " + String(apSsid),"Pass: 12345678", "Poner IP en navegador: 192.168.4.1");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    server.on("/", handleRoot);
    server.on("/wifi", handleWifi);

    server.begin();
    Serial.println("Servidor web iniciado");
}

void loopAP()
{
    server.handleClient();
}

void intentoconexion(const char *apname, const char *appassword)
{
    Serial.begin(115200);
    EEPROM.begin(512);
    Wire.begin(4, 15);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("No se encuentra la pantalla OLED");
        while (true);
    }

    display.clearDisplay();
    display.display();

    // 👉 Verificar si ya estamos conectados antes de hacer cualquier cosa
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Ya estamos conectados");
        mostrarEnPantalla("Conexion exitosa", "Red: " + WiFi.SSID(), "IP: " + WiFi.localIP().toString());
        return; // No hacer nada más
    }

    Serial.println("Iniciando intento de conexion...");
    mostrarEnPantalla("Intentando conexion...");

    if (!lastRed())
    {
        Serial.println("No se encontro red guardada");
        mostrarEnPantalla("Configurar WiFi", "Conectese a:", apname);

        Serial.println("Conectese desde su celular a la red: " + String(apname));
        Serial.println("Ingrese en el navegador: 192.168.4.1");

        initAP(apname, appassword);
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        loopAP();
    }

    Serial.println("Conexion exitosa");
    mostrarEnPantalla("Conexion exitosa", "Red: " + WiFi.SSID(), "IP: " + WiFi.localIP().toString());
}

