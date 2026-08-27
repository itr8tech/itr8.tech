# Host-side build for the pure layers: ISO-TP reassembly, decoders, and the
# offset finder. Needs nothing but a C++ compiler — no PlatformIO, no ESP32
# toolchain, no hardware. Firmware builds go through PlatformIO (see README).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
BUILD    := build

PURE_SRC := src/transport/isotp.cpp src/leaf/decode.cpp

.PHONY: all test find-offsets clean
all: test find-offsets

test: $(BUILD)/host_tests
	@./$(BUILD)/host_tests

$(BUILD)/host_tests: $(PURE_SRC) test/host/main.cpp test/host/test_isotp.cpp test/host/test_decode.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

find-offsets: $(BUILD)/find_offsets

$(BUILD)/find_offsets: $(PURE_SRC) tools/find_offsets.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
