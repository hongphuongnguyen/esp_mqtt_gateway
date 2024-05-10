#include <WiFi.h>
#include <PubSubClient.h>

#define ssid "Phuong Huyen"
#define pass "123456789"

#define mqtt_server "192.168.1.6"

#define thingsboard_server  "mqtt.thingsboard.cloud"
#define accessToken         "e6apOql9bEc1Wh4MDpCK"

WiFiClient espClient;
PubSubClient client_sub(espClient);
PubSubClient client_pub(espClient);

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
  if (client_pub.connect("ESP32Client", accessToken, NULL) && String(topic) == "sensor/DHT11/temp") {
    String payload = "{temperature:";
    payload += data;
    payload += "}";
    client_pub.publish("v1/devices/me/telemetry", payload.c_str());
    delay(2000);
    Serial.println("Published data to Thingsboard: " + payload);
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
  while(!client_pub.connected()){
    Serial.println("Attempting MQTT connection...");
    if (client_pub.connect("ESP32Client", accessToken, NULL)) {
      Serial.println("Connected to Thingsboard!");
      delay(1000);
      // client_pub.subscribe("v1/devices/me/telemetry");
      // Serial.println("Subscribed to topic v1/devices/me/telemetry");
    } else {
      Serial.println("Connect to Thingsboard failed, try again in 5 seconds!");
      delay(5000);
    }
  }
  while(!client_sub.connected()){
    if (client_sub.connect("ESP32Client")) {
      Serial.println("Connected to Mosquitto Broker!");
      client_sub.subscribe("sensor/DHT11/temp");
      Serial.println("Subscribed to topic \"sensor/DHT11/temp\"");
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
  client_sub.setServer(mqtt_server, 1883);
  client_pub.setServer(thingsboard_server, 1883);
  client_sub.setCallback(callback);
  client_pub.setCallback(callback);
}

void loop() {
  if (!client_sub.connected() || !client_pub.connected()) {
    reconnect();
  }
  client_sub.loop();
  client_pub.loop();
}
