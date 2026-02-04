# Compile all examples for each board.

# Config
CLI = arduino-cli
ARDUINO_BOARDS = arduino:avr:uno arduino:avr:mega arduino:mbed_giga:giga
ARDUINO_EXAMPLES = messageBuilder client server serverWithObserve


.PHONY: all	arduino esp32 esp8266 

# Build for all. The first failure will stop the process.
all: arduino esp32 esp8266

# Build for different Arduino boards.
arduino:
	@for board in $(ARDUINO_BOARDS); do \
		echo "--- Compiling for board: $$board ---"; \
		for sketch in $(ARDUINO_EXAMPLES); do \
			echo "Compiling $$sketch..."; \
			$(CLI) compile --fqbn $$board --library . examples/$$sketch/$$sketch.ino || exit 1; \
		done || exit 1; \
	done

# Build for ESP32 platform
esp32:
	@echo "--- Compiling for board: esp32:esp32:esp32 ---"; \
	$(CLI) compile --fqbn esp32:esp32:esp32 --library . examples/serverEsp32/serverEsp32.ino || exit 1;

# Build for ESP8266 platform. Requires the ESP8266 core. Install with:
# arduino-cli core install esp8266:esp8266 --additional-urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
esp8266:
	@echo "--- Compiling for board: esp8266:esp8266:nodemcuv2 ---"; \
	echo "Compiling esp8266..."; \
	$(CLI) compile --fqbn esp8266:esp8266:nodemcuv2 --library . examples/serverEsp8266/serverEsp8266.ino || exit 1;