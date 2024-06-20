#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <coap-simple.h>

#define mqtt_server "192.168.137.1"
#define ssid        "hongphuong" 
#define pass        "abcde123"

#define thingsboard_server  "thingsboard.hust-2slab.org"
#define accessToken         "PD3S3LMavuogQB7AV8EY"

#define coap_server   "192.168.137.100"

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
  Environ_element_t lumi;
}__attribute__((packed)) Gateway_element_t;

WiFiUDP udp;
Coap coap(udp);

WiFiUDP light_udp;
Coap light_coap(light_udp);

WiFiClient espClient1;
WiFiClient espClient2;

PubSubClient client_sub(espClient1);
PubSubClient client_pub(espClient2);

IPAddress local_IP(192,168,137,30);
IPAddress gateway(192,168,137, 1);
IPAddress subnet(255,255,255,1);
IPAddress primaryDNS(8,8,8,8);
IPAddress secondaryDNS(4,4,4,4);

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

// Hàm callback để xử lý yêu cầu CoAP từ client cuar Coap server
void callback_coap(CoapPacket &packet, IPAddress ip, int port) {
  // Lấy payload từ gói tin
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.print("Message CoAP arrived: ");
  Serial.println(String(p));
  stored_data.lumi.sensing = String(p).toFloat();
  client_pub.publish(THINGSBOARD_TOPIC_PUB, String(p).c_str());
  Serial.println("Published data to Thingsboard: " + String(p));
  // Gửi phản hồi cho client (nếu cần thiết)
  coap.sendResponse(ip, port, packet.messageid, "OK\0", strlen("OK\0"));
}

// Hàm callback để xử lý dữ liệu gửi về từ Thingsboard bằng MQTT
void callback_pub(char* topic, byte* payload, unsigned int length) {
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

// Hàm callback để xử lý dữ liệu gửi về từ sensor bằng MQTT
void callback_sub(char* topic, byte* payload, unsigned int length) {
  String data;
  Serial.print("Message MQTT arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  data += "\0";
  Serial.println("");

  if (String(topic) == "sensor/DHT11") {

    client_pub.publish(THINGSBOARD_TOPIC_PUB, data.c_str());
    delay(2000);
    Serial.println("Published data to Thingsboard: " + data);
  }
  String topic_response = String(ROOM_ID) + String(MOSQUITTO_TOPIC_RES);

  // callback kich hoat khi co phan hoi tu HVAC ve ket qua sau khi thu thi
  
  if(String(topic) == topic_response){
    if(data.indexOf("HVAC") != -1){
      if(data.indexOf("disconnected") != -1){
        //can publish trang thai mat ket noi cho thingsboard
        Serial.println("HVAC Disconnected");
        client_pub.publish(THINGSBOARD_TOPIC_PUB, "{\"status\": false}");
        stored_data.hvac.status = 0;
      }
      else if(data.indexOf("OK") != -1){
        //cap nhat set point luu o gateway
      }
      else if(data.indexOf("Connected") != -1){
        //cap nhat set point luu o gateway
        Serial.println("HVAC connected");
        client_pub.publish(THINGSBOARD_TOPIC_PUB, "{\"status\": true}");
        stored_data.hvac.status = 1;
      }
      else{
        // luu loi chua duoc xu ly
      }
    }
  }
}

void control_task( void *arg){
  char pub_topic[40] = "";
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
        client_sub.publish(pub_topic, data.c_str(), (bool)false);
      }
      else if(String(tmp_controler.type) == "\"humidity\""){
        Serial.println(data);
        stored_data.humi.set_point = tmp_controler.set_point;
        client_sub.publish(pub_topic, data.c_str(), (bool)false);

      }
      else if(String(tmp_controler.type) == "\"air\""){
        stored_data.air.set_point = tmp_controler.set_point;
        client_sub.publish(pub_topic, data.c_str(), (bool)false);
      }
      else if(String(tmp_controler.type)== "\"light\""){
        Serial.println(data);
        stored_data.lumi.set_point = tmp_controler.set_point - stored_data.lumi.sensing;
        light_coap.put(coap_server, 5683, "light", String(stored_data.lumi.set_point, 0).c_str());
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
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
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

void connectToThingsboard(){
  if(!client_pub.connected()){
    Serial.println("Connecting to Thingsboard...");
    while(!client_pub.connect("ESP32Gateway", accessToken, NULL)) {
      Serial.println("Failed to connect to Thingsboard!");
      Serial.println("Retrying...");
      delay(5000);
    }
    Serial.println("Connected to Thingsboard!");
    client_pub.subscribe(THINGSBOARD_TOPIC_SUB, 1);
  }
}

void connectToMosquitto(){
  
  String topic_response = String(ROOM_ID) + String(MOSQUITTO_TOPIC_RES);

  if(!client_sub.connected()){
    Serial.println("Connecting to Mosquitto Broker...");
    while(!client_sub.connect("ESP32Gateway")) {
      Serial.println("Failed to connect to Thingsboard!");
      Serial.println("Retrying...");
      delay(5000);
    }
    Serial.println("Connected to Mosquitto Broker!");
    if(client_sub.subscribe("sensor/DHT11")){
      Serial.println("Subcribed to topic sensor/DHT11");
    }
    if(client_sub.subscribe(topic_response.c_str(),1)){
      Serial.println("Subcribed to topic: " + topic_response);
    }
  }
}

void reconnect() {
  if(!client_pub.connected()){
    connectToThingsboard();
  }
  if(!client_sub.connected()){
    connectToMosquitto();
  }
}

void setup() {

  Serial.begin(115200);
  setup_wifi();
  client_sub.setServer(mqtt_server, 1884);
  client_sub.setCallback(callback_sub);
  client_pub.setServer(thingsboard_server, 1883);
  client_pub.setCallback(callback_pub);
  coap.server(callback_coap, "coap");
  coap.start(5685);

  ctrl_queue = xQueueCreate(3, sizeof(Controller_t));

  light_coap.response(coap_light_response);
  light_coap.start();

  xTaskCreatePinnedToCore(control_task, "hvac task", 2048, NULL, 1, NULL, 1);
}

void loop() {
  if (!client_sub.connected() || !client_pub.connected()) {
    reconnect();
  }
  client_sub.loop();
  client_pub.loop();
  coap.loop();
  light_coap.loop();
  vTaskDelay(100/portTICK_PERIOD_MS);
}