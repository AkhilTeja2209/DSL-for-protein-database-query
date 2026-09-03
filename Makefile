# Makefile
# Two builds from one set of sources:
#
#   make        -> bin/proteindsl, the native CLI.  Needs flex, bison, g++ (C++17).
#   make wasm   -> docs/, the GitHub Pages site.    Needs the above plus emcc.
#
# See README "Tech Stack" and "Deploying to GitHub Pages".

CXX      := g++
# Used by `make wasm` and `make serve`. Emscripten itself is written in Python,
# so the browser build already depends on it. python3 on CI, python on Windows.
PYTHON   ?= python
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
BUILD    := build
BIN      := bin/proteindsl

# Everything except the front end; main.cpp and wasm_api.cpp are the two
# alternative entry points onto the same pipeline.
CORE     := ast.cpp semantic.cpp executor.cpp pipeline.cpp
SRCS     := $(CORE) main.cpp
OBJS     := $(SRCS:%.cpp=$(BUILD)/%.o) $(BUILD)/lexer.yy.o $(BUILD)/parser.tab.o

GENERATED := $(BUILD)/parser.tab.cpp $(BUILD)/lexer.yy.cpp

.PHONY: all clean clean-wasm run-valid run-invalid wasm serve

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
	./$(BIN) sample_queries/valid_query.dsl

# This sample is supposed to fail semantic analysis (exit 3), so the leading
# "-" stops make from reporting the expected failure as a build error.
run-invalid: $(BIN)
	-./$(BIN) sample_queries/invalid_query.dsl

# docs/ is a plain static directory -- any web server will do; this is just
# the one that needs no installing.
serve: wasm
	$(PYTHON) -m http.server -d $(DOCS) 8080

# --------------------------------------------------------------------------
# WebAssembly build -> docs/, which GitHub Pages serves directly.
#
# ASYNCIFY is what lets LOAD UNIPROT await a browser fetch from inside the
# executor without restructuring it (see js_fetch_text in executor.cpp).
# --preload-file puts dataset/proteins.csv in the virtual filesystem, so
# LOAD "dataset/proteins.csv" works in the browser exactly as it does natively.
# --------------------------------------------------------------------------
EMCC     := em++
DOCS     := docs
WASM_SRCS := $(CORE) wasm_api.cpp $(GENERATED)

EMFLAGS := -std=c++17 -O2 -I. -I$(BUILD) \
	-sASYNCIFY \
	-sMODULARIZE -sEXPORT_NAME=createProteinDSL \
	-sALLOW_MEMORY_GROWTH \
	-sEXPORTED_FUNCTIONS=_proteindsl_run_json,_malloc,_free \
	-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString,stringToNewUTF8 \
	--preload-file dataset

wasm: $(GENERATED)
	mkdir -p $(DOCS)
	$(EMCC) $(EMFLAGS) $(WASM_SRCS) -o $(DOCS)/proteindsl.js
	cp web/index.html web/app.css web/app.js $(DOCS)/
	$(PYTHON) tools/make_examples.py $(DOCS)/examples.json
	@echo "docs/ is ready -- serve it with: make serve"

clean:
	rm -rf $(BUILD) bin

clean-wasm:
	rm -rf $(DOCS)
