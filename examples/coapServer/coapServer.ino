/**
 * A CoAP server.
 *
 * The current light status can be read using:
 * ```
 * coap-client-notls -m get coap://192.168.0.99/light
 * ```
 * and the light can be set ON or OFF, respectively, by using:
 * ```
 * coap-client-notls -m put coap://192.168.0.99/light -e "1"
 * coap-client-notls -m put coap://192.168.0.99/light -e "0"
 * ```
 * Note that on some boards, logic may be inverted.
 * 
 * In addition, this example also acts as client, sending a GET request every second.
 * To receive such requests, start a CoAP server on the remote machine. To test, you may use:
 * ```
 * coap-server-notls -A 0.0.0.0 -v 9
 * ```
 */
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

#define LEDP LED_BUILTIN

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress dev_ip(192, 168, 0, 99); // Set your own.

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t);

// CoAP server endpoint url callback
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port);

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// LED STATE
bool LEDSTATE;

// CoAP server endpoint URL.
// The expected payload input is one character.
//
// A GET request will only return the current value.
// A PUT request will set a new value.
// The response will be a string, either "1" or "0".
void callbackLight(CoapPacket &packet, IPAddress ip, uint16_t port)
{
  if(packet.code == COAP_PUT && packet.payloadLength) {
    Serial.println("Incoming PUT");
    // Process incoming value, considering first byte only.
    LEDSTATE = ((const char*)packet.payload)[0] == '1' ? HIGH : LOW; 
    digitalWrite(LEDP, LEDSTATE);
    }
  
  // Send response with current value.
  coap.sendResponse(ip, port, packet, COAP_CONTENT, LEDSTATE?"1":"0", 1, COAP_TEXT_PLAIN);
  Serial.print("[Light] ");
  Serial.println(LEDSTATE);
}

// CoAP client response callback. This will receive the ACK responses.
void callbackResponse(CoapPacket &packet, IPAddress ip, uint16_t port)
{
  if (packet.payloadLength)
  {
    Serial.println("[Coap Response ACK] Message ID ");
    Serial.print(packet.messageId);
    Serial.print(" from ");
    Serial.print(ip + ":" + port);
    Serial.println();
  }
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
  coap.sendGetRequest(IPAddress(192, 168, 0, 100), COAP_DEFAULT_PORT, "time"); // Set your own remote IP.

  delay(1000);
  coap.loop();
}
/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/
