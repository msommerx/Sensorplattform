#include <Arduino.h>

/*****************************************************************
/* OK LAB Particulate Matter Sensor                              *
/*      - nodemcu-LoLin board                                    *
/*      - Nova SDS0111                                           *
/*  ﻿http://inovafitness.com/en/Laser-PM2-5-Sensor-SDS011-35.html *
/*                                                               *
/* Wiring Instruction:                                           *
/*      - SDS011 Pin 1  (TX)   -> Pin D1 / GPIO5                 *
/*      - SDS011 Pin 2  (RX)   -> Pin D2 / GPIO4                 *
/*      - SDS011 Pin 3  (GND)  -> GND                            *
/*      - SDS011 Pin 4  (2.5m) -> unused                         *
/*      - SDS011 Pin 5  (5V)   -> VU                             *
/*      - SDS011 Pin 6  (1m)   -> unused                         *
/*                                                               *
/*****************************************************************
/*                                                               *
/* Alternative                                                   *
/*      - nodemcu-LoLin board                                    *
/*      - Shinyei PPD42NS                                        *
/*      http://www.sca-shinyei.com/pdf/PPD42NS.pdf               *
/*                                                               *
/* Wiring Instruction:                                           *
/*      Pin 2 of dust sensor PM2.5 -> Digital 6 (PWM)            *
/*      Pin 3 of dust sensor       -> +5V                        *
/*      Pin 4 of dust sensor PM1   -> Digital 3 (PMW)            * 
/*                                                               *
/*      - PPD42NS Pin 1 (grey or green)  => GND                  *
/*      - PPD42NS Pin 2 (green or white)) => Pin D5 /GPIO14      *
/*        counts particles PM25                                  *
/*      - PPD42NS Pin 3 (black or yellow) => Vin                 *
/*      - PPD42NS Pin 4 (white or black) => Pin D6 / GPIO12      *
/*        counts particles PM10                                  *
/*      - PPD42NS Pin 5 (red)   => unused                        *
/*                                                               *
/*****************************************************************
/* Extension: DHT22 (AM2303)                                     *
/*  ﻿http://www.aosong.com/en/products/details.asp?id=117         *
/*                                                               *
/* DHT22 Wiring Instruction                                      *
/* (left to right, front is perforated side):                    *
/*      - DHT22 Pin 1 (VDD)     -> Pin 3V3 (3.3V)                *
/*      - DHT22 Pin 2 (DATA)    -> Pin D7 (GPIO13)               *
/*      - DHT22 Pin 3 (NULL)    -> unused                        *
/*      - DHT22 Pin 4 (GND)     -> Pin GND                       *
/*                                                               *
/*****************************************************************
/* Extensions connected via I2C:                                 *
/* BMP180, BME280, OLED Display with SSD1309                     *
/*                                                               *
/* Wiring Instruction                                            *
/* (see labels on display or sensor board)                       *
/*      VCC       ->     Pin 3V3                                 *
/*      GND       ->     Pin GND                                 *
/*      SCL       ->     Pin D4 (GPIO2)                          *
/*      SDA       ->     Pin D3 (GPIO0)                          *
/*                                                               *
/*****************************************************************/
// increment on change
#define SOFTWARE_VERSION "HSRM_170402"

/*****************************************************************
/* Includes                                                      *
/*****************************************************************/
#if defined(ESP8266)
#include <FS.h>                     // must be first
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <base64.h>
#endif
#include <ArduinoJson.h>
#include <Wire.h>
#include <DHT.h>
#include <Ticker.h>

#include "ext_def.h"
#include "html-content.h"

/*****************************************************************
/* Variables with defaults                                       *
/*****************************************************************/
char wlanssid[65] = "Freifunk-disabled";
char wlanpwd[65] = "";

char www_username[65] = "admin";
char www_password[65] = "feinstaub";
bool www_basicauth_enabled = 0;

char version_from_local_config[30] = "";

bool dht_read = 1;
bool send2csv = 0;

