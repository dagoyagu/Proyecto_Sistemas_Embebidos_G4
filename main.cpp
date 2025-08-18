// Proyecto: Dispensador de comida para gatos
// Autor: David
// Plataforma: ESP32 con PlatformIO
// Librerías necesarias:
//   - HX711
//   - Adafruit SSD1306
//   - Adafruit GFX
//   - Rtc by Makuna
//   - Stepper.h (motor paso a paso)




#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <RtcDS3231.h>
#include <EEPROM.h>
#include <Stepper.h>
#include "apwifieeprommode.h"
#include "endpointsApp.h"
#include "alexaControl.h"



#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pines OLED y RTC (I2C en GPIO 4 y 15)
#define SDA_PIN 4
#define SCL_PIN 15

// Pines HX711
#define HX711_DT 32
#define HX711_SCK 33
HX711 balanza;

// Pines Botones (PULL DOWN)
#define BOTON1 5
#define BOTON2 17
#define BOTON3 16
#define BOTON4 2

// Pines motor paso a paso ULN2003
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25
Stepper motor(2048, IN1, IN3, IN2, IN4);

// Pines sensor ultrasónico
#define TRIG 18
#define ECHO 19

// DS3231
RtcDS3231<TwoWire> rtc(Wire);

// Variables globales
float factorEscala =0; 
int porciones = 0;
int intervaloHoras = 0;
int pesoPlato = 0;
unsigned long ultimoServido = 0;
RtcDateTime horaUltima;

bool calibrado = false;
bool mostrarTiempoRestante = false;
unsigned long tiempoB4 = 0;

unsigned long tiempoInicio = 0;
bool boton3Activo = false;

unsigned long tiempoBoton1 = 0; // Guarda el tiempo cuando se presiona
bool boton1Activo = false;      // Indica si el botón está siendo presionado

bool boton1Presionado = false;
bool taraRealizada = false;


const int maximosIntentos = 5; // ✅ Puedes cambiar este número fácilmente
int contadorIntentos = 0;

unsigned long tiempoReintento = 0;
bool esperandoReintento = false;


unsigned long tiempoBotonB2 = 0;
bool mostrandoIP = false;
bool boton2Procesado = false;

volatile bool servirDesdeAlexa = false; // responde alexa rapido


// EEPROM Principal
#define EEPROM_SIZE 512
#define EEPROM_ADDR_ESCALA 0
#define EEPROM_ADDR_PORCIONES 10
#define EEPROM_ADDR_INTERVALO 20
#define EEPROM_ADDR_HORAULTIMA 30





void guardarEEPROM() {
  EEPROM.put(EEPROM_ADDR_ESCALA, factorEscala);
  EEPROM.put(EEPROM_ADDR_PORCIONES, porciones);
  EEPROM.put(EEPROM_ADDR_INTERVALO, intervaloHoras);
  EEPROM.put(EEPROM_ADDR_HORAULTIMA, horaUltima);
  EEPROM.commit();
}

void leerEEPROM() {
  EEPROM.get(EEPROM_ADDR_ESCALA, factorEscala);
  EEPROM.get(EEPROM_ADDR_PORCIONES, porciones);
  EEPROM.get(EEPROM_ADDR_INTERVALO, intervaloHoras);
  EEPROM.get(EEPROM_ADDR_HORAULTIMA, horaUltima);
  if (isnan(factorEscala) || factorEscala <= 0.1 || factorEscala > 100000) {
    factorEscala = 1.0;
    calibrado = false;
  } else {
    calibrado = true;
  }

  if (intervaloHoras <= 0 || intervaloHoras > 24) {
    intervaloHoras = 1; // Valor por defecto seguro
  }


}

bool botonPresionado(int pin) {
  if (digitalRead(pin)) {
    delay(10); // pequeña espera para estabilizar
    if (digitalRead(pin)) {
      while (digitalRead(pin)); // espera a que se suelte
      return true;
    }
  }
  return false;
}

void setupBotones() {
  pinMode(BOTON1, INPUT_PULLDOWN);
  pinMode(BOTON2, INPUT_PULLDOWN);
  pinMode(BOTON3, INPUT_PULLDOWN);
  pinMode(BOTON4, INPUT_PULLDOWN);
}

void setupUltrasonico() {
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
}

float leerDistancia() {
  float suma = 0;
  const int muestras = 5;

  for (int i = 0; i < muestras; i++) {
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);

    long duracion = pulseIn(ECHO, HIGH, 30000);
    float distancia = duracion * 0.034 / 2.0;

    suma += distancia;
    delay(50); // pequeña espera entre lecturas
  }

  float promedio = suma / muestras;

  Serial.print("Distancia: ");
  Serial.print(promedio, 2); // Imprime con 2 decimales
  Serial.println(" cm");

  return promedio;
}


bool hayObstruccion() {
  long distancia = leerDistancia();
  return distancia <= 10; // si es igual o menor a 10 cm entonces es true
}

