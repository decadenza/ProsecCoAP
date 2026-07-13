/**
 * @file client.ino
 * @brief Example CoAP Client.
 *
 * This example sends a confirmable GET request to a CoAP server and handles any response.
 *
 * To test this example, you can use a CoAP server such as libcoap or microcoap.
 * You may also test with:
 * ```
 * coap-server-notls -v 9
 * ```
 *
 * Note that coap-server-notls exposes a "time" endpoint that returns the current
 * timestamp as payload.
 */
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress deviceIp(192, 168, 0, 99);                   // Set your own.
IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.
IPAddress dns(192, 168, 0, 1);                         // Optional. Set your own.
IPAddress gateway(192, 168, 0, 1);                     // Optional. Set your own.
IPAddress subnet(255, 255, 255, 0);                    // Optional. Set your own.

// CoAP client response callback
void callbackResponse(Coap::Message &message, IPAddress ip, uint16_t port);

// UDP and CoAP instances.
EthernetUDP Udp;
Coap::Node coapNode(Udp);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
  } // Wait for serial.

  // NOTE: You may use DHCP instead of static IP. In that case, call `Ethernet.begin(mac)` instead.
  // Also Ethernet.begin(mac, ip) will work, but it will use the default DNS, gateway and subnet.
  // Please refer to Arduino documentation.
  Ethernet.begin(mac, deviceIp, dns, gateway, subnet);

  // This is a single handler for all responses.
  coapNode.setResponseHandler(callbackResponse);

  // Start coap node.
  coapNode.start();
  Serial.print("Coap node started on ");
  Serial.println(Ethernet.localIP());
}

void loop()
{
  // Build a GET request message to transmit.
  Coap::Message msg(Coap::MessageType::CON, Coap::MessageCode::GET); // Initialise a new CoAP confirmable message, as GET request.
  msg.addPath("time");                                               // Set the URI path to "time".
  msg.addRandomToken(4);                                             // OPTIONAL: Add a random token of 4 bytes.

  coapNode.sendMessage(msg, destinationIp, COAP_DEFAULT_PORT);
  Serial.print("[Request] id=");
  Serial.println(msg.getId());

  // Even when acting as client, we still need to run the loop housekeeping.
  coapNode.loop();

  delay(1000);
}

// CoAP client response callback.
// If a payload is present, print it as plain text.
// NOTE: According to protocol specifications, you should check the Content-Format option first before attempting to
// parse the payload.
void callbackResponse(Coap::Message &message, IPAddress ip, uint16_t port)
{
  Serial.print("[Response] id=");
  Serial.print(message.getId());
  Serial.print(" length=");
  Serial.print(message.getLength());
  // Read the payload.
  const uint8_t *payload;
  size_t payloadLength;
  message.getPayload(payload, payloadLength);
  // NOTE: The payload will be an array of bytes and needs to be interpreted.
  // In this example we just print it as is.
  Serial.print(" payload=");
  Serial.write(payload, payloadLength);
  Serial.println();
}
