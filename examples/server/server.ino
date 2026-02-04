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
 * Note that on some boards, ON/OFF logic may be inverted.
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
  Serial.begin(115200);
  while (!Serial)
  {
  } // Wait for serial.

  Ethernet.begin(mac, dev_ip);

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
  Serial.print("Coap node started on ");
  Serial.println(Ethernet.localIP());
}

void loop()
{
  Serial.println(".");
  coapNode.loop();
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
    // Send a response to a PUT requests.
    Coap::Message response;
    Coap::Message::buildResponse(&message, Coap::MessageCode::Changed, response);
    coapNode.sendMessage(response, ip, port);
  }
  else if(message.getCode() == Coap::MessageCode::Get) {
    // Send a piggybacked response to a GET requests.
    Coap::Message response;
    Coap::Message::buildResponse(&message, Coap::MessageCode::Content, response);
    response.addPayload((const uint8_t *)(LED_STATUS ? "1" : "0"), 1, Coap::ContentFormat::TextPlain);
    coapNode.sendMessage(response, ip, port);
    }
  
  // NOTE: If you cannot reply immediately, the protocol allows to:
  // 1. First, send an empty acknowledgement (to stop retransmission).
  // 2. When data is ready, send a separate response.

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
