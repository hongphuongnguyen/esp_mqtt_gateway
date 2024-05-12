#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define ssid "Kien"
#define pass "abcde123"

#define mqtt_server "192.168.1.6"

#define thingsboard_server  "mqtt.thingsboard.cloud"
#define accessToken         "e6apOql9bEc1Wh4MDpCK"
#define THINGSBOARD_TOPIC_SUB "v1/devices/me/attributes"
#define THINGSBOARD_TOPIC_PUB "v1/devices/me/telementry"

#define MOSQUITTO_TOPIC_RES "/response"
#define MOSQUITTO_TOPIC_PUB "/actuator/HVAC"

#define ROOM_ID   "room1"

typedef struct {
  char *type;
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
  Environ_element_t temp;
  Environ_element_t humi;
  Environ_element_t air;
}__attribute__((packed)) Gateway_element_t;

WiFiClient espClient;
PubSubClient client_thingsboard(espClient);
PubSubClient client_mosquitto(espClient);

QueueHandle_t ctrl_queue;

Controller_t tmp_controler = {
  .type = "temp"
};

Gateway_element_t stored_data = {
  .room_id = 1
};

void callback(char* topic, byte* payload, unsigned int length) {
  String data;
  

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  Serial.println("");
  delay(2000);
  if (client_mosquitto.connect("ESP32Client", accessToken, NULL) && String(topic) == "sensor/DHT11/temp") {
    String payload = "{temperature:";
    payload += data;
    payload += "}";
    client_mosquitto.publish(THINGSBOARD_TOPIC_PUB, payload.c_str());
    delay(2000);
    Serial.println("Published data to Thingsboard: " + payload);
  }


  // callback nhan tin hieu dieu khien tu thingsboard va publish tin hieu dieu khien ve HVAC
  if(String(topic) == THINGSBOARD_TOPIC_SUB){
    
    if(data.indexOf(":") != -1){
      int idx = data.indexOf(":");
      strcpy(tmp_controler.type, data.substring(0, idx - 1).c_str());
      tmp_controler.set_point = data.substring(idx+1).toFloat();
      xQueueSend(ctrl_queue, &tmp_controler, 0);
    }
  }

  // callback kich hoat khi co phan hoi tu HVAC ve ket qua sau khi thu thi
  if(String(topic) == MOSQUITTO_TOPIC_RES){
    if(data.indexOf("HVAC") != -1){
      if(data.indexOf("disconnected") != -1){
        //can publish trang thai mat ket noi cho thingsboard
        stored_data.hvac.status = 0;
      }
      else if(data.indexOf("OK") != -1){
        //cap nhat set point luu o gateway
      }
      else if(data.indexOf("Connected") != -1){
        //cap nhat set point luu o gateway
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
  while(1){
    while(xQueueReceive(ctrl_queue, &tmp_controler, 0)){
      data += String(tmp_controler.type) + ":" + String(tmp_controler.set_point, 1);
      client_mosquitto.publish(strcat(pub_topic, MOSQUITTO_TOPIC_PUB), data.c_str(), (bool)true);
      if(String(tmp_controler.type) == "temp"){
        stored_data.temp.set_point = tmp_controler.set_point;
      }
      else if(String(tmp_controler.type) == "humi"){
        stored_data.humi.set_point = tmp_controler.set_point;
      }
      else if(String(tmp_controler.type) == "air"){
        stored_data.air.set_point = tmp_controler.set_point;
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

  while(!client_mosquitto.connected()){
    Serial.println("Attempting MQTT connection...");
    if (client_mosquitto.connect("ESP32Gateway")) {
      Serial.println("Connected to Mosquitto Broker!");

      client_mosquitto.subscribe("sensor/DHT11/temp");
      Serial.println("Subscribed to topic \"sensor/DHT11/temp\"");
      // subscribe vao topic de nghe phan hoi tu thiet bi
      client_mosquitto.subscribe(topic_response.c_str(), 2);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_response);
    } else {
      Serial.println("Connect to mosquitto failed, try again in 5 seconds!");
      vTaskDelay(5000/portTICK_PERIOD_MS);
    }
  }
  while(!client_thingsboard.connected()){
    if (client_thingsboard.connect("ESP32Client")) {
      Serial.println("Connected to Thingsboard Broker!");

      // subscribe vao topic de nhan cap nhat set point tu thingsboard
      client_thingsboard.subscribe(THINGSBOARD_TOPIC_SUB, 1);
      Serial.print("Subscribed to topic: ");
      Serial.println(THINGSBOARD_TOPIC_SUB);
      
    }
    // } else {
    //   Serial.println("Connect to Mosquitto Broker failed, try again in 5 seconds!");
    //   delay(5000);
    // }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client_thingsboard.setServer(mqtt_server, 1883);
  client_mosquitto.setServer(thingsboard_server, 1883);
  client_thingsboard.setCallback(callback);
  client_mosquitto.setCallback(callback);

  ctrl_queue = xQueueCreate(3, sizeof(Controller_t));

  xTaskCreatePinnedToCore(control_task, "hvac task", 1024, NULL, 1, NULL, 1);
}

void loop() {
  if (!client_thingsboard.connected() || !client_mosquitto.connected()) {
    reconnect();
  }
  client_thingsboard.loop();
  client_mosquitto.loop();
}