long int sample_count = 0;

char host_custom[100] = "192.168.234.1";
char url_custom[100] = "/data.php";
int httpPort_custom = 80;
char user_custom[100] = "";
char pwd_custom[100] = "";
String basic_auth_custom = "";

ESP8266WebServer server(80);
int TimeZone=1;

/*****************************************************************
/* DHT declaration                                               *
/*****************************************************************/
DHT dht(DHT_PIN, DHT_TYPE);

/*****************************************************************
/* Variable Definitions for PPD24NS                              *
/* P1 for PM10 & P2 for PM25                                     *
/*****************************************************************/

unsigned long durationP1;
unsigned long durationP2;

boolean trigP1 = false;
boolean trigP2 = false;
unsigned long trigOnP1;
unsigned long trigOnP2;

unsigned long lowpulseoccupancyP1 = 0;
unsigned long lowpulseoccupancyP2 = 0;

bool send_now = false;
unsigned long starttime;
unsigned long act_micro;
unsigned long act_milli;
unsigned long last_micro = 0;
unsigned long min_micro = 1000000000;
unsigned long max_micro = 0;
unsigned long diff_micro = 0;

const unsigned long sampletime_ms = 30000;


const unsigned long sending_intervall_ms = 55000;
unsigned long sending_time = 0;
bool send_failed = false;

//unsigned long last_update_attempt;
//const unsigned long pause_between_update_attempts = 86400000;
//bool will_check_for_update = false;

String last_result_DHT = "";
String last_value_DHT_T = "";
String last_value_DHT_H = "";
String last_data_string = "";

String esp_chipid;

String server_name;

long last_page_load = millis();

bool config_needs_write = false;

bool first_csv_line = 1;

String data_first_part = "{\"software_version\": \"" + String(SOFTWARE_VERSION) + "\"FEATHERCHIPID, \"sensordatavalues\":[";

static unsigned long last_loop;


/*****************************************************************
/* IPAddress to String                                           *
/*****************************************************************/
String IPAddress2String(const IPAddress& ipaddress) {
	char myIpString[24];
	sprintf(myIpString, "%d.%d.%d.%d", ipaddress[0], ipaddress[1], ipaddress[2], ipaddress[3]);
	return String(myIpString);
}

/*****************************************************************
/* convert float to string with a                                *
/* precision of two decimal places                               *
/*****************************************************************/
String Float2String(const float value) {
	// Convert a float to String with two decimals.
	char temp[15];
	String s;

	dtostrf(value,13, 2, temp);
	s = String(temp);
	s.trim();
	return s;
}

/*****************************************************************
/* convert value to json string                                  *
/*****************************************************************/
String Value2Json(const String& type, const String& value) {
	String s = F("{\"value_type\":\"{t}\",\"value\":\"{v}\"},");
	s.replace("{t}",type);s.replace("{v}",value);
	return s;
}

/*****************************************************************
/* copy config from ext_def                                      *
/*****************************************************************/
void copyExtDef() {

#define strcpyDef(var, def) if (def != NULL) { strcpy(var, def); }
#define setDef(var, def)    if (def != var) { var = def; }

	strcpyDef(wlanssid, WLANSSID);
	strcpyDef(wlanpwd, WLANPWD);
	strcpyDef(www_username, WWW_USERNAME);
	strcpyDef(www_password, WWW_PASSWORD);
	setDef(www_basicauth_enabled, WWW_BASICAUTH_ENABLED);
	setDef(dht_read, DHT_READ);
	setDef(send2csv, SEND2CSV);
	
#undef strcpyDef
#undef setDef
}

