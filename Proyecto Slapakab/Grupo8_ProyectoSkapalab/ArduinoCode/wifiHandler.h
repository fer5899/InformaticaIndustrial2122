#include <ESP8266WiFi.h>

//variables necesarias para manejar la conexion wifi

WiFiClient wClient;

const char* ssid = "masdoritos";
const char* password = "12345678";

//----------------------------------------

void conecta_wifi() {
  Serial.printf("\nConnecting to %s:\n", ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
}