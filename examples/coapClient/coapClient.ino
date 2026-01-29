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
IPAddress deviceIp(192, 168, 0, 99);                   // Set your own.
IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.

// CoAP client response callback
void callbackResponse(Coap::Message &message, IPAddress ip, uint16_t port);

// UDP and CoAP class
EthernetUDP Udp;
// Coap coap(Udp);

// CoAP client response callback
void callbackResponse(Coap::Message &message, IPAddress ip, uint16_t port)
{
  // Serial.print("[Coap Response] ");
  // Serial.write((const char *)message.payload, message.payloadLength);
  // Serial.print(" (token: ");
  // for (size_t i = 0; i < message.tokenLength; i++)
  // {
  //   Serial.print(packet.token[i], HEX);
  // }
  // Serial.print(", message ID:");
  // Serial.print(packet.messageId);
  // Serial.println(")");
}

void setup()
{
  Serial.begin(9600);

  Ethernet.begin(mac, deviceIp);
  Serial.print("IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

  // Handler acknowledgment responses.
  // This is a single handler for all ACK responses.
  Serial.println("Setup Response Callback");
  // coap.responseHandler(callbackResponse);

  // start coap server/client
  // coap.start();
}

void loop()
{
  IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.

  Coap::Message msg(Coap::MessageType::Con, Coap::MessageCode::Get); // Initialise a new CoAP confirmable message, as GET request.
  msg.addToken(4); // Optionally, add a token of 4 bytes. Use getToken() to retrieve it later.

  Serial.print("GET request ready, id: ");
  Serial.println(msg.getId());
  
  // CoapPacket packet;
  // packet.setType(COAP_CON);   // Confirmable.
  // packet.asRequest(COAP_GET); // GET request.
  // packet.setRecipient(destinationIp, "time");

  // // Optionally, set a token.
  // uint8_t token[2];
  // CoapgenerateRandomToken(token, sizeof(token));
  // packet.withToken(token, sizeof(token));

  // // Send and show info.
  // if (coap.sendPacket(packet, destinationIp) == 0) // 0 means success
  //   Serial.print("[GET request] OK");
  // else
  //   Serial.print("[GET request] FAILED");

  // Serial.print(" (message ID: ");
  // Serial.print(packet.messageId);
  // Serial.print(", token: ");
  // for (size_t i = 0; i < packet.tokenLength; i++)
  // {
  //   Serial.print(packet.token[i], HEX);
  // }
  // Serial.println(")");

  delay(1000);
  // coap.loop();
}