/*****************************************************************
/* read config from spiffs                                       *
/*****************************************************************/
void readConfig() {
#if defined(ESP8266)
	String json_string = "";

	if (SPIFFS.begin()) {
		if (SPIFFS.exists("/config.json")) {
			//file exists, reading and loading
			File configFile = SPIFFS.open("/config.json", "r");
			if (configFile) {
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.
				std::unique_ptr<char[]> buf(new char[size]);

				configFile.readBytes(buf.get(), size);
				StaticJsonBuffer<2000> jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(buf.get());
				json.printTo(json_string);
				if (json.success()) {
					if (json.containsKey("SOFTWARE_VERSION")) strcpy(version_from_local_config, json["SOFTWARE_VERSION"]);

#define setFromJSON(key)    if (json.containsKey(#key)) key = json[#key];
#define strcpyFromJSON(key) if (json.containsKey(#key)) strcpy(key, json[#key]);
					strcpyFromJSON(wlanssid);
					strcpyFromJSON(wlanpwd);
					strcpyFromJSON(www_username);
					strcpyFromJSON(www_password);
					setFromJSON(www_basicauth_enabled);
					setFromJSON(dht_read);
					setFromJSON(send2csv);
#undef setFromJSON
#undef strcpyFromJSON
				} 
			}
		} 
	}
#endif
}

/*****************************************************************
/* write config to spiffs                                        *
/*****************************************************************/
void writeConfig() {
#if defined(ESP8266)
	String json_string = "";
	StaticJsonBuffer<2000> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

#define copyToJSON(varname) json[#varname] = varname;
	copyToJSON(SOFTWARE_VERSION);
	copyToJSON(wlanssid);
	copyToJSON(wlanpwd);
	copyToJSON(www_username);
	copyToJSON(www_password);
	copyToJSON(www_basicauth_enabled);
	copyToJSON(dht_read);
	copyToJSON(send2csv);
#undef copyToJSON

	File configFile = SPIFFS.open("/config.json", "w");

	json.printTo(json_string);
	json.printTo(configFile);
	configFile.close();
	config_needs_write = false;
	//end save
#endif
}

/*****************************************************************
/* Base64 encode user:password                                   *
/*****************************************************************/
void create_basic_auth_strings() {
	basic_auth_custom = base64::encode(String(user_custom)+":"+String(pwd_custom));
}

/*****************************************************************
/* html helper functions                                         *
/*****************************************************************/
String form_input(const String& name, const String& info, const String& value, const int length) {
	String s = F("<tr><td>{i} </td><td style='width:90%;'><input type='text' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/></td></tr>");
	s.replace("{i}",info);s.replace("{n}",name);s.replace("{v}",value);s.replace("{l}",String(length));
	return s;
}

String form_password(const String& name, const String& info, const String& value, const int length) {
	String s = F("<tr><td>{i} </td><td style='width:90%;'><input type='password' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/></td></tr>");
	s.replace("{i}",info);s.replace("{n}",name);s.replace("{v}",value);s.replace("{l}",String(length));
	return s;
}

String form_checkbox(const String& name, const String& info, const bool checked) {
	String s = F("<label for='{n}'><input type='checkbox' name='{n}' value='1' id='{n}' {c}/><input type='hidden' name='{n}' value='0' /> {i}</label><br/>");
	if (checked) {s.replace("{c}",F(" checked='checked'"));} else {s.replace("{c}","");};
	s.replace("{i}",info);s.replace("{n}",name);
	return s;
}

String table_row_from_value(const String& name, const String& value) {
	String s = F("<tr><td>{n}</td><td style='width:80%;'>{v}</td></tr>");
	s.replace("{n}",name);s.replace("{v}",value);
	return s;
}

String wlan_ssid_to_table_row(const String& ssid, const String& encryption, const long rssi) {
	long rssi_temp = rssi;
	if (rssi_temp > -50) {rssi_temp = -50; }
	if (rssi_temp < -100) {rssi_temp = -100; }
	int quality = (rssi_temp+100)*2;
	String s = F("<tr><td><a href='#wlanpwd' onclick='setSSID(this)' style='background:none;color:blue;padding:5px;display:inline;'>{n}</a>&nbsp;{e}</a></td><td style='width:80%;vertical-align:middle;'>{v}%</td></tr>");
	s.replace("{n}",ssid);s.replace("{e}",encryption);s.replace("{v}",String(quality));
	return s;
}

