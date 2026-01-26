/**
 * Example CoAP Client.
 *
 * This example sends a GET request to a CoAP server and handles the response.
 *
 * To test this example, you can use a CoAP server such as libcoap or microcoap.
 * You may also test with:
 * ```
 * coap-server-notls -v 9
 * ```
 */
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress dev_ip(192, 168, 0, 99); // Set your own.

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t);

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// CoAP client response callback
void callbackResponse(CoapPacket &packet, IPAddress, uint16_t)
{
  Serial.println("[Coap Response]");
  Serial.write((const char *)packet.payload, packet.payloadLength);
  Serial.print(" (message ID:");
  Serial.print(packet.messageId);
  Serial.println(")");
}

void setup()
{
  Serial.begin(9600);

  Ethernet.begin(mac, dev_ip);
  Serial.print("My IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

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
  coap.sendGetRequest(IPAddress(192, 168, 0, 100), COAP_DEFAULT_PORT, "time"); // Set your CoAP server IP address.

  delay(1000);
  coap.loop();
}
