#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <SPI.h>      // incluye libreria bus SPI
#include <MFRC522.h>      // incluye libreria especifica para MFRC522
#include <array>


//ADC_MODE(ADC_VCC);

//pines modulo FRC
#define RST_PIN  5      // constante para referenciar pin de reset
#define SS_PIN  4      // constante para referenciar pin de slave select

MFRC522 mfrc522(SS_PIN, RST_PIN);

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
const char* ssid = "MiFibra-B810";
const char* password = "Kzyr4Uym";
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
const char* abierta = "{\"open\":true}" ;
const char* cerrada = "{\"open\":false}" ;
//GPIOs

int pinPuerta = 1;  //Low=cerrado, High=abierto

//int LED = 2; 


// Vars
unsigned long ahora;


// funciones para progreso de OTA
void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);

//variables para lectura y comparacion usuarios
byte LecturaUID[4];         // crea array para almacenar el UID leido
byte Usuario1[4]= {0x00, 0x00, 0x00, 0x00} ;    // UID de tarjeta leido en programa 1
byte Usuario2[4]= {0x00, 0x00, 0x00, 0x00} ;    // UID de llavero leido en programa 1
byte Usuario3[4]= {0x00, 0x00, 0x00, 0x00} ;
byte Usuario4[4]= {0x00, 0x00, 0x00, 0x00} ;
struct usuario {
  String nombre;
  byte id[4]; 
};
struct usuario usuarios_comunes[4];
bool usuario_conocido = false;
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

// funcion de procesado de mensaje de ids comunes de la base de datos

void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  //String idAux ;
  char *mensaje = (char *)malloc(length + 1); // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length); // copio el mensaje en cadena de caracteres
  mensaje[length] = '\0'; // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  // compruebo el topic
    if(strcmp(topic,"puerta_1/IDs_cache")==0)
  {
    Serial.print("me llega el topic");
    StaticJsonDocument<512> root; // el tamaño tiene que ser adecuado para el mensaje
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(root, mensaje);

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    else
    if(root.containsKey("idComun1"))  // comprobar si existe el campo/clave que estamos buscando
    {
      String idAux1 = root["idComun1"];
      stringIDtoByteID( idAux1, 0);
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"idComun1\" key not found in JSON");
    }
    if(root.containsKey("idComun2"))  // comprobar si existe el campo/clave que estamos buscando
    {
       String idAux2 = root["idComun2"];
      stringIDtoByteID( idAux2, 1);
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"idComun2\" key not found in JSON");
    }
    if(root.containsKey("idComun3"))  // comprobar si existe el campo/clave que estamos buscando
    {
      String idAux3 = root["idComun3"];
      stringIDtoByteID( idAux3, 2);
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"idComun3\" key not found in JSON");
    }
    if(root.containsKey("idComun4"))  // comprobar si existe el campo/clave que estamos buscando
    {
      String idAux4 = root["idComun4"];
      stringIDtoByteID( idAux4, 3);
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"idComun4\" key not found in JSON");
    }    
    
  } 
  else
  {
    Serial.println("Error: Topic desconocido");
  }

  free(mensaje); // libero memoria
  
}

//-----------------------------------------------------

void abrirPuerta(){
  digitalWrite(pinPuerta, HIGH);
  ahora = millis();
  mqtt_client.publish(topicPubEstadoPuerta,abierta,true);
  Serial.printf("Puerta abierta\n\n");
}
void cerrarPuerta(){
  digitalWrite(pinPuerta, LOW);
  mqtt_client.publish(topicPubEstadoPuerta,cerrada,true);
    Serial.printf("Puerta cerrada\n\n");
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
  pinMode(pinPuerta, OUTPUT);    
  digitalWrite(pinPuerta, LOW);

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
  //conecta_mqtt();
  Serial.printf("Termina setup en %lu ms\n\n",millis());
  //intenta_OTA();
  Serial.println("----------------------");

  SPI.begin();        // inicializa bus SPI
  mfrc522.PCD_Init();     // inicializa modulo lector
//  Iguala_Array(Usuario1, 0);
//  Iguala_Array(Usuario2, 1);
//  Iguala_Array(Usuario3, 2);
//  Iguala_Array(Usuario4, 3);
  usuarios_comunes[0].nombre = "juan";
  usuarios_comunes[1].nombre = "pedro";
  usuarios_comunes[2].nombre = "nada1";
  usuarios_comunes[3].nombre = "nada2";
  
  
}

//-----------------------------------------------------

//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------

