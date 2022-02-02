#include <PubSubClient.h>
#include "wifiHandler.h"

//mensaje de control mqtt

const char* online = "{\"online\":true}" ;
const char* offline = "{\"online\":false}" ;


//variables necesarias para el manejo de MQTT

PubSubClient mqtt_client(wClient);

char ID_PLACA[16];
char topicSubActivacion[256];
char topicPubIDnewCard[256];
char topicPubEstado[256];

  

//credenciales mqtt

const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* mqtt_server = "192.168.17.94";

void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass,topicPubEstado,0,true,offline)) {  //connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
      Serial.printf(" conectado a broker: %s\n",mqtt_server);
      mqtt_client.subscribe(topicSubActivacion);
      mqtt_client.publish(topicPubEstado,online,true);
            
    } 
    else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
