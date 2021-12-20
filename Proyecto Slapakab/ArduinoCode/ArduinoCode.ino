#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

//ADC_MODE(ADC_VCC);

// datos para actualización   >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("192.168.31.94")         // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu" 

// numero de puerta (cambiar para cada modulo)
#define N_PUERTA  1

WiFiClient wClient;
PubSubClient mqtt_client(wClient);

// Update these with values suitable for your network.
const char* ssid = "masdoritos";
const char* password = "12345678";
const char* mqtt_server = "192.168.31.94";

const char* mqtt_user = "";
const char* mqtt_pass = "";


// Definiciones de chars (nombres para topics y otros datos)

char ID_PLACA[16];
char topicPubIDmatch[256];
char topicPubIDunmatch[256];
char topicPubEstado[256];
char topicPubUpdateCache[256];
char topicPubEstadoPuerta[256];

char topicSubCache[256];
char topicSubAbrir[256];

const char* online = "{\"online\":true}" ;
const char* offline = "{\"online\":false}" ;

// GPIOs

//int LED = 2; 


// Vars
//unsigned long ahora;


// funciones para progreso de OTA
void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);


//-----------------------------------------------------


void intenta_OTA()
{ 
  Serial.println( "-------------------------" );  
  Serial.println( "Comprobando actualización:" );
  Serial.print(HTTP_OTA_ADDRESS);Serial.print(":");Serial.print(HTTP_OTA_PORT);Serial.println(HTTP_OTA_PATH);
  Serial.println( "--------------------------" );  
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
  ESPhttpUpdate.onStart(inicio_OTA);
  ESPhttpUpdate.onError(error_OTA);
  ESPhttpUpdate.onProgress(progreso_OTA);
  ESPhttpUpdate.onEnd(final_OTA);
  WiFiClient wClient;
  switch(ESPhttpUpdate.update(wClient, HTTP_OTA_ADDRESS, HTTP_OTA_PORT, HTTP_OTA_PATH, HTTP_OTA_VERSION)) {
    case HTTP_UPDATE_FAILED:
      Serial.printf(" HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F(" El dispositivo ya está actualizado"));
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F(" OK"));
      break;
    }
}

//-----------------------------------------------------
void final_OTA()
{
  Serial.println("Fin OTA. Reiniciando...");
}

void inicio_OTA()
{
  Serial.println("Nuevo Firmware encontrado. Actualizando...");
}

void error_OTA(int e)
{
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);
}

void progreso_OTA(int x, int todo)
{
  char cadena[256];
  int progress=(int)((x*100)/todo);
  if(progress%10==0)
  {
    snprintf(cadena,256,"Progreso: %d%% - %dK de %dK",progress,x/1024,todo/1024);
    Serial.println(cadena);
  }
}

//-----------------------------------------------------
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

//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass,topicPubEstado,0,true,offline)) {  //connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
      Serial.printf(" conectado a broker: %s\n",mqtt_server);
      mqtt_client.subscribe(topicSubCache);
      mqtt_client.subscribe(topicSubAbrir);
      mqtt_client.publish(topicPubEstado,online,true);
    } else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------

void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  
}

//-----------------------------------------------------
//     SETUP
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  pinMode(LED_BUILTIN, OUTPUT);    // inicializa GPIO como salida  
  digitalWrite(LED_BUILTIN, HIGH); // apaga el led

  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topicPubIDmatch,"puerta_%d/ID_detectado/match",N_PUERTA);  
  sprintf(topicPubIDunmatch,"puerta_%d/ID_detectado/unmatch",N_PUERTA);
  sprintf(topicPubEstado,"puerta_%d/estado",N_PUERTA);
  sprintf(topicPubUpdateCache,"puerta_%d/IDs_cache/update",N_PUERTA);
  sprintf(topicPubEstadoPuerta,"puerta_%d/puerta/estado",N_PUERTA);
  
  sprintf(topicSubCache,"puerta_%d/IDs_cache");
  sprintf(topicSubAbrir,"puerta_%d/puerta/abrir");
  
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  Serial.printf("Termina setup en %lu ms\n\n",millis());
  intenta_OTA();
  Serial.println("----------------------");
}

//-----------------------------------------------------

//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------

//-------------------------------------------------
void loop() {
  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop(); // esta llamada para que la librería recupere el control
  delay(1);
}
