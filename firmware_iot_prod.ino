#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <ESP8266HTTPClient.h>

// Update these with values suitable for your network.
const char* ssid = "Jr";
const char* password = "senha123";
const char* mqtt_server = "11ecd64be4a14dd0b10818f3c750d86b.s2.eu.hivemq.cloud";


BearSSL::CertStore certStore;


WiFiClient espClient;
PubSubClient * client;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;


void http_request() {

  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  http.setTimeout(30000);
  http.begin(espClient, "http://server4iot.herokuapp.com/devices/jose"); //HTTP
  http.addHeader("Content-Type", "application/json");
  
  
  int httpCode = http.POST("{\"name\": \"LED 5\",\"type\": 0}");

  if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  
  }


void setup_wifi() {
  
  
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void setDateTime() {
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  switch ((int)payload[0]){
    //49 = 1 acionar
    case 49:
      if ((int)payload[1] == 49) { 
        digitalWrite(LED_BUILTIN, LOW);
        delay(5000); 
      }
      else{
          digitalWrite(LED_BUILTIN, HIGH); 
      } 
      delay(5000);
      break;
    // 50 = 2 estado 
    case 50:
      Serial.println((int)payload[1]);
      Serial.println("Atuador não aciona e nem desliga"); 
      break; 
    // 51 = 3 keep alive
    case 51:
      delay(1000);
      Serial.println("Publicando 9 no Topico LED 5: ");
      client->publish("LED 5Alive", "9");
      break;
    // 57  = 9 bit lido pois o publish do keep alive está no mesmo topico
    case 57: 
      Serial.println("Enviando mensagem de volta ao Servidor");
      break;
    default: 
      Serial.println("Bit inválido!"); 
      break;
  }
}

void reconnect() {
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection…");
    String clientId = "LED 5";
    if (client->connect(clientId.c_str(), "juninhocb", "palmeiras")) {
      Serial.println("connected");
      client->subscribe("LED 5");
      http_request();
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client->state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void setup() {
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);

  LittleFS.begin();
  setup_wifi();
  setDateTime();

  pinMode(LED_BUILTIN, OUTPUT); // Initialize the LED_BUILTIN pin as an output

  // you can use the insecure mode, when you want to avoid the certificates
  //espclient->setInsecure();

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }

  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);

  client = new PubSubClient(*bear);

  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);
}

void loop() {
  if (!client->connected()) {
    reconnect();
  }
  client->loop();
}
