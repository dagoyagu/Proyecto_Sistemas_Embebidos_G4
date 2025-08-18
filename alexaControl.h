#ifndef ALEXA_CONTROL_H
#define ALEXA_CONTROL_H

#include <SinricPro.h>
#include <SinricProSwitch.h>


extern volatile bool servirDesdeAlexa;
extern volatile bool servirDesdeAlexa;

// 👉 Declarar como externas
extern void servirComida(); // Ya tienes esta función en tu main




// 👉 Tus credenciales de Sinric Pro
#define APP_KEY "8c5fe701-d118-4700-81dc-d0d3e7ee79e5"
#define APP_SECRET "b62381bd-a0fe-4cf2-9619-f6e7e422c7ab-82844d00-a896-4535-9ed4-6071f0521d52"
#define SWITCH_ID "68574dd8030990a558ba6f8a"




// 👉 Callback que manejará el comando desde Alexa  // ( la cambié ahora )
bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId == SWITCH_ID) {
    if (state) {
      Serial.println("Alexa: Servir comida");
      servirDesdeAlexa = true;  // 👉 solo marca para servir luego
    } else {
      Serial.println("Alexa: Apagado");
    }
  }
  return true;  // 👉 Siempre responder inmediatamente
}


// 👉 Configurar Sinric Pro (ya estás conectado a WiFi desde el main)
void setupAlexa() {
  // No necesitas conectar al WiFi aquí. Ya estás conectado.

  // Configurar el Switch
  SinricProSwitch& miSwitch = SinricPro[SWITCH_ID];
  miSwitch.onPowerState(onPowerState);

  // Iniciar Sinric Pro
  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(false);

}

// 👉 Manejador de SinricPro para el loop principal
void handleAlexa() {
  SinricPro.handle();
}

#endif
