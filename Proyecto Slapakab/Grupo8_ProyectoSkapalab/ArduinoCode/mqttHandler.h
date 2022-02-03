#include <PubSubClient.h>
#include "wifiHandler.h"

//mensaje de control mqtt

const char* online = "{\"online\":true}" ;
const char* offline = "{\"online\":false}" ;


//variables necesarias para el manejo de MQTT

PubSubClient mqtt_client(wClient);

char ID_PLACA[16];
char topicPubIDmatch[256];
char topicPubIDunmatch[256];
char topicPubEstado[256];
char topicPubUpdateCache[256];
char topicPubEstadoPuerta[256];
char topicSubCache[256];
char topicSubAbrir[256];

//credenciales servidor UMA

const char* mqtt_user = "II8";
const char* mqtt_pass = "kS0Oooyj";
const char* mqtt_server = "iot.ac.uma.es";


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
      mqtt_client.publish(topicPubUpdateCache,"true");      
    } 
    else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
