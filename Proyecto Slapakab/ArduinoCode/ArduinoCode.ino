#include <ArduinoJson.h>
#include <SPI.h>      // incluye libreria bus SPI
#include <MFRC522.h>      // incluye libreria especifica para MFRC522
#include <array>
#include "procesoOTA.h"   //funciones para OTA
#include "mqttHandler.h"  //Funciones MQTT y Wifi anidada



//pines modulo FRC

#define RST_PIN  5      // (GPIO 5, pin D1) constante para referenciar pin de reset
#define SS_PIN  4       // (GPIO 4, pin D2)constante para referenciar pin de slave select

MFRC522 mfrc522(SS_PIN, RST_PIN);

//Pin cerradura

#define PIN_PUERTA 10  //pin SD3, Low=cerrado, High=abierto
#define N_PUERTA  1 // numero de puerta (cambiar para cada modulo)

//Variables de comunicacion

char mensaje[128];  // cadena de 128 caracteres
char buffer[128];   // cadena de 128 caracteres

//mensaje de control mqtt

const char* abierta = "{\"estado_puerta\":\"abierta\"}" ;
const char* cerrada = "{\"estado_puerta\":\"cerrada\"}" ;

// Vars
unsigned long ahora;

//variables para lectura y comparacion usuarios
byte LecturaUID[4];         // crea array para almacenar el UID leido
struct usuario {
  byte id[4]; 
};
struct usuario usuarios_comunes[4];
bool usuario_conocido = false;


//-----------------------------------------------------

//FUNCIONES

//-----------------------------------------------------
// funcion de procesado de mensaje de ids comunes de la base de datos

void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  char *mensaje = (char *)malloc(length + 1); // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length); // copio el mensaje en cadena de caracteres
  mensaje[length] = '\0'; // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  // compruebo el topic
  if(strcmp(topic,topicSubAbrir)==0){
    if(strcmp(mensaje,"true")==0) 
    abrirPuerta();
    }
  else if(strcmp(topic,topicSubCache)==0){
    Serial.println("Cache recibido");
    StaticJsonDocument<512> root; // el tamaño tiene que ser adecuado para el mensaje
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(root, mensaje);

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error deserializeJson failed: ");
      Serial.print(error.c_str());
      Serial.println();
    }
    else{
      String idAux[4];
      for ( int i = 1; i<=4; i++)
      {
        if(root.containsKey("idComun" + (String)i))
        {
          idAux[i-1] = (String) root["idComun" + (String)i];
          stringIDtoByteID( idAux[i-1], i);
          Serial.println("{\"ID\": \""+idAux[i-1]+"\" }");
          
        }
        else{
        Serial.print("Error : ");
        Serial.println("\"idComun"+(String)i+"\" key not found in JSON");

        }
        
      }

    }
  }
  else{
    Serial.println("Error: Topic desconocido");
  }

  free(mensaje); // libero memoria
  
}

//-----------------------------------------------------

void abrirPuerta(){
  digitalWrite(PIN_PUERTA, HIGH);
  ahora = millis();
  mqtt_client.publish(topicPubEstadoPuerta,abierta,true);
  Serial.println("Puerta abierta\n");
}
void cerrarPuerta(){
  digitalWrite(PIN_PUERTA, LOW);
  mqtt_client.publish(topicPubEstadoPuerta,cerrada,true);
    Serial.println("Puerta cerrada\n");
}

//-----------------------------------------------------

boolean comparaUID(byte lectura[],byte usuario[]) // funcion comparaUID
{
  for (byte i=0; i < mfrc522.uid.size; i++){    // bucle recorre de a un byte por vez el UID
  if(lectura[i] != usuario[i])        // si byte de UID leido es distinto a usuario
    return(false);          // retorna falso
  }
  return(true);           // si los 4 bytes coinciden retorna verdadero
}

//-----------------------------------------------------

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
  pinMode(PIN_PUERTA, OUTPUT);
  digitalWrite(PIN_PUERTA, LOW);

  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topicPubIDmatch,"puerta_%d/ID_detectado/match",N_PUERTA);  
  sprintf(topicPubIDunmatch,"puerta_%d/ID_detectado/unmatch",N_PUERTA);
  sprintf(topicPubEstado,"puerta_%d/estado",N_PUERTA);
  sprintf(topicPubUpdateCache,"puerta_%d/IDs_cache/update",N_PUERTA);
  sprintf(topicPubEstadoPuerta,"puerta_%d/puerta/estado",N_PUERTA);
  
  sprintf(topicSubCache,"puerta_%d/IDs_cache",N_PUERTA);
  sprintf(topicSubAbrir,"puerta_%d/puerta/abrir",N_PUERTA);
  
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  Serial.printf("Termina setup en %lu ms\n\n",millis());
  intenta_OTA();
  Serial.println("----------------------");

  SPI.begin();        // inicializa bus SPI
  mfrc522.PCD_Init();     // inicializa modulo lector
  
}

//-----------------------------------------------------

//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------

//-------------------------------------------------
void loop() {

  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop(); // esta llamada para que la librería recupere el control

  if (millis()-ahora>3000){
    cerrarPuerta();
    ahora=millis();
  }

  if ( !mfrc522.PICC_IsNewCardPresent())   // si no hay una tarjeta presente
    return;           // retorna al loop esperando por una tarjeta
  
  if ( !mfrc522.PICC_ReadCardSerial()){     // si no puede obtener datos de la tarjeta
    Serial.println("no se pueden obtener datos");
    return;           // retorna al loop esperando por otra tarjeta
  }
  
  for (byte i = 0; i < mfrc522.uid.size; i++){      // bucle recorre de a un byte por vez el UID
    LecturaUID[i]=mfrc522.uid.uidByte[i];     
  }
          
  for ( int i = 0; i < 4; i++)
  {
    if(comparaUID(LecturaUID, usuarios_comunes[i].id)){  
      // llama a funcion comparaUID con Usuario1              
      sprintf (buffer,"Bienvenido \n"); // si retorna verdadero muestra texto bienvenida
      Serial.println(buffer);
      usuario_conocido = true;
      snprintf(mensaje, 128, "{\"ID\": \"%d:%d:%d:%d\" }", usuarios_comunes[i].id[0], usuarios_comunes[i].id[1], usuarios_comunes[i].id[2], usuarios_comunes[i].id[3]);

      mqtt_client.publish(topicPubIDmatch, mensaje);
      abrirPuerta();
    }
  }
  
  if (usuario_conocido == false){
    Serial.println("No te conozco");    // muestra texto equivalente a acceso denegado
    snprintf(mensaje, 128, "{\"ID\": \"%d:%d:%d:%d\" }",LecturaUID[0],LecturaUID[1],LecturaUID[2],LecturaUID[3]);
    mqtt_client.publish(topicPubIDunmatch, mensaje);
  }
      
  mfrc522.PICC_HaltA();    // detiene comunicacion con tarjeta 
  usuario_conocido = false;
  
}
