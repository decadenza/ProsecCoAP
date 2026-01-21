#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress dev_ip(10, 0, 0, 99); // Set your own.

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, uint16_t port);

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, uint16_t port)
{
  Serial.println("[Coap Response got]");

  Serial.write((const char *)packet.payload, packet.payload_length);
  Serial.println(); // newline
}

void setup()
{
  Serial.begin(9600);

  Ethernet.begin(mac, dev_ip);
  Serial.print("My IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop()
{
  // send GET or PUT coap request to CoAP server.
  // To test, use libcoap, microcoap server...etc
  Serial.println("Send Request");
  coap.getRequest(IPAddress(10, 0, 0, 1), 5683, "time");

  delay(1000);
  coap.loop();
}
