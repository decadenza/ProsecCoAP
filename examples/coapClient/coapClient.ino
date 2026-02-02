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

  // Build a GET request message to transmit.
  Coap::Message msg(Coap::MessageType::Con, Coap::MessageCode::Get); // Initialise a new CoAP confirmable message, as GET request.
  // Optionally, add a token of 4 bytes. Use getToken() to retrieve it later.
  msg.addToken(4);                                                   
  Serial.print("Added token of length: ");
  Serial.println(msg.getTokenLength());

  Coap::ErrorCode err = msg.addHost(destinationIp);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
    
  // Add CoAP options using the constructor...
  Coap::Option newOption(Coap::OptionNumber::UriPath,(const uint8_t *)"abcdefg", 7);
  err = msg.addOption(newOption);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
  // ...or passing the values directly.
  err = msg.addOption(Coap::OptionNumber::UriPath, (const uint8_t *)"test", 4);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
  
  // SECTION Read the options from the message.
  // TEMP: Move to server example.
  Coap::OptionIterator optIterator = msg.getOptionIterator();
  Coap::Option opt;
  while((err = optIterator.next(opt)) == Coap::ErrorCode::None) {
      Serial.print("Found option: ");
      Serial.print((int8_t)opt.number);
      Serial.print(", length: ");
      Serial.println((int8_t)opt.length);
    }
  if(err!= Coap::ErrorCode::NotFound) {
    Serial.print("Error while reading options: ");
    Serial.println((int8_t)err);
    }
   // !SECTION

  delay(1000);
  // coap.loop();
}
