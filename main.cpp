
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define MQTTCLIENT_QOS2 1
#include "mbed.h"
#include "Timer.h"
#include <cmath>
#include "MQTTmbed.h"
#include "MQTTClientMbedOs.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "DS1820.h"

WiFiInterface *wifi;
AnalogIn light(A1);
AnalogIn humidity(A0);
DigitalOut pump(D3);
DS1820 temp(A3);

Thread humidityThread;
Thread lightThread;
Thread tempThread;

bool manual = false;
char* host = "192.168.0.75";
int port = 1883;
const char* global_topic = "%/";

TCPSocket socket;
MQTTClient client(&socket);
int rc;
int pot_id;


struct desired{
    double humidity;
    double light;    
};

struct desired desire;


void messageSend(const char* topic, char* message)
{
    char* buff =(char*)malloc(strlen(global_topic) + strlen(topic));
    strcat(buff,global_topic);
    strcat(buff,topic);
    MQTT::Message messg;
    messg.qos = MQTT::QOS0;
    messg.retained = false;
    messg.dup = false;
    messg.payload = (void*)message;
    messg.payloadlen = strlen(message);
    rc = client.publish(buff,messg);
}

void initializeMQTTConnection(NetworkInterface* net)
{
    
    SocketAddress a;
    net->gethostbyname(host, &a);
    a.set_port(port);
    socket.open(net);
    rc = socket.connect(a);
    printf("rc from TCP connect is %d\n", rc);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.username.cstring = "Nucleo_client_2";
    data.clientID.cstring = "nucleo_1";
    char nameBuf[30];
    snprintf(nameBuf, sizeof(nameBuf), "POT_CLIENT_%d",pot_id);
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

void TemperatureHandler()
{
    if(temp.begin()){
        while(true){
            temp.startConversion();
            float tmp = temp.read();
            if(isnan(tmp)){
                printf("Failed to read temperature !\n");
            }
            else{
                printf("Temperature: %f\n",tmp);
            }
        }
    }
}

void LightHandler()
{
    while(true){
        float lgt = light.read();
        if(isnan(lgt)){
            printf("Failed to read light !\n");
        }
        else{
            printf("Light: %f\n",lgt);
        }
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%f", lgt);
        messageSend("1/light", buffer);
        wait_us(2000000);
    }
}

void HumidityHandler()
{
    while(true){
        float hmd = humidity.read();
        if(!manual){
            if(hmd<desire.humidity-0.1){
                pump = 1;
                while(hmd<desire.humidity){
                    if(manual) break;
                    hmd = humidity.read();
                    wait_us(100000);
                }
                pump = 0;
            }
        }
        if(isnan(hmd)){
            printf("Failed to read humidity!");
        }
         else{
            printf("Humidity: %f\n",hmd);
        }
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%f", hmd);
        messageSend("1/humidity", buffer);
        wait_us(2000000);
    }
}

void messageArrived(MQTT::MessageData& md){
    MQTT::Message &message = md.message;
    char topic[md.topicName.lenstring.len+1]; 
    char msg[message.payloadlen+1];
    snprintf(msg, sizeof(msg), "%.*s\r\n",message.payloadlen, (char*)message.payload);
    snprintf(topic, sizeof(topic), "%.*s\r\n", md.topicName.lenstring.len, (char*)md.topicName.lenstring.data);
    if (strcmp(topic,"%/update")==0){
        LightHandler();
        HumidityHandler();
    }
    if(strcmp(topic,"%/1/desire/humidity")==0){
        desire.humidity = atof(msg);
    }
    if(strcmp(topic,"%/1/desire/light")==0){
        desire.light = atof(msg);
        
    }
    if(strcmp((char*)topic,"%/1/control_pump")==0){
        if(manual){
            pump=atoi(msg);
            printf("Pump should be %d\n",atoi(msg));
            printf("%d\n",atoi((char*)msg));
        }
    }
    if(strcmp((char*)topic,"%/1/manual")==0){
        manual=strcmp((char*)msg,"1")==0;
        printf("Manual: %d\n",manual);
    }
}

int find_network(WiFiInterface *wifi)
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


int main()
{
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\n");
            return -1;
    }
    desire.humidity = 0.25;
    int count = find_network(wifi);
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

    initializeMQTTConnection(wifi);
    client.subscribe("%/#", MQTT::QOS0, messageArrived);
    

    humidityThread.start(HumidityHandler);
    lightThread.start(LightHandler);
    tempThread.start(TemperatureHandler);
    while(true){
        client.yield(100);
    }
    wifi->disconnect();

    printf("\nDone\n");
}
