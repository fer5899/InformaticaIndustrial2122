#include <ArduinoJson.h>
#include <SPI.h>      // incluye libreria bus SPI
#include <MFRC522.h>      // incluye libreria especifica para MFRC522
#include <array>

#include "mqttHandler.h"

//pines modulo FRC
#define RST_PIN  5      // constante para referenciar pin de reset
#define SS_PIN  4       // constante para referenciar pin de slave select
MFRC522 mfrc522(SS_PIN, RST_PIN);
byte LecturaUID[4];

char mensaje[128];  // cadena de 128 caracteres
char IDnew[128];  


void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  char *mensaje = (char *)malloc(length + 1); // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length); // copio el mensaje en cadena de caracteres
  mensaje[length] = '\0'; // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  // compruebo el topic
  if(strcmp(topic,topicSubActivacion)==0){
      if(strcmp(mensaje,"true")==0)
       
        LeerID(0);
  }

  else{
    Serial.println("Error: Topic desconocido");
  }

  free(mensaje); // libero memoria
  
}

 void LeerID(bool leido){
  while(leido==0)
  {
    if (mfrc522.PICC_IsNewCardPresent())   // si no hay una tarjeta presente    
    {
      if (mfrc522.PICC_ReadCardSerial()){     // si no puede obtener datos de la tarjeta

      leido = 1;
                 
      }
    }
  }
    for (byte i = 0; i < mfrc522.uid.size; i++){      // bucle recorre de a un byte por vez el UID
      LecturaUID[i]=mfrc522.uid.uidByte[i];     
    }

    snprintf(IDnew, 128, "{\"ID\": \"%d:%d:%d:%d\" }",LecturaUID[0],LecturaUID[1],LecturaUID[2],LecturaUID[3]);
    mqtt_client.publish(topicPubIDnewCard, IDnew);
    mfrc522.PICC_HaltA();
    
  }

  

void setup() {
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topicPubIDnewCard,"Lector/ID");
  sprintf(topicSubActivacion,"Lector/control");
  sprintf(topicPubEstado,"Lector/estado");

  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  
  SPI.begin();        // inicializa bus SPI
  mfrc522.PCD_Init();     // inicializa modulo lector

  Serial.printf("Termina setup en %lu ms\n\n",millis());
  


}

void loop() {
  
  // put your main code here, to run repeatedly:

}
