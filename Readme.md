All Credits to opendata-stuttgart/sensors-software !

Die ursprüngliche Programmierung vom Projekt luftdaten.info wurde übernommen, um eine eigene Sensorplattform zu erstellen.

# Version für Sensoren DHT22

Features:
* gleichzeitiger Betrieb mehrerer Sensoren
* Konfiguration über WLAN (Sensor als Access Point) möglich
* nutzbar für ESP8266

ToDo's:
*

Die grundsätzliche Konfiguration der Parameter erfolgt über die Datei `ext_dev.h`.

## WLAN Konfiguration

Wenn das vorgegebene WLAN nach 10 Sekunden nicht erreichbar ist, wird ein Access-Point eingerichtet, der über "Feinstaubsensor-\[Sensor-ID\]" erreichbar ist. Nach dem Verbinden zu diesem Accesspoint sollten alle Anfragen auf die Konfigurationsseite umgeleitet werden. Direkte Adresse der Seite ist http://192.168.4.1/ .

Konfigurierbar sind:
* WLAN-Name und Passwort


## Speichern als CSV

Die Daten können als CSV via USB ausgegeben werden. Dafür sollte sowohl in ext_def.h als auch in der WLAN-Konfiguration Debug auf 0 gesetzt werden, damit die ausgegebenen Daten nur noch die Sensordaten sind. Beim Neustart des ESP8266 erscheinen dann nur noch ein paar wenige Zeichen, die den Startzustand darstellen.

## Wiring

* SDS and DHT wiring: [pdf/sds_dht_wiring.pdf](pdf/sds_dht_wiring.pdf)

## Benötigte Software (in Klammern getestete Version):

* [Arduino IDE](https://www.arduino.cc/en/Main/Software)  (Version 1.8.1)
* [ESP8266 für Arduino](http://arduino.esp8266.com/stable/package_esp8266com_index.json) (Version 2.3.0)

### Verwendete Bibliotheken (für ESP8266):

In Arduino enthalten:
* Wire

In ESP8266 für Arduino IDE enthalten:
* FS
* ESP8266WiFi
* ESP8266WebServer
* DNSServer

Installierbar über Arduino IDE (Menü Sketch -> Bibliothek einbinden -> Bibliotheken verwalten, in Klammern die getestete Version):
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (5.8.2)
* [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) (1.0.2)
* [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) (1.3.0)
* ESP8266httpUpdate (1.1.0)
* [PubSubClient](http://pubsubclient.knolleary.net/) (2.6.0)
* [SoftwareSerial](https://github.com/plerup/espsoftwareserial) (1.0.0)


## Anschluss der Sensoren

Beim Anschluss von Sensoren mit 5V bitte die Board-Version beachten. NodeMCU v3 liefert 5V an `VU`, Version 2 fehlt dieser Anschluss und `VIN` kann dafür genutzt werden.

### DHT22
* Pin 1 => 3V3
* Pin 2 => Pin D7 / GPIO13
* Pin 3 => unused
* Pin 4 => GND
