## Introduction
This program aims to create an IoT gateway that relays and processes data from sensors and actuators within a local network to the Thingsboard cloud on the Internet.
## Requirements
#### Hardware
- Esp32 devkit v1
- USB to MicroUSB cable
#### Software
- Arduino IDE
- Esp32 Arduino libraries
- Docker
## Tutorial
#### Mosquitto Broker Deployment
To deploy broker for local mqtt network communication, run
```bash
    docker compose up -d
```
Broker listen on port 1884
#### Configure Wifi, MQTT, CoAP parameters
| Parameters | Type | Description       |
| :--------- | :---------- | :--------------- |
| `ssid`     | `string`    |  Your Wifi ssid    |
| `pass`     | `string`    | Your Wifi password |
| `mqtt_server`     | `string`    | IP or hostname of mosquitto broker |
| `thingsboard_server`     | `string`    | Ip or hostname of thingsboard |
| `accessToken`     | `string`    | Device AccessToken  |
| `coap_server`     | `string`    | Ip or hostname of coap server (light) for light controlling |

## Note
Note that the gateway is using a static IP (192.168.137.30) to fix the gateway's address within the local network, enabling sensors and actuators to identify the gateway's IP without the need for repeated manual configuration.
## Related
Repo of IoT [Device ](https://github.com/hongphuongnguyen/esp_mqtt_pub) 