/*****************************************************************
/* Webserver request auth: prompt for BasicAuth                              
 *  
 * -Provide BasicAuth for all page contexts except /values and images
/*****************************************************************/
void webserver_request_auth() {
	if (www_basicauth_enabled) {
		if (!server.authenticate(www_username, www_password))
			return server.requestAuthentication();  
	}
}

/*****************************************************************
/* Webserver root: show all options                              *
/*****************************************************************/
void webserver_root() {
	if (WiFi.status() != WL_CONNECTED) {
		server.sendHeader(F("Location"),F("http://192.168.4.1/config"));
		server.send(302,FPSTR(TXT_CONTENT_TYPE_TEXT_HTML),"");
	} else {
		webserver_request_auth();

		String page_content = "";
		last_page_load = millis();
		page_content = FPSTR(WEB_PAGE_HEADER);
		page_content.replace("{t}",F("Übersicht"));
		page_content.replace("{id}",esp_chipid);
		page_content.replace("{mac}",WiFi.macAddress());
		page_content.replace("{fw}",SOFTWARE_VERSION);
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content += FPSTR(WEB_PAGE_FOOTER);
		server.send(200,FPSTR(TXT_CONTENT_TYPE_TEXT_HTML),page_content);
	}
}

/*****************************************************************
/* Webserver root: show all options                              *
/*****************************************************************/
void webserver_config() {
	webserver_request_auth();
	
	String page_content = "";
	last_page_load = millis();

	page_content += FPSTR(WEB_PAGE_HEADER);
	page_content.replace("{t}",F("Konfiguration"));
	page_content.replace("{id}",esp_chipid);
	page_content.replace("{mac}",WiFi.macAddress());
	page_content.replace("{fw}",SOFTWARE_VERSION);
	page_content += FPSTR(WEB_CONFIG_SCRIPT);
	if (server.method() == HTTP_GET) {
		page_content += F("<form method='POST' action='/config' style='width: 100%;'>");
		page_content += F("<b>WLAN Daten</b><br/>");
		if (WiFi.status() != WL_CONNECTED) {  // scan for wlan ssids
			WiFi.disconnect();
			int n = WiFi.scanNetworks();
			if (n == 0) {
				page_content += F("<br/>Keine Netzwerke gefunden<br/>");
			} else {
				page_content += F("<br/>Netzwerke gefunden: ");
				page_content += String(n);
				page_content += F("<br/>");
				int indices[n];
				for (int i = 0; i < n; i++) { indices[i] = i; }
				for (int i = 0; i < n; i++) {
					for (int j = i + 1; j < n; j++) {
						if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
							std::swap(indices[i], indices[j]);
						}
					}
				}
				String cssid;
				for (int i = 0; i < n; i++) {
					if (indices[i] == -1) continue;
						cssid = WiFi.SSID(indices[i]);
						for (int j = i + 1; j < n; j++) {
							if (cssid == WiFi.SSID(indices[j])) {
							indices[j] = -1; // set dup aps to index -1
						}
					}
				}
				page_content += F("<br/><table>");
				for (int i = 0; i < n; ++i) {
					if (indices[i] == -1) continue;
					// Print SSID and RSSI for each network found
					page_content += wlan_ssid_to_table_row(WiFi.SSID(indices[i]),((WiFi.encryptionType(indices[i]) == ENC_TYPE_NONE)?" ":"*"),WiFi.RSSI(indices[i]));
				}
				page_content += F("</table><br/><br/>");
			}
		}
		page_content += F("<table>");
		page_content += form_input(F("wlanssid"),F("WLAN"),wlanssid,64);
		page_content += form_password(F("wlanpwd"),F("Passwort"),wlanpwd,64);
		page_content += F("</table><br/><input type='submit' name='submit' value='Speichern'/><br/><br/><b>Ab hier nur ändern, wenn Sie wirklich wissen, was Sie tun!!!</b><br/><br/><b>BasicAuth</b><br/>");
		page_content += F("<table>");
		page_content += form_input(F("www_username"),F("User"),www_username,64);
		page_content += form_password(F("www_password"),F("Passwort"),www_password,64);
		page_content += form_checkbox(F("www_basicauth_enabled"),F("BasicAuth aktivieren"),www_basicauth_enabled);
		page_content += F("</table><br/><input type='submit' name='submit' value='Speichern'/><br/><br/><b>APIs</b><br/>");
		page_content += F("<br/><b>Sensoren</b><br/>");
		page_content += form_checkbox(F("dht_read"),F("DHT22 (Temp.,Luftfeuchte)"),dht_read);
		page_content += F("<br/><b>Weitere Einstellungen</b><br/>");
		page_content += F("<table>");
		page_content += F("</table><br/><b>Weitere APIs</b><br/><br/>");
		page_content += F("<table>");
		page_content += F("</table><br/>");
		page_content += F("<table>");
		page_content += F("</table><br/>");
		page_content += F("<br/><input type='submit' name='submit' value='Speichern'/></form>");
	} else {

#define readCharParam(param) if (server.hasArg(#param)){ server.arg(#param).toCharArray(param, sizeof(param)); }
#define readBoolParam(param) if (server.hasArg(#param)){ param = server.arg(#param) == "1"; }
#define readIntParam(param)  if (server.hasArg(#param)){ int val = server.arg(#param).toInt(); if (val != 0){ param = val; }}

		if (server.hasArg("wlanssid") && server.arg("wlanssid") != "") {
			readCharParam(wlanssid);
			readCharParam(wlanpwd);
		}
		readCharParam(www_username);
		readCharParam(www_password);
		readBoolParam(www_basicauth_enabled);
		readBoolParam(dht_read);
//		if (server.hasArg("debug")) { debug = server.arg("debug").toInt(); } else { debug = 0; }


#undef readCharParam
#undef readBoolParam
#undef readIntParam

		config_needs_write = true;

		page_content += F("<br/>Lese DHT "); page_content += String(dht_read);
		page_content += F("<br/><br/><a href='/reset?confirm=yes'>Gerät neu starten?</a>");

	}
	page_content += FPSTR(WEB_PAGE_FOOTER);
	server.send(200,FPSTR(TXT_CONTENT_TYPE_TEXT_HTML),page_content);
}