//-------------------------------------------------
void loop() {
<<<<<<< Updated upstream
  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop(); // esta llamada para que la librería recupere el control

  if (millis()-ahora>2000) cerrarPuerta();
  
  delay(1);
=======
   char mensaje[128];  // cadena de 128 caracteres
   char buffer[128];  // cadena de 128 caracteres
//  if (!mqtt_client.connected()) conecta_mqtt();
//  mqtt_client.loop(); // esta llamada para que la librería recupere el control
//  delay(1);
    if ( ! mfrc522.PICC_IsNewCardPresent())   // si no hay una tarjeta presente
    return;           // retorna al loop esperando por una tarjeta
  
  if ( ! mfrc522.PICC_ReadCardSerial())     // si no puede obtener datos de la tarjeta
    {
      Serial.println("no se pueden obtener datos");
      return;           // retorna al loop esperando por otra tarjeta
    }
    
    Serial.print("UID leido:");       // muestra texto UID:
    for (byte i = 0; i < mfrc522.uid.size; i++) 
      { // bucle recorre de a un byte por vez el UID
        if (mfrc522.uid.uidByte[i] < 0x10){   // si el byte leido es menor a 0x10
          Serial.print(" 0");       // imprime espacio en blanco y numero cero
        }
        else{           // sino
          Serial.print(" ");        // imprime un espacio en blanco
        }
        Serial.print(mfrc522.uid.uidByte[i], DEC);    // imprime el byte del UID leido en hexadecimal
        LecturaUID[i]=mfrc522.uid.uidByte[i];
         
      }
          
    Serial.print("\t");         // imprime un espacio de tabulacion             

    for ( int i = 0; i < 4; i++)
    {
      if(comparaUID(LecturaUID, usuarios_comunes[i].id))
      {
        // llama a funcion comparaUID con Usuario1
                
        sprintf (buffer,"Bienvenido Usuario: %s \n", usuarios_comunes[i].nombre); // si retorna verdadero muestra texto bienvenida
        Serial.print (buffer);
        usuario_conocido = true;
        snprintf(mensaje, 128, "{\n\"usuario conocido\": %s, \"id usuario: %d:%d:%d:%d }",usuarios_comunes[i].nombre , usuarios_comunes[i].id[0], usuarios_comunes[i].id[1], usuarios_comunes[i].id[2], usuarios_comunes[i].id[3]);
        Serial.print (mensaje);
        
//        snprintf(mensaje, 128, "{\"usuario conocido\": %s, \"id usuario: %02X:%02X:%02X:%02X }",usuarios_comunes[i].nombre , usuarios_comunes[i].id[0], usuarios_comunes[i].id[1], usuarios_comunes[i].id[2], usuarios_comunes[i].id[3]);
//        Serial.print (mensaje);
        mqtt_client.publish(topicPubIDmatch, mensaje);
        
      }
      
    }    
    if (usuario_conocido == false)
      {
      Serial.println("No te conozco");    // muestra texto equivalente a acceso denegado
      snprintf(mensaje, 128, "{\"usuario deconocido id\": %02X:%02X:%02X:%02X }",LecturaUID[0],LecturaUID[1],LecturaUID[2],LecturaUID[3]);
      mqtt_client.publish(topicPubIDunmatch, mensaje);
      }
      
    mfrc522.PICC_HaltA();    // detiene comunicacion con tarjeta 
    usuario_conocido = false;
}

boolean comparaUID(byte lectura[],byte usuario[]) // funcion comparaUID
{
  for (byte i=0; i < mfrc522.uid.size; i++){    // bucle recorre de a un byte por vez el UID
  if(lectura[i] != usuario[i])        // si byte de UID leido es distinto a usuario
    return(false);          // retorna falso
  }
  return(true);           // si los 4 bytes coinciden retorna verdadero
}

void Iguala_Array( byte usuarioN[4], int user)
{
  
    for (int i = 0; i < 4; i ++)
    {
    usuarios_comunes[user].id[i] = usuarioN[i];
    }
    //lineas de comprobacion de la actulizacion de la base de datos
//    for (byte i = 0; i < 4; i++) 
//    { // bucle recorre de a un byte por vez el UID
//      if (usuarios_comunes[user].id[i] < 0x10){   // si el byte leido es menor a 0x10
//        Serial.print(" 0");       // imprime espacio en blanco y numero cero
//      }
//      else{           // sino
//        Serial.print(" ");        // imprime un espacio en blanco
//      }
//      Serial.print(usuarios_comunes[user].id[i], HEX);    // imprime el byte del UID leido en hexadecimal
      
       
//    }
 
    
}

void stringIDtoByteID( String data, int user)
{
  byte prevPos = data.indexOf(':');                       // Buscar la posición de la primera coma en la cadena
  byte currPos = 0 ;                                      
                                                          
  
  for (byte i = 0; i <3; i++) {
    usuarios_comunes[user].id[i]= data.substring(currPos,prevPos).toInt(); // Extrae la subcadena que contiene un número, luego se convierte en un valor
                                                         // númerico de variable.
    currPos= prevPos + 1;                                // Como ahora trabajamos con dos posiciones y necesitamos pasar a la siguiente
                                                          // subcadena, la "posición actual" ahora es la anterior. No olvidemos el
                                                          // desplazamiento de índice.
    prevPos = data.indexOf(':', prevPos + 1); // La "posición actual" ahora es la próxima ocurrencia del "caracter separador"
                                                            // (en este caso, una coma).
  }                                                       // Y todo esto se repite 5 veces más.

  usuarios_comunes[user].id[3]= data.substring(currPos).toInt();
  
>>>>>>> Stashed changes
}
