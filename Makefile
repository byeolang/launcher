CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SRC := codex_launcher_prototype.cpp
BIN_DIR := bin
OUT := $(BIN_DIR)/byeol
WORKER := ../toolchains/local/byeol-exec
DEMO_SCRIPT := ../examples/oneshot-demo.by

.PHONY: all build run demo-run clean

all: build

build: $(OUT)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OUT): $(SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

run: $(OUT)
	cd $(BIN_DIR) && ./byeol --help

demo-run: $(OUT)
	@cd $(BIN_DIR) && chmod 755 $(WORKER) && \
	set +e; \
	./byeol run $(DEMO_SCRIPT) --demo-flag sample; \
	status=$$?; \
	echo "launcher exit code: $$status"; \
	test $$status -eq 17

clean:
	rm -f $(OUT)