/*****************************************************************
/* Webserver root: show latest values                            *
/*****************************************************************/
void webserver_values() {
#if defined(ESP8266)
	if (WiFi.status() != WL_CONNECTED) {
		server.sendHeader(F("Location"),F("http://192.168.4.1/config"));
		server.send(302,FPSTR(TXT_CONTENT_TYPE_TEXT_HTML),"");
	} else {
		String page_content = "";
		last_page_load = millis();
		long signal_strength = WiFi.RSSI();
		long signal_temp = signal_strength;
		if (signal_temp > -50) {signal_temp = -50; }
		if (signal_temp < -100) {signal_temp = -100; }
		int signal_quality = (signal_temp+100)*2;
		int value_count = 0;
		page_content += FPSTR(WEB_PAGE_HEADER);
		page_content.replace("{t}",F("Aktuelle Werte"));
		page_content.replace("{id}",esp_chipid);
		page_content.replace("{mac}",WiFi.macAddress());
		page_content.replace("{fw}",SOFTWARE_VERSION);
		page_content += F("<table>");
		if (dht_read) {
			page_content += table_row_from_value(F("DHT22&nbsp;Temperatur:"),last_value_DHT_T+"&nbsp;°C");
			page_content += table_row_from_value(F("DHT22&nbsp;rel.&nbsp;Luftfeuchte:"),last_value_DHT_H+"&nbsp;%");
		}
		page_content += table_row_from_value(" "," ");
		page_content += table_row_from_value(F("WiFi&nbsp;Signal"),String(signal_strength)+"&nbsp;dBm");
		page_content += table_row_from_value(F("Signal&nbsp;Qualität"),String(signal_quality)+"%");
		page_content += F("</table>");
		page_content += FPSTR(WEB_PAGE_FOOTER);
		server.send(200,FPSTR(TXT_CONTENT_TYPE_TEXT_HTML),page_content);
	}
#endif
}

