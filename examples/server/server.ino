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
 * coap-server-notls -v 9
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

// CoAP server path callback.
void callbackLight(Coap::Message &packet, IPAddress ip, uint16_t port);

// CoAP response handler callback.
void callbackResponse(Coap::Message &packet, IPAddress ip, uint16_t port);

// UDP and CoAP instances.
EthernetUDP Udp;
Coap::Node coapNode(Udp);

bool LED_STATUS;

void setup()
{
  Serial.begin(9600);

  Ethernet.begin(mac, dev_ip);
  Serial.print("Server running on: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

  // LED state.
  pinMode(LEDP, OUTPUT);
  digitalWrite(LEDP, HIGH);
  LED_STATUS = true;

  // Serve the light endpoint linked to callbackLight function.
  coapNode.serve("light", callbackLight);

  // This is a single handler for all responses.
  coapNode.setResponseHandler(callbackResponse);

  // Start coap node.
  coapNode.start();
}

void loop()
{
  // // send GET or PUT coap request to CoAP server.
  // // To test, use libcoap, microcoap server...etc
  // Serial.println("Send Request");
  // // Constantly send a GET request.
  // // Set your own IP to receive it.
  // coap.sendGetRequest(IPAddress(192, 168, 0, 100), COAP_DEFAULT_PORT, "time"); // Set your own remote IP.

  // coap.loop();
  delay(1000);
}


// CoAP server path URL.
// The expected payload input is one string character.
//
// A GET request will only return the current value.
// A PUT request will set a new value.
// The response will be a string, either "1" or "0".
void callbackLight(Coap::Message &message, IPAddress ip, uint16_t port)
{
   if (message.getCode() == Coap::MessageCode::Put)
   {
     Serial.println("Incoming PUT");
     const uint8_t *payload;
     size_t length;
     message.getPayload(payload, length); // You should check for any returned error.
     // Process incoming value, considering first byte only.
     LED_STATUS = (reinterpret_cast<const char *>(payload))[0] == '1' ? HIGH : LOW;
     digitalWrite(LEDP, LED_STATUS);
   }

  // // SECTION: OPTION 1
  // // Payload is ready. Send a piggybacked response.
  // coap.sendResponse(ip, port, packet, COAP_CONTENT, LED_STATUS ? "1" : "0", 1, COAP_TEXT_PLAIN);
  // // !SECTION END OF OPTION 1

  // // SECTION: OPTION 2
  // // Send an empty acknowledgement followed by a separate response.
  // // Payload may not be ready. Send an Empty Ackwnoledgement to tell the client that the request has been received...
  // // coap.sendEmptyAcknowledgement(ip, port, packet);
  // // delay(50); // Simulate some delay caused by processing...
  // //... and when the payload is ready, send a "separate response".
  // // coap.sendSeparateResponse(ip, port, packet, COAP_CONTENT, LED_STATUS ? "1" : "0", 1, COAP_TEXT_PLAIN);
  // // !SECTION END OF OPTION 2

  Serial.print("[Light] ");
  Serial.println(LED_STATUS);
}

// CoAP client response callback. This will receive the ACK responses.
void callbackResponse(Coap::Message &message, IPAddress ip, uint16_t port)
{
   if (message.getType() == Coap::MessageType::Ack)
   {
     Serial.println("[Coap Response ACK] Message ID ");
     Serial.print(message.getId());
     Serial.print(" from ");
     Serial.print(ip + ":" + port);
     Serial.println();
   }
}
