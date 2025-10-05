#ifndef WIFI_INFO_H_
#define WIFI_INFO_H_

#include <ESP8266WiFi.h>

#define STATIC_IP                       // uncomment for static IP, set IP below
#ifdef STATIC_IP
  IPAddress ip(192,168,0,121);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
#endif

const char *ssid = "TP-Link_5480";
const char *password = "92621442";

  
void wifi_connect() {
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
  #ifdef STATIC_IP  
    WiFi.config(ip, gateway, subnet);
  #endif
	WiFi.setAutoReconnect(true);
	WiFi.begin(ssid, password);
	while (!WiFi.isConnected()) {
		delay(100);
	}
}

void check_wifi_connection() {
	static unsigned long last_check = 0;
	const unsigned long check_interval = 30000; // 30 seconds
	
	if (millis() - last_check >= check_interval) {
	  if (WiFi.status() != WL_CONNECTED) {
			WiFi.disconnect();
			delay(100);
			wifi_connect();
		
			// If still not connected after 5 seconds, restart
			unsigned long start = millis();
			while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
		  	delay(100);
			}
		
			if (WiFi.status() != WL_CONNECTED) {
		 	 	delay(1000);
		  	ESP.restart();
			}
		}
		last_check = millis();
	}
}

#endif /* WIFI_INFO_H_ */