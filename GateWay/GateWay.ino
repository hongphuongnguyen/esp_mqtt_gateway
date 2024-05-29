#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <coap-simple.h>

#define mqtt_server "192.168.1.6"
#define ssid        "Phuong Huyen" 
#define pass        "123456789"

#define thingsboard_server  "mqtt.thingsboard.cloud"
#define accessToken         "e6apOql9bEc1Wh4MDpCK"

WiFiUDP udp;
Coap coap(udp);

WiFiClient espClient1;
WiFiClient espClient2;

PubSubClient client_sub(espClient1);
PubSubClient client_pub(espClient2);

// Hàm callback để xử lý yêu cầu CoAP từ client
void callback_coap_ldr(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Request received]");

  // Lấy payload từ gói tin
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  Serial.print("LDR Value: ");
  Serial.println(p);

  String P = "{ldr:" + String(p) + "}";
  client_pub.publish("v1/devices/me/telemetry", P.c_str());
  Serial.println("Published data to Thingsboard: " + P);
  // Gửi phản hồi cho client (nếu cần thiết)
  coap.sendResponse(ip, port, packet.messageid, "OK");
}

// Hàm callback để xử lý dữ liệu gửi về từ Thingsboard bằng MQTT
void callback_pub(char* topic, byte* payload, unsigned int length) {
}

// Hàm callback để xử lý dữ liệu gửi về từ sensor bằng MQTT
void callback_sub(char* topic, byte* payload, unsigned int length) {
  String data;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  data += "\0";
  Serial.println("");
  client_pub.publish("v1/devices/me/telemetry", data.c_str(), false);
  Serial.println("Published data to Thingsboard: " + data);
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

void connectToThingsboard(){
  if(!client_pub.connected()){
    Serial.println("Connecting to Thingsboard...");
    while(!client_pub.connect("ESP32Gateway", accessToken, NULL)) {
      Serial.println("Failed to connect to Thingsboard!");
      Serial.println("Retrying...");
      delay(5000);
    }
    Serial.println("Connected to Thingsboard!");
  }
}

void connectToMosquitto(){
  if(!client_sub.connected()){
    Serial.println("Connecting to Mosquitto Broker...");
    while(!client_sub.connect("ESP32Gateway")) {
      Serial.println("Failed to connect to Thingsboard!");
      Serial.println("Retrying...");
      delay(5000);
    }
    Serial.println("Connected to Mosquitto Broker!");
    client_sub.subscribe("sensor/DHT11");
    Serial.println("Subcribed to topic sensor/DHT11");
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
  client_sub.setServer(mqtt_server, 1885);
  client_sub.setCallback(callback_sub);
  client_pub.setServer(thingsboard_server, 1883);
  client_pub.setCallback(callback_pub);
  coap.server(callback_coap_ldr, "ldr");
  coap.start(7000);
}

void loop() {
  if (!client_sub.connected() || !client_pub.connected()) {
    reconnect();
  }
  client_sub.loop();
  client_pub.loop();
  coap.loop();
  delay(3000);
}