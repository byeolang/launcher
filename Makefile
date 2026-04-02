CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SRC := codex_launcher_prototype.cpp
OUT := codex_launcher_prototype
WORKER := toolchains/local/byeol-exec
DEMO_SCRIPT := examples/oneshot-demo.by

.PHONY: all build run demo-run clean

all: build

build: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

run: $(OUT)
	./$(OUT) --help

demo-run: $(OUT)
	chmod 755 $(WORKER)
	@set +e; \
	./$(OUT) run $(DEMO_SCRIPT) --demo-flag sample; \
	status=$$?; \
	echo "launcher exit code: $$status"; \
	test $$status -eq 17

clean:
	rm -f $(OUT)
