CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD := build

SRCS := src/pinyin.cpp src/engine.cpp src/c_api.cpp
OBJS := $(SRCS:src/%.cpp=$(BUILD)/%.o)

.PHONY: all test clean

all: $(BUILD)/libime_core.a

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Iinclude -Isrc -c $< -o $@

$(BUILD)/libime_core.a: $(OBJS)
	ar rcs $@ $(OBJS)

$(BUILD)/test_engine: tests/test_engine.cpp $(BUILD)/libime_core.a
	$(CXX) $(CXXFLAGS) -Iinclude tests/test_engine.cpp $(BUILD)/libime_core.a -o $@

test: $(BUILD)/test_engine
	$(BUILD)/test_engine ../data/dict.tsv $(BUILD)

clean:
	rm -rf $(BUILD)