/*****************************************************************
/* Webserver set debug level                                     *
/*****************************************************************/
void webserver_debug_level() {
	webserver_request_auth();
  
	String page_content = "";
	last_page_load = millis();
	page_content += FPSTR(WEB_PAGE_HEADER);
	page_content.replace("{t}","Debug level");
	page_content.replace("{id}",esp_chipid);
	page_content.replace("{mac}",WiFi.macAddress());
	page_content.replace("{fw}",SOFTWARE_VERSION);

	page_content += FPSTR(WEB_PAGE_FOOTER);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
/* Webserver remove config                                       *
/*****************************************************************/
void webserver_removeConfig() {
	webserver_request_auth();
 
	String page_content = "";
	last_page_load = millis();
	page_content += FPSTR(WEB_PAGE_HEADER);
	page_content.replace("{t}","Config.json löschen");
	page_content.replace("{id}",esp_chipid);
	page_content.replace("{mac}",WiFi.macAddress());
	page_content.replace("{fw}",SOFTWARE_VERSION);
	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);
	} else {
#if defined(ESP8266)
		if (SPIFFS.exists("/config.json")) {	//file exists
			if (SPIFFS.remove("/config.json")) {
				page_content += F("<h3>Config.json gelöscht.</h3>");
			} else {
				page_content += F("<h3>Config.json konnte nicht gelöscht werden.</h3>");
			}
		} else {
			page_content += F("<h3>Config.json nicht gefunden.</h3>");
		}
#endif
	}
	page_content += FPSTR(WEB_PAGE_FOOTER);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
/* Webserver reset NodeMCU                                       *
/*****************************************************************/
void webserver_reset() {
	webserver_request_auth();
 
	String page_content = "";
	last_page_load = millis();
	page_content += FPSTR(WEB_PAGE_HEADER);
	page_content.replace("{t}","Sensor neu starten");
	page_content.replace("{id}",esp_chipid);
	page_content.replace("{mac}",WiFi.macAddress());
	page_content.replace("{fw}",SOFTWARE_VERSION);
	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_RESET_CONTENT);
	} else {
#if defined(ESP8266)
		ESP.restart();
#endif
	}
	page_content += FPSTR(WEB_PAGE_FOOTER);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
/* Webserver data.json                                           *
/*****************************************************************/
void webserver_data_json() {
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON),last_data_string);
}

/*****************************************************************
/* Webserver Logo Luftdaten.info                                 *
/*****************************************************************/
void webserver_luftdaten_logo() {
  server.send(200, FPSTR(TXT_CONTENT_TYPE_IMAGE_SVG),FPSTR(LUFTDATEN_INFO_LOGO_SVG));
}

/*****************************************************************
/* Webserver Logo Code For Germany                               *
/*****************************************************************/
void webserver_cfg_logo() {
	server.send(200, FPSTR(TXT_CONTENT_TYPE_IMAGE_SVG),FPSTR(CFG_LOGO_SVG));
}

/*****************************************************************
/* Webserver page not found                                      *
/*****************************************************************/
void webserver_not_found() {
	last_page_load = millis();
	 server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
}