void mostrarPantalla() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Porc: "); display.println(porciones);
  display.print("Cada: "); display.print(intervaloHoras); display.println(" h");
  display.print("Peso: "); display.print(pesoPlato); display.println(" g");
  if (mostrarTiempoRestante) {
    RtcDateTime ahora = rtc.GetDateTime();
    RtcDateTime proximaHora = horaUltima + intervaloHoras * 3600;
    int32_t segundosRestantes = proximaHora.TotalSeconds() - ahora.TotalSeconds();
    int horasRestantes = segundosRestantes / 3600;
    int minutosRestantes = (segundosRestantes % 3600) / 60;

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Proxima en: ");
    display.print(horasRestantes); display.print("h ");
    display.print(minutosRestantes); display.println("m");

  }
  display.display();
}

void mostrarSirviendo() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Sirviendo...");
  display.display();
}


void servirComida() {
  if (hayObstruccion()) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.printf("Bloqueo en %.2f cm", leerDistancia());
    display.display();
    delay(3000);
    return;
  }

  int pesoActual = balanza.get_units(10);
  int porcionesEnPlato = round(pesoActual / 50.0);
  int porcionesAservir = porciones - porcionesEnPlato;
  if (porcionesAservir <= 0) return;

  // 👉 Aquí mostramos el mensaje
  mostrarSirviendo();

  float vueltas = porcionesAservir / 4.0;  // 4 porciones = 1 vuelta
  int pasos = vueltas * 2048;
  motor.step(pasos);
  horaUltima = rtc.GetDateTime();
  guardarEEPROM();
}


void configurarParametros() {
  // BOTON 1 - Pulsación corta o larga (TARA)
  if (digitalRead(BOTON1)) {
    if (!boton1Presionado) {
      tiempoBoton1 = millis(); // Marca el tiempo en que se presionó
      boton1Presionado = true;
      taraRealizada = false; // Resetea para permitir nueva tara
    } else {
      // Si el botón lleva más de 3 segundos presionado
      if (!taraRealizada && millis() - tiempoBoton1 >= 3000) {
        balanza.tare(); // Realiza la tara
        Serial.println("TARA realizada");
        
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 20);
        display.println("TARA OK");
        display.display();
        delay(1000); // Mostrar el mensaje durante 1 segundo

        taraRealizada = true; // Evita múltiples taras con el mismo pulsado
      }
    }
  } else {
    // Si se suelta el botón
    if (boton1Presionado) {
      // Si NO se hizo la tara => fue una pulsación corta
      if (!taraRealizada) {
        porciones = (porciones + 1) % 21;
        guardarEEPROM();
      }
      boton1Presionado = false; // Resetea el estado
    }
  }


  if (digitalRead(BOTON2)) {
    if (tiempoBotonB2 == 0) tiempoBotonB2 = millis();

    if ((millis() - tiempoBotonB2 >= 5000) && !mostrandoIP) { // pulsación larga
        mostrandoIP = true;
        boton2Procesado = true;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Conectado a Wifi");
        display.print("Red: ");
        display.println(WiFi.SSID());
        display.print("IP: ");
        display.println(WiFi.localIP());
        display.display();
    }
} else {  // cuando el botón se suelta
    if (tiempoBotonB2 != 0 && !boton2Procesado) {  // pulsación corta
        intervaloHoras = (intervaloHoras + 1) % 25;
        guardarEEPROM();
    }
    tiempoBotonB2 = 0;
    mostrandoIP = false;
    boton2Procesado = false;
}

}

