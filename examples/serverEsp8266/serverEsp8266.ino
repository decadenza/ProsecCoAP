/**
 * @file serverEsp8266.ino
 * @brief A CoAP server example for ESP8266 using WiFi.
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ProsecCoAP.h>

const char *ssid = "your-ssid";
const char *password = "your-password";

// CoAP client response callback
void callbackResponse(Coap::Message &message, IPAddress, uint16_t);

// CoAP server path url callback
void callbackLight(Coap::Message &message, IPAddress ip, uint16_t port);

WiFiUDP udp;
Coap::Node coapNode(udp);

// Track LED status at runtime.
bool LED_STATUS;
const int LEDP = 2; // On-board LED pin for ESP8266 NodeMCU. Change as needed.

void setup()
{
  Serial.begin(115200);

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
  pinMode(LEDP, OUTPUT);
  digitalWrite(LEDP, HIGH);
  LED_STATUS = true;

  // Add server url paths.
  Serial.println("Setup callback light");
  coapNode.serve("light", callbackLight);

  // Handler acknowledgment responses.
  // This is a single handler for all ACK responses.
  Serial.println("Setup Response Callback");
  coapNode.setResponseHandler(callbackResponse);

  coapNode.start();
  Serial.println("Coap node started");
}

void loop()
{
  delay(1000);
  coapNode.loop();
}

// CoAP server path URL.
// The expected payload input is one string character.
//
// A GET request will only return the current value.
// A PUT request will set a new value.
// The response will be a string, either "1" or "0".
void callbackLight(Coap::Message &message, IPAddress ip, uint16_t port)
{
  if (message.getCode() == Coap::MessageCode::PUT)
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
    response.intoResponse(message, Coap::MessageCode::CHANGED);
    coapNode.sendMessage(response, ip, port);
  }
  else if (message.getCode() == Coap::MessageCode::GET)
  {
    // Send a piggybacked response to a GET requests.
    Coap::Message response;
    response.intoResponse(message, Coap::MessageCode::CONTENT);
    response.addPayload((const uint8_t *)(LED_STATUS ? "1" : "0"), 1, Coap::ContentFormat::TEXT_PLAIN);
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
  if (message.getType() == Coap::MessageType::ACK)
  {
    Serial.println("[Coap Response ACK] Message ID ");
    Serial.print(message.getId());
    Serial.print(" from ");
    Serial.print(ip + ":" + port);
    Serial.println();
  }
}
