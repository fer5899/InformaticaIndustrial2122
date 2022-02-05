#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include "DHTesp.h"
ADC_MODE(ADC_VCC);


// datos para actualización   >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("192.168.137.1")         // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu" 

WiFiClient wClient;
PubSubClient mqtt_client(wClient);
DHTesp dht;

// Update these with values suitable for your network.
const char* ssid = "masdoritos";
const char* password = "12345678";
const char* mqtt_server = "192.168.137.1";
const char* mqtt_user = "";//infind";
const char* mqtt_pass = "";//zancudo";
const char* online = "{\"online\":true}" ;
const char* offline = "{\"online\":false}" ;

char ID_PLACA[16];
char topicPubDatos[256];
char topic_estado[256];//"infind/GRUPO8/conexion"
char topicSubLed[256];
char topicLedStatus[256];


// GPIOs
int LED1 = 2;  
int LED2 = 16;
int estadoLed = 0; 

// ------
unsigned long ultimo_mensaje=0;
unsigned long ahora;
char mensaje[192];

// AÑADIDO NUEVA VERSIÓN

struct registro_datos {
  float temperatura;
  float humedad;
  unsigned long tiempo;
  float alimentacion;
  int led;
  String redSSID;
  String redIP;
  float redRSSI;
  };

int valorLED;
int valorLEDact;

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
  ESPhttpUpdate.setLedPin(LED2, LOW);
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
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass,topic_estado,0,true,offline)) {  //connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
      Serial.printf(" conectado a broker: %s\n",mqtt_server);
      mqtt_client.publish(topic_estado,online,true);
      mqtt_client.subscribe(topicSubLed);
      
    } else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  char *mensaje = (char *)malloc(length + 1); // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length); // copio el mensaje en cadena de caracteres
  mensaje[length] = '\0'; // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  // compruebo el topic
    if(strcmp(topic,"infind/GRUPO8/led/cmd")==0)
  {
    // Serial.print("me llega el topic");
    StaticJsonDocument<512> root; // el tamaño tiene que ser adecuado para el mensaje
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(root, mensaje);

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    else
    if(root.containsKey("level"))  // comprobar si existe el campo/clave que estamos buscando
    {
     valorLED = root["level"];
     Serial.print("Mensaje OK, level = ");
     Serial.println(valorLED);
       if (valorLEDact != valorLED) {
      
        control_led();
       
        valorLEDact = valorLED;
       }
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"level\" key not found in JSON");
    }
  } // if topic
  else
  {
    Serial.println("Error: Topic desconocido");
  }

  free(mensaje); // libero memoria
  
}

//-----------------------------------------------------

// AÑADIDO NUEVA VERSIÓN

String serializaJSONDatos (struct registro_datos datos)
{
  StaticJsonDocument<192> jsonRoot;
  String jsonString;
 
  jsonRoot["Uptime"]= datos.tiempo;
  jsonRoot["Vcc"]= datos.alimentacion;
  jsonRoot["LED"]= datos.led;
  JsonObject sDHT11=jsonRoot.createNestedObject("sDHT11");
  sDHT11["temp"] = datos.temperatura; // CUIDAO
  sDHT11["hum"] = datos.humedad;
  JsonObject Wifi=jsonRoot.createNestedObject("Wifi");
  Wifi["SSID"] = datos.redSSID;
  Wifi["IP"] = datos.redIP;
  Wifi["RSSI"] = datos.redRSSI;
  
  
  serializeJson(jsonRoot,jsonString);
  return jsonString;
}

static void control_led(){

  int LEDPWM = map(valorLED,0,100,255,0);

  analogWrite(LED1,LEDPWM);
  
  estadoLed = valorLED;

  snprintf(mensaje, 128, "{\"Estado_LED\": %d}",estadoLed);
  Serial.println(mensaje);
  mqtt_client.publish(topicLedStatus, mensaje);
  digitalWrite(LED2, LOW); // enciende el led al enviar mensaje
  
   
} 

//-----------------------------------------------------

// AÑADIDO NUEVA VERSIÓN

registro_datos leerDatos () {

  registro_datos datos;

  delay(dht.getMinimumSamplingPeriod());
  
  datos.temperatura = float(dht.getTemperature());

  datos.humedad = float(dht.getHumidity());

  datos.alimentacion = float(ESP.getVcc()) / 1000;

  datos.led = estadoLed; // LEER NIVEL DE PWM DEL LED

  datos.tiempo = millis();

  datos.redSSID = WiFi.SSID();

  datos.redIP = WiFi.localIP().toString();

  datos.redRSSI = float(WiFi.RSSI());

  return datos;
}


//-----------------------------------------------------
//     SETUP
//-----------------------------------------------------
void setup() {
  valorLEDact=0;
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  pinMode(LED1, OUTPUT);    // inicializa GPIO como salida
  pinMode(LED2, OUTPUT);    
  digitalWrite(LED1, HIGH); // apaga el led
  digitalWrite(LED2, HIGH); 

  // AÑADIDO NUEVA VERSIÓN

  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topicPubDatos,"infind/GRUPO8/datos");
  sprintf(topic_estado,"infind/GRUPO8/conexion");
  sprintf(topicSubLed, "infind/GRUPO8/led/cmd");
  sprintf(topicLedStatus, "infind/GRUPO8/led/status");
  
  
  
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  dht.setup(5, DHTesp::DHT11);
  Serial.printf("Termina setup en %lu ms\n\n",millis());
  intenta_OTA();
  Serial.println("----------V4------------");
}

//-----------------------------------------------------


//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------

//-------------------------------------------------
void loop() {
  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop(); // esta llamada para que la librería recupere el control
  
  ahora = millis();

  registro_datos datos;

  
  if (ahora - ultimo_mensaje >= 5000) {

    
    // AÑADIDO NUEVA VERSIÓN
    
    ultimo_mensaje = ahora;

    datos = leerDatos();


    snprintf(mensaje, 192, serializaJSONDatos(datos).c_str());

    Serial.println(mensaje);
    mqtt_client.publish(topicPubDatos, mensaje);

    
    digitalWrite(LED2, LOW); // enciende el led al enviar mensaje
  }
  if (digitalRead(LED2)==LOW && ahora-ultimo_mensaje>=100) 
    digitalWrite(LED2, HIGH); 
}
