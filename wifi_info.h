#ifndef WIFI_INFO_H_
#define WIFI_INFO_H_


#include <ESP8266WiFi.h>

#define STATIC_IP                       // uncomment for static IP, set IP below
#ifdef STATIC_IP
  IPAddress ip(192,168,0,120);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
#endif


const char *ssid = "TP-Link_5480";
const char *password = "#########";

void wifi_connect() {
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
  #ifdef STATIC_IP  
    WiFi.config(ip, gateway, subnet);
  #endif
	WiFi.setAutoReconnect(true);
	WiFi.begin(ssid, password);
	Serial.println("WiFi connecting...");
	while (!WiFi.isConnected()) {
		delay(100);
		Serial.print(".");
	}
	Serial.print("\n");
	Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
}

#endif /* WIFI_INFO_H_ */