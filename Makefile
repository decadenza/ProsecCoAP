# Compile all examples for each board.

# Config.
CLI = arduino-cli
ARDUINO_BOARDS = arduino:avr:uno arduino:avr:mega arduino:mbed_giga:giga
ARDUINO_EXAMPLES = messageBuilder client server serverWithObserve


 
# Build for all. The first failure will stop the process.
.PHONY: all
all: arduino esp32 esp8266

# Build for different Arduino boards.
.PHONY: arduino
arduino:
	@for board in $(ARDUINO_BOARDS); do \
		echo "--- Compiling for board: $$board ---"; \
		for sketch in $(ARDUINO_EXAMPLES); do \
			echo "Compiling $$sketch..."; \
			$(CLI) compile --fqbn $$board --library . examples/$$sketch/$$sketch.ino || exit 1; \
		done || exit 1; \
	done

# Build for ESP32 platform
PHONY: esp32
esp32:
	@echo "--- Compiling for board: esp32:esp32:esp32 ---"; \
	$(CLI) compile --fqbn esp32:esp32:esp32 --library . examples/serverEsp32/serverEsp32.ino || exit 1;

# Build for ESP8266 platform. Requires the ESP8266 core. Install with:
# arduino-cli core install esp8266:esp8266 --additional-urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
PHONY: esp8266
esp8266:
	@echo "--- Compiling for board: esp8266:esp8266:nodemcuv2 ---"; \
	echo "Compiling esp8266..."; \
	$(CLI) compile --fqbn esp8266:esp8266:nodemcuv2 --library . examples/serverEsp8266/serverEsp8266.ino || exit 1;

	# Build all firmware documentation.
.PHONY: doc
doc:
	doxygen
	@echo "Documentation generated at $(shell pwd)/html/index.html"

# Open existing documentation (cross-platform).
.PHONY: doc-open
doc-open: doc
	@if [ "$(OS)" = "Windows_NT" ]; then \
		start "$(shell pwd)/html/index.html"; \
	elif [ "$$(uname -s)" = "Darwin" ]; then \
		open "$(shell pwd)/html/index.html"; \
	else \
		xdg-open "$(shell pwd)/html/index.html"; \
	fi


.PHONY: test
test:
	git submodule sync --recursive
	git submodule update --init --recursive
	@echo "--- Running unit tests ---"; \
	for test_file in $$(find tests -name '*_test.cpp'); do \
		echo "Running unit tests: $$test_file"; \
		runner="$$test_file.out"; \
		g++ -std=c++11 -I./tests/mocks -I./src -I./tests/unity/src $$test_file ./tests/unity/src/unity.c -o $$runner || exit 1; \
		./$$runner || exit 1; \
		rm $$runner; \
	done
