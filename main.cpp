
#include <cstdio>
#include <cstdlib>
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

AnalogIn light(A4);
AnalogIn humidity(A5);
AnalogIn temperature(A1);

char* host = "127.0.0.1";
int port = 8883;
const char* global_topic = "global/";
TCPSocket socket;
MQTTClient client(&socket);
int rc;
int pot_id;


void messageArrived(MQTT::MessageData& md){
    MQTT::Message &message = md.message;
    char* m = (char*)message.payload;
    char* topic = md.topicName.cstring;
    if(!strcmp(topic,"global/1/pump")){
        if(*m=='0'){
            //выключаем выход
        } else if(*m=='1'){
            //включаем выход
        }
    }

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
    char nameBuf[30];
    snprintf(nameBuf, sizeof(nameBuf), "POR_CLIENT_%d",pot_id);
    data.username.cstring = "POT_CLIENT";
    client.connect(data);
    printf("Connected to MQTT\n");
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

    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

 void TemperatureHandler()
 {
    float temp = temperature.read();
    if(isnan(temp)){
        printf("Failed to read temperature from DHT!\n");
    }
    else{
        printf("Temperature: %f\n",temp);
    }
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", temp);
    messageSend("1/temperature", buffer);
    //TODO SENT TO MQTT CLIENT

}
void LightHandler()
{
    float lgt = light.read();
    if(isnan(lgt)){
        printf("Failed to read light !\n");
    }
     else{
        printf("Light: %f\n",lgt);
    }
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", lgt);
    messageSend("1/light", buffer);
}

void HumidityHandler(){
    float hdt = humidity.read();
    if(isnan(hdt)){
        printf("Failed to read humidity!\n");
    } else {
        printf("Humidity: %f\n",hdt);
    }
}

void Read(){
    LightHandler();
    HumidityHandler();
    TemperatureHandler();
}

int main()
{
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
    while(1){
        Read();
        client.yield(500);
    }
    //wifi->disconnect();

    printf("\nDone\n");
}
