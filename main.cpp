/* WiFi Example
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstring>
#define MQTTCLIENT_QOS2 1
#include "mbed.h"
#include "Timer.h"
#include "DHT.h"
#include <cmath>
#include "MQTTmbed.h"
#include "MQTTClientMbedOs.h"
#include "string.h"


WiFiInterface *wifi;
//DHT dht(D2, D3);
//AnalogIn humidity(PA_7);

char* host = "192.168.43.120";
int port = 1883;
const char* global_topic = "global/";
TCPSocket socket;
MQTTClient client(&socket);
int rc;
int counter=0;

void messageArrived(MQTT::MessageData& md){
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    char* m = (char*)message.payload;
    printf("Payload %.*s\r\n", message.payloadlen, m);
    counter++;

}
void messageSend(const char* topic, char* message){
    char* buff =(char*)malloc(strlen(global_topic) + strlen(topic) + 1);
    strcat(buff,global_topic);
    strcat(buff,topic);
    MQTT::Message messg;
    messg.qos = MQTT::QOS0;
    messg.retained = false;
    messg.dup = false;
    messg.payload = (void*)message;
    messg.payloadlen = strlen(message)+1;
    rc = client.publish(buff,messg);
}

void initializeConnection(NetworkInterface* net){
    
    SocketAddress a;
    net->gethostbyname(host, &a);
    a.set_port(port);
    socket.open(net);
    rc = socket.connect(a);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);
    printf("Connected socket\n\r");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "1";
    data.username.cstring = "Nucleo_client";
    client.connect(data);
    messageSend("plant1", "Hi!");
}

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}




int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    // for (int i = 0; i < count; i++) {
    //     printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
    //            sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
    //            ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    // }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

// void TemperatureHandler()
// {
//     float temp = dht.ReadTemperature(CELCIUS);
//     if(isnan(temp)){
//         printf("Failed to read temperature from DHT!");
//     }
//     else{
//         printf("%f",temp);
//     }
//     //TODO SENT TO MQTT CLIENT

// }
// void HumidityHandler()
// {
//     float hum = humidity.read();
//     if(isnan(hum)){
//          printf("Failed to read humidity !");
//     }
//      else{
//         printf("%f",hum);
//     }
//     //TODO SENT TO MQTT CLIENT
// }

int main()
{

    printf("WiFi example\n");
    


#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    int count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");

    

    printf("MAC: %s\n", wifi->get_mac_address());
    SocketAddress a;
    wifi->get_ip_address(&a);
    printf("IP: %s\n", a.get_ip_address());
    wifi->get_netmask(&a);
    printf("Netmask: %s\n", a.get_ip_address());
    wifi->get_gateway(&a);
    printf("Gateway: %s\n", a.get_ip_address());
    printf("RSSI: %d\n\n", wifi->get_rssi());
    initializeConnection(wifi);
    client.subscribe("global/plant2",MQTT::QOS0,messageArrived);
    while(counter<5){
        client.yield(100);
    }
    //wifi->disconnect();

    printf("\nDone\n");
}
