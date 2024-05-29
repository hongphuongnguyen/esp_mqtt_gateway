#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <coap-simple.h>

#define ssid "Kien"
#define pass "abcde123"

#define mqtt_server         "192.168.1.7"
#define thingsboard_server  "192.168.1.7"

#define coap_server         "192.168.1.6"

#define accessToken         "PD3S3LMavuogQB7AV8EY"
#define THINGSBOARD_TOPIC_SUB "v1/devices/me/attributes"
#define THINGSBOARD_TOPIC_PUB "v1/devices/me/telemetry"

#define MOSQUITTO_TOPIC_RES "/response"
#define MOSQUITTO_TOPIC_PUB "/actuator/HVAC"

#define ROOM_ID   "room1"

typedef struct {
  char type[12];
  bool status;
  float set_point;
}__attribute__((packed))  Controller_t;

typedef struct {
  float high;
  float low;
}__attribute__((packed)) Threshole_t;

typedef struct {
  Threshole_t threshole;
  float sensing;
  float set_point;
}__attribute__((packed)) Environ_element_t;

typedef struct {
  uint8_t room_id;
  Controller_t hvac;
  Controller_t light;
  Environ_element_t temp;
  Environ_element_t humi;
  Environ_element_t air;
}__attribute__((packed)) Gateway_element_t;

WiFiClient espClient1;
WiFiClient espClient2;
PubSubClient client_thingsboard(espClient1);
PubSubClient client_mosquitto(espClient2);

WiFiUDP light_udp;
Coap light_coap(light_udp);

QueueHandle_t ctrl_queue;

Controller_t tmp_controler = {
  .type = "temp"
};

Gateway_element_t stored_data = {
  .room_id = 1
};


void coap_light_response(CoapPacket &packet, IPAddress ip, int port){
    Serial.println("[Coap Response got]");
  
    char p[packet.payloadlen + 1];
    memcpy(p, packet.payload, packet.payloadlen);
    p[packet.payloadlen] = NULL;
    
    Serial.println(p);
}

void thingsboard_callback(char* topic, byte* payload, unsigned int length) {
  String data;
  int idx = 0;
  

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  Serial.println("");
  delay(2000);
  // callback nhan tin hieu dieu khien tu thingsboard va publish tin hieu dieu khien ve HVAC
  if(String(topic) == THINGSBOARD_TOPIC_SUB){
    Serial.print("Message form Thingsboard: ");
    Serial.println(data);

    if(data.indexOf(":") != -1){
      idx = data.indexOf(":");
      strcpy(tmp_controler.type, data.substring(1, idx).c_str());
      tmp_controler.set_point = data.substring(idx+1).toFloat();
      xQueueSend(ctrl_queue, &tmp_controler, 0);
    }
  }

}

void mosquitto_callback(char * topic, byte* payload, unsigned int length){
  String data;
  int idx = 0;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  Serial.println("");

  if (String(topic) == "sensor/DHT11/temp") {
    String payload = "{temperature:";
    payload += data;
    payload += "}";
    client_mosquitto.publish(THINGSBOARD_TOPIC_PUB, payload.c_str());
    delay(2000);
    Serial.println("Published data to Thingsboard: " + payload);
  }
  String topic_response = String(ROOM_ID) + String(MOSQUITTO_TOPIC_RES);

  // callback kich hoat khi co phan hoi tu HVAC ve ket qua sau khi thu thi
  
  if(String(topic) == topic_response){
    if(data.indexOf("HVAC") != -1){
      if(data.indexOf("disconnected") != -1){
        //can publish trang thai mat ket noi cho thingsboard
        Serial.println("HVAC Disconnected");
        client_thingsboard.publish(THINGSBOARD_TOPIC_PUB, "{\"status\": false}");
        stored_data.hvac.status = 0;
      }
      else if(data.indexOf("OK") != -1){
        //cap nhat set point luu o gateway
      }
      else if(data.indexOf("Connected") != -1){
        //cap nhat set point luu o gateway
        Serial.println("HVAC connected");
        client_thingsboard.publish(THINGSBOARD_TOPIC_PUB, "{\"status\": true}");
        stored_data.hvac.status = 1;
      }
      else{
        // luu loi chua duoc xu ly
      }
    }
  }
}

void control_task( void *arg){
  char pub_topic[40];
  String data;
  memset((void *)pub_topic, 0, 40);
  strcat(pub_topic, ROOM_ID);
  strcat(pub_topic, MOSQUITTO_TOPIC_PUB);
  while(1){
    while(xQueueReceive(ctrl_queue, &tmp_controler, 0)){
      data = String(tmp_controler.type) + ":" + String(tmp_controler.set_point, 1);
      Serial.println("Controlling...");
      if(String(tmp_controler.type) == "\"temp\""){
        stored_data.temp.set_point = tmp_controler.set_point;
        client_mosquitto.publish(pub_topic, data.c_str(), (bool)false);
      }
      else if(String(tmp_controler.type) == "\"humidity\""){
        Serial.println(data);
        stored_data.humi.set_point = tmp_controler.set_point;
        client_mosquitto.publish(pub_topic, data.c_str(), (bool)false);

      }
      else if(String(tmp_controler.type) == "\"air\""){
        stored_data.air.set_point = tmp_controler.set_point;
        client_mosquitto.publish(pub_topic, data.c_str(), (bool)false);
      }
      else if(String(tmp_controler.type)== "\"light\""){
        Serial.println(data);
        stored_data.light.set_point = tmp_controler.set_point;
        light_coap.put(coap_server, 5683, "light", String(tmp_controler.set_point, 0).c_str());
      }
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  String topic_response = String(ROOM_ID) + String(MOSQUITTO_TOPIC_RES);

  while(!client_thingsboard.connected()){
    if (client_thingsboard.connect("ESP32Client", accessToken, NULL)) {
      Serial.println("Connected to Thingsboard Broker!");

      // subscribe vao topic de nhan cap nhat set point tu thingsboard
      client_thingsboard.subscribe(THINGSBOARD_TOPIC_SUB, 1);
      Serial.print("Subscribed to topic: ");
      Serial.println(THINGSBOARD_TOPIC_SUB);
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
    
  while(!client_mosquitto.connected()){
    Serial.println("Attempting MQTT connection...");
    if (client_mosquitto.connect("ESP32Gateway")) {
      Serial.println("Connected to Mosquitto Broker!");

      client_mosquitto.subscribe("sensor/DHT11/temp");
      Serial.println("Subscribed to topic sensor/DHT11/temp");
      // subscribe vao topic de nghe phan hoi tu thiet bi
      client_mosquitto.subscribe(topic_response.c_str(), 1);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_response);
    } else {
      Serial.println("Connect to mosquitto failed, try again in 5 seconds!");
      
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
  
    // } else {
    //   Serial.println("Connect to Mosquitto Broker failed, try again in 5 seconds!");
    //   delay(5000)
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client_thingsboard.setServer(thingsboard_server, 1883);
  client_mosquitto.setServer(mqtt_server, 1884);
  client_thingsboard.setCallback(thingsboard_callback);
  client_mosquitto.setCallback(mosquitto_callback);

  ctrl_queue = xQueueCreate(3, sizeof(Controller_t));

  light_coap.response(coap_light_response);
  light_coap.start();

  xTaskCreatePinnedToCore(control_task, "hvac task", 2048, NULL, 1, NULL, 1);
}

void loop() {
  if (!client_thingsboard.connected() || !client_mosquitto.connected()) {
    reconnect();
  }
  client_thingsboard.loop();
  client_mosquitto.loop();
  light_coap.loop();
}
