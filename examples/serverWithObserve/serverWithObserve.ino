/*
 * A CoAP server with Observe functionality.
 *
 * Author: Pasquale Lafiosca (2025)
 *
 * To test this example with the coap-client tool from libcoap:
 * ```
 * coap-client-notls -m get -s 5 coap://192.168.0.1/subscribe
 * ```
 * Note that at exit the unsubscribe call will be sent
 * automatically by coap-client-notls.
 */
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

// --- DEBUG MACROS ---
// Set to 1 to enable Serial output for debugging. Set to 0 to disable all Serial calls (no-ops).
#define ENABLE_SERIAL_DEBUG 1

#if ENABLE_SERIAL_DEBUG
#define SERIAL_BEGIN(baud) Serial.begin(baud)
#define SERIAL_WHILE_WAIT \
    while (!Serial)       \
        ;
#define SERIAL_PRINT(x) Serial.print(x)
#define SERIAL_PRINT_HEX(x) Serial.print(x, HEX)
#define SERIAL_PRINTLN(x) Serial.println(x)
#define SERIAL_WRITE(x) Serial.write(x)
#else
#define SERIAL_BEGIN(baud)
#define SERIAL_WHILE_WAIT
#define SERIAL_PRINT(x)
#define SERIAL_PRINT_HEX(x)
#define SERIAL_PRINTLN(x)
#define SERIAL_WRITE(x)
#endif

// UDP and CoAP instances.
EthernetUDP Udp;
Coap::Node coapNode(Udp);

// Define an observer registry with maximum capacity for 5 observers.
// This registry is binded to a specific resource!
Coap::ObserverRegistry<5> myObservers;

// Using a sequential identifier. IP will be based on this.
#define DEVICE_ID 1

byte mac[] = {0xBE, 0xEF, 0xBE, 0xEF, 0x00, DEVICE_ID}; // Define the MAC address, this must be unique.
IPAddress ip(192, 168, 0, DEVICE_ID);                   // This device IP.

// Declaration of our observe callback.
void observeCallback(Coap::Message &message, IPAddress ip, uint16_t port);

void setup()
{
    // Initialize serial and wait for port to open.
    SERIAL_BEGIN(115200);
    SERIAL_WHILE_WAIT;
    SERIAL_PRINTLN("Booting...");
    SERIAL_PRINT("Configuring Ethernet...");
    Ethernet.begin(mac, ip);
    // Check for hardware issues
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        SERIAL_PRINTLN("Ethernet shield/hardware not found. Check connections!");
        return;
    }
    SERIAL_PRINTLN("OK");

    coapNode.serve("observe", observeCallback); // Serve the "observe" path using the observeCallback.

    // Start coap server.
    coapNode.start();

    SERIAL_PRINT("Server listening on ");
    SERIAL_PRINT(Ethernet.localIP());
    SERIAL_PRINT(":");
    SERIAL_PRINTLN(coapNode.getPort());

    SERIAL_PRINTLN("Initialisation completed!");
}

void loop()
{
    coapNode.loop(); // Process coap requests.

    sendNotification(); // Simulate a recurring notification.
}

// Handle observe registration and deregistration requests.
void observeCallback(Coap::Message &message, IPAddress ip, uint16_t port)
{

    if (message.isObserveRegister())
    {
        // This is a subscription request. Add a new observer in the registry.
        Coap::ErrorCode err = myObservers.add(ip, port, message.getToken(), message.getTokenLength());
        Coap::Message response;
        if (err == Coap::ErrorCode::OK)
        {
            // Send ACK response.
            Coap::Message::buildResponse(message, Coap::MessageCode::VALID, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Subscribed!");
        }
        else
        {
            // Tell the client that the subscription failed.
            Coap::Message::buildResponse(message, Coap::MessageCode::SERVICE_UNAVAILABLE, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Observer could not be added!");
        }
    }
    else if (message.isObserveDeregister())
    {
        Coap::ErrorCode err = myObservers.remove(ip, port, message.getToken(), message.getTokenLength());
        Coap::Message response;
        if (err == Coap::ErrorCode::OK)
        {
            Coap::Message::buildResponse(message, Coap::MessageCode::VALID, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Unsubscribed!");
        }
        else
        {
            // Tell the client that the subscription failed.
            Coap::Message::buildResponse(message, Coap::MessageCode::SERVICE_UNAVAILABLE, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Observer could not be removed!");
        }
    }
    else
    {
        // Not a valid observe request. Ignore.
        SERIAL_PRINTLN("Missing/invalid observe value.");
    }
}

// Demo notification with gibberish data.
void sendNotification()
{
    char payload[] = "The answer is 42\n";
    size_t payloadLength = strlen(payload);

    // if (coap.notifyObservers("subscribe", payload, payloadLength, COAP_TEXT_PLAIN) > 0)
    // {
    //     SERIAL_PRINTLN("Notified!");
    // }
}
