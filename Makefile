CXX = g++
# -march=native tells the compiler to use your specific CPU's advanced instructions (like AVX2)
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -march=native
LDFLAGS = -luring

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET = $(BUILD_DIR)/chronos_test

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	@echo "--- Executing $(TARGET) ---"
	@./$(TARGET)

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)
	rm -f *.log

.PHONY: all run clean