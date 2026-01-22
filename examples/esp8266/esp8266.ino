#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ProsecCoAP.h>

const char *ssid = "your-ssid";
const char *password = "your-password";

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t);

// CoAP server endpoint url callback
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port);

// UDP and CoAP class
// other initialize is "Coap coap(Udp, 512);"
// 2nd default parameter is COAP_BUF_MAX_SIZE(defaulit:128)
// For UDP fragmentation, it is good to set the maximum under
// 1280byte when using the internet connection.
WiFiUDP udp;
Coap coap(udp);

// LED STATE
bool LEDSTATE;

// CoAP server endpoint URL
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port)
{
  Serial.println("[Light] ON/OFF");

  // Expects one byte payload: "0" or "1" in ASCII.
  char p[2]; // Include space for null terminator
  memcpy(p, packet.payload, 1);
  p[1] = '\0';

  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if (message.equals("1"))
    LEDSTATE = true;

  if (LEDSTATE)
  {
    digitalWrite(9, HIGH);
    coap.sendResponse(ip, port, packet.messageId, "1");
  }
  else
  {
    digitalWrite(9, LOW);
    coap.sendResponse(ip, port, packet.messageId, "0");
  }
}

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t)
{
  Serial.println("[Coap Response got]");

  Serial.write((const char *)packet.payload, packet.payloadLength);
  Serial.println(); // newline
}

void setup()
{
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // LED State
  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);
  LEDSTATE = true;

  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback Light");
  coap.server(callbackLight, "light");

  // Handler acknowledgment responses.
  // This is a single handler for all ACK responses.
  Serial.println("Setup Response Callback");
  coap.acknowledgeWith(callbackResponse);

  // start coap server/client
  coap.start();
}

void loop()
{
  delay(1000);
  coap.loop();
}
/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/