/*****************************************************************
/* Webserver setup                                               *
/*****************************************************************/
void setup_webserver() {
	server_name  = F("CO2-");
	server_name += esp_chipid;

	server.on("/", webserver_root);
	server.on("/config", webserver_config);
	server.on("/values", webserver_values);
	server.on("/generate_204", webserver_config);
	server.on("/fwlink", webserver_config);
	server.on("/debug", webserver_debug_level);
	server.on("/removeConfig", webserver_removeConfig);
	server.on("/reset", webserver_reset);
	server.on("/data.json", webserver_data_json);
	server.on("/luftdaten_logo.svg", webserver_luftdaten_logo);
	server.on("/cfg_logo.svg", webserver_cfg_logo);
	server.onNotFound(webserver_not_found);

	server.begin();
}

/*****************************************************************
/* WifiConfig                                                    *
/*****************************************************************/
void wifiConfig() {
#if defined(ESP8266)
	const char *softAP_password = "";
	const byte DNS_PORT = 53;
	int retry_count = 0;
	DNSServer dnsServer;
	IPAddress apIP(192, 168, 4, 1);
	IPAddress netMsk(255, 255, 255, 0);

	WiFi.softAPConfig(apIP, apIP, netMsk);
	WiFi.softAP(server_name.c_str(), "");

	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, "*", apIP);

	// 10 minutes timeout for wifi config
	last_page_load = millis();
	while (((millis() - last_page_load) < 600000) && (! config_needs_write)) {
		dnsServer.processNextRequest();
		server.handleClient();
		wdt_reset(); // nodemcu is alive
		yield();
	}

	// half second to answer last requests
	last_page_load = millis();
	while (((millis() - last_page_load) < 500)) {
		dnsServer.processNextRequest();
		server.handleClient();
		yield();
	}

	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	delay(100);

	WiFi.begin(wlanssid, wlanpwd);
	
	while ((WiFi.status() != WL_CONNECTED) && (retry_count < 20)) {
		delay(500);
		retry_count++;
	}
#endif
}

/*****************************************************************
/* WiFi auto connecting script                                   *
/*****************************************************************/
void connectWifi() {
#if defined(ESP8266)
	int retry_count = 0;
	WiFi.mode(WIFI_STA);
	WiFi.begin(wlanssid, wlanpwd); // Start WiFI

	while ((WiFi.status() != WL_CONNECTED) && (retry_count < 20)) {
		delay(500);
		retry_count++;
	}
	if (WiFi.status() != WL_CONNECTED) {
		wifiConfig();
		if (WiFi.status() != WL_CONNECTED) {
			retry_count = 0;
			while ((WiFi.status() != WL_CONNECTED) && (retry_count < 20)) {
				delay(500);
				retry_count++;
			}
		}
	}
	WiFi.softAPdisconnect(true);
#endif
}


/*****************************************************************
/* send data as csv to serial out                                *
/*****************************************************************/
void send_csv(const String& data) {
	char* s;
	String tmp_str;
	String headline;
	String valueline;
	int value_count = 0;
	StaticJsonBuffer<1000> jsonBuffer;
	JsonObject& json2data = jsonBuffer.parseObject(data);
	if (json2data.success()) {
		headline = F("Timestamp_ms;");
		valueline = String(act_milli)+";";
		for (int i=0;i<json2data["sensordatavalues"].size();i++) {
			tmp_str = jsonBuffer.strdup(json2data["sensordatavalues"][i]["value_type"].as<char*>());
			headline += tmp_str+";";
			tmp_str = jsonBuffer.strdup(json2data["sensordatavalues"][i]["value"].as<char*>());
			valueline += tmp_str+";";
		}
		if (first_csv_line) {
			if (headline.length() > 0) headline.remove(headline.length()-1);
			Serial.println(headline);
			first_csv_line = 0;
		}
		if (valueline.length() > 0) valueline.remove(valueline.length()-1);
		Serial.println(valueline);
	} else {
	}
}

/*****************************************************************
/* read DHT22 sensor values                                      *
/*****************************************************************/
String sensorDHT() {
	String s = "";
	int i = 0;
	float h;
	float t;


	// Check if valid number if non NaN (not a number) will be send.

	while ((i++ < 5) && (s == "")) {
//		dht.begin();
		h = dht.readHumidity(); //Read Humidity
		t = dht.readTemperature(); //Read Temperature
		if (isnan(t) || isnan(h)) {
			delay(100);
			h = dht.readHumidity(true); //Read Humidity
			t = dht.readTemperature(false,true); //Read Temperature
		}
	}
	return s;
}