void loop() {

  handleAlexa();

  if (servirDesdeAlexa) {
    servirComida();        // 👉 Sirve la comida desde el loop
    servirDesdeAlexa = false;  // 👉 Resetea la bandera
}

 
  pesoPlato = balanza.get_units(5);

  // 👉 Solo refrescar pantalla si NO estamos mostrando la IP
  if (!mostrandoIP) {
      mostrarPantalla();
  }

  configurarParametros();


  // Mostrar red, ip asignada con boton B2 por 5 segundos
  if (digitalRead(BOTON2)) {
    if (tiempoBotonB2 == 0) tiempoBotonB2 = millis();

    if (millis() - tiempoBotonB2 >= 5000 && !mostrandoIP) { // 5 segundos
        mostrandoIP = true;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Conectado a Wifi");
        display.print("Red: ");
        display.println(WiFi.SSID());
        display.print("IP asignada: ");
        display.println(WiFi.localIP());
        display.display();
    }
  } else {
    tiempoBotonB2 = 0;
    mostrandoIP = false;
  }


  //

  if (digitalRead(BOTON1)) {
    if (!boton1Activo) {
        tiempoBoton1 = millis(); // Registra el momento en que se presiona
        boton1Activo = true;
    } else {
        // Si el botón lleva más de 3 segundos presionado
        if (millis() - tiempoBoton1 >= 3000) {
            balanza.tare(); // Realiza la tara
            Serial.println("TARA realizada");

            // Muestra mensaje en pantalla
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(10, 20);
            display.println("TARA OK");
            display.display();
            delay(1000); // Pequeña pausa para que el usuario lo vea

            boton1Activo = false; // Resetea el estado para futuras taretas
        }
    }
} else {
    // Si se suelta el botón antes de los 3 segundos, se resetea
    boton1Activo = false;
}



  if (digitalRead(BOTON3) && !boton3Activo) {
        tiempoInicio = millis();
        boton3Activo = true;
    }

    if (boton3Activo) {
        if (hayObstruccion()) {
            mostrarPantalla();
        }

        if (millis() - tiempoInicio >= 1000) {
            servirComida();
            boton3Activo = false;
        }
    }

  if (digitalRead(BOTON4)) {
    if (tiempoB4 == 0) tiempoB4 = millis();
    if (millis() - tiempoB4 >= 2000) {
      mostrarTiempoRestante = true;
    }
  } else {
    mostrarTiempoRestante = false;
    tiempoB4 = 0;
  }

  RtcDateTime ahora = rtc.GetDateTime();

  // Codigo de intentos de servido automatico 

  if (intervaloHoras > 0) {
      if ((ahora.TotalSeconds() - horaUltima.TotalSeconds()) >= intervaloHoras * 3600) {
          
          // Si no hay obstrucción, servir normalmente
          if (!hayObstruccion()) {
              Serial.println("Sirviendo comida automaticamente...");
              display.clearDisplay();
              display.setTextSize(2);
              display.setCursor(0, 0);
              display.println("Sirviendo...");
              display.display();

              servirComida();

              horaUltima = rtc.GetDateTime();
              guardarEEPROM();
              
              // Reiniciar reintentos por si venía de un bloqueo
              contadorIntentos = 0;
              esperandoReintento = false;

          } else {
              // Si hay obstrucción, comenzar la lógica de reintento
              if (!esperandoReintento) {
                  Serial.println("Obstrucción detectada. Intentando nuevamente en 1 minuto...");
                  
                  display.clearDisplay();
                  display.setTextSize(1);
                  display.setCursor(0, 0);
                  display.println("Obstrucción!");
                  display.println("Reintentando en 1 min...");
                  display.display();

                  tiempoReintento = millis();
                  esperandoReintento = true;
                  contadorIntentos++;
              }
          }
      }
  }

  // Si estamos esperando un reintento
  if (esperandoReintento && (millis() - tiempoReintento >= 60000)) { // 1 minuto
      if (!hayObstruccion()) {
          Serial.println("Obstrucción despejada. Sirviendo comida...");

          display.clearDisplay();
          display.setTextSize(2);
          display.setCursor(0, 0);
          display.println("Sirviendo...");
          display.display();

          servirComida();

          horaUltima = rtc.GetDateTime();
          guardarEEPROM();
          contadorIntentos = 0;
          esperandoReintento = false;
      } else {
          // Sigue la obstrucción
          if (contadorIntentos < maximosIntentos) {
              Serial.println("Obstrucción persiste. Nuevo intento en 1 minuto...");

              display.clearDisplay();
              display.setTextSize(1);
              display.setCursor(0, 0);
              display.println("Obstrucción persiste!");
              display.println("Reintentando en 1 min...");
              display.display();

              tiempoReintento = millis(); // Actualizamos el tiempo para el próximo intento
              contadorIntentos++;
          } else {
              Serial.println("Máximo de intentos alcanzados. No se servirá esta vez.");

              display.clearDisplay();
              display.setTextSize(1);
              display.setCursor(0, 0);
              display.println("Max intentos!");
              display.println("Espere al prox horario");
              display.display();

              // Detenemos reintentos hasta el siguiente ciclo
              esperandoReintento = false;
              contadorIntentos = 0;

              // Nota: La próxima oportunidad será cuando se cumpla el siguiente intervalo programado
          }
      }
  }


  delay(50);
}

void setup() {
  Serial.begin(115200);

  limpiarEEPROMWiFi();


  intentoconexion("ESPGroup4", "12345678");

  setupAlexa(); // alexa


  configurarEndpointsApp();  // Activamos las rutas para la app

  Wire.begin(SDA_PIN, SCL_PIN);
  EEPROM.begin(EEPROM_SIZE);
  // borrarEEPROM(); // ⬅️ SOLO durante pruebas iniciales
  leerEEPROM();

  rtc.Begin();

  balanza.begin(HX711_DT, HX711_SCK);
  balanza.set_scale(factorEscala);
  balanza.tare();


  // Imprimir el estado de calibración
  if (calibrado) {
      Serial.println("El sistema está calibrado.");
      Serial.printf("Factor de escala: ");
      Serial.println(factorEscala);
  } else {
      Serial.println("El sistema NO está calibrado.");
      
  }



  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se encuentra la pantalla OLED");
    while (true);
  }

  setupBotones();
  setupUltrasonico();

  motor.setSpeed(5);

  if (horaUltima.Year() < 2020) {
    horaUltima = rtc.GetDateTime(); // Si la fecha no es válida, usa la actual
    guardarEEPROM(); // Guarda el valor correcto para futuras comparaciones
  }

} 
