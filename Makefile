# Makefile
# Builds bin/proteindsl from the Flex lexer, Bison parser and C++ sources.
# Requires: flex, bison, g++ (C++17). See README "Tech Stack".

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
BUILD    := build
BIN      := bin/proteindsl

SRCS     := ast.cpp semantic.cpp executor.cpp main.cpp
OBJS     := $(SRCS:%.cpp=$(BUILD)/%.o) $(BUILD)/lexer.yy.o $(BUILD)/parser.tab.o

.PHONY: all clean run-valid run-invalid

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD) bin

# Bison: generates parser.tab.cpp and parser.tab.hpp (the token/type defs
# the lexer includes).
$(BUILD)/parser.tab.cpp $(BUILD)/parser.tab.hpp: parser.y | $(BUILD)
	bison -d -o $(BUILD)/parser.tab.cpp parser.y

# Flex: generates the lexer implementation; depends on the Bison header
# for token codes.
$(BUILD)/lexer.yy.cpp: lexer.l $(BUILD)/parser.tab.hpp | $(BUILD)
	flex -o $(BUILD)/lexer.yy.cpp lexer.l

$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(BUILD) -c $< -o $@

$(BUILD)/lexer.yy.o: $(BUILD)/lexer.yy.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(BUILD) -c $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(BUILD) -c $< -o $@

$(BIN): $(OBJS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(BIN)

run-valid: $(BIN)
	./$(BIN) main/sample_queries/valid_query.dsl

run-invalid: $(BIN)
	./$(BIN) main/sample_queries/invalid_query.dsl

clean:
	rm -rf $(BUILD) bin