/*****************************************************************
/* The Setup                                                     *
/*****************************************************************/
void setup() {
	Serial.begin(9600);					// Output to Serial at 9600 baud
#if defined(ESP8266)
	Wire.begin(D3,D4);
	esp_chipid = String(ESP.getChipId());
#endif
#if defined(ARDUINO_SAMD_ZERO)
	Wire.begin();
#endif
	copyExtDef();
	readConfig();
	setup_webserver();
	connectWifi();						// Start ConnectWifi
	writeConfig();

	create_basic_auth_strings();

	dht.begin();						// Start DHT
	delay(10);

	if (MDNS.begin(server_name.c_str())) {
		MDNS.addService("http", "tcp", 80);
	}

	// sometimes parallel sending data and web page will stop nodemcu, watchdogtimer set to 30 seconds
	wdt_disable();
	wdt_enable(30000);// 30 sec

	starttime = millis();					// store the start time
}

/*****************************************************************
/* And action                                                    *
/*****************************************************************/
void loop() {
	String data = "";
	String tmp_str;
	String data_sample_times = "";

	String sensemap_path = "";

	String result_DHT = "";
	String signal_strength = "";
	
	unsigned long sum_send_time = 0;
	unsigned long start_send;

	send_failed = false;

	act_micro = micros();
	act_milli = millis();
	send_now = (act_milli-starttime) > sending_intervall_ms;

	sample_count++;

	wdt_reset(); // nodemcu is alive

	if (last_micro != 0) {
		diff_micro = act_micro-last_micro;
		if (max_micro < diff_micro) { max_micro = diff_micro;}
		if (min_micro > diff_micro) { min_micro = diff_micro;}
		last_micro = act_micro;
	} else {
		last_micro = act_micro;
	}

	if (send_now) {
		if (dht_read) {
			result_DHT = sensorDHT();			// getting temperature and humidity (optional)
		}
	}

	if (send_now) {
		data = data_first_part;
		data_sample_times  = Value2Json("samples",String(long(sample_count)));
		data_sample_times += Value2Json("min_micro",String(long(min_micro)));
		data_sample_times += Value2Json("max_micro",String(long(max_micro)));

		signal_strength = String(WiFi.RSSI())+" dBm";

		if (dht_read) {
			last_result_DHT = result_DHT;
			data += result_DHT;
		}

		data_sample_times += Value2Json("signal",signal_strength);
		data += data_sample_times;

		if ( result_DHT.length() > 0) {
			data.remove(data.length()-1);
		}
		data += "]}";

		//sending to api(s)

//		if ((act_milli-last_update_attempt) > pause_between_update_attempts) {
//			will_check_for_update = true;
//		}

		if (send2csv) {
			send_csv(data);
		}

//		if ((act_milli-last_update_attempt) > (28 * pause_between_update_attempts)) {
//			ESP.restart();
//		}


		if (! send_failed) sending_time = (4 * sending_time + sum_send_time) / 5;
		// Resetting for next sampling
		last_data_string = data;
		lowpulseoccupancyP1 = 0;
		lowpulseoccupancyP2 = 0;
		sample_count = 0;
		last_micro = 0;
		min_micro = 1000000000;
		max_micro = 0;
		starttime = millis(); // store the start time
		if (WiFi.status() != WL_CONNECTED) {  // reconnect if connection lost
			int retry_count = 0;
			WiFi.reconnect();
			while ((WiFi.status() != WL_CONNECTED) && (retry_count < 20)) {
				delay(500);
				retry_count++;
			}
		}
	}
	if (config_needs_write) { writeConfig(); create_basic_auth_strings(); }
	server.handleClient();
	yield();
}
