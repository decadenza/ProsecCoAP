#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

#define LEDP 9

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress dev_ip(10, 0, 0, 99); // Set your own.

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t);

// CoAP server endpoint url callback
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port);

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// LED STATE
bool LEDSTATE;

// CoAP server endpoint URL
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port)
{
  Serial.println("[Light] ON/OFF");

  // send response
  char p[2];                                // 1 character + null terminator
  p[0] = ((const char *)packet.payload)[0]; // Only the first character from the payload is considered here.
  p[1] = '\0';

  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if (message.equals("1"))
    LEDSTATE = true;

  if (LEDSTATE)
  {
    digitalWrite(LEDP, HIGH);
    coap.sendResponse(ip, port, packet, COAP_CONTENT, "1", 1, COAP_TEXT_PLAIN);
  }
  else
  {
    digitalWrite(LEDP, LOW);
    coap.sendResponse(ip, port, packet, COAP_CONTENT, "0", 1, COAP_TEXT_PLAIN);
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

  Ethernet.begin(mac, dev_ip);
  Serial.print("My IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

  // LED State
  pinMode(LEDP, OUTPUT);
  digitalWrite(LEDP, HIGH);
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
  // send GET or PUT coap request to CoAP server.
  // To test, use libcoap, microcoap server...etc
  Serial.println("Send Request");
  // Constantly send a GET request.
  // Set your own IP to receive it.
  coap.sendGetRequest(IPAddress(10, 0, 0, 1), 5683, "time");

  delay(1000);
  coap.loop();
}
/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/
