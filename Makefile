# ============================================================================
#  Frostmark - Makefile
#
#   make            build ottimizzata      -> ./frostmark
#   make debug      con simboli e -O0
#   make run        compila ed esegue
#   make web        build WebAssembly (richiede emscripten)
#   make clean
#
#  raylib puo' essere:
#   a) installata nel sistema        -> rilevata via pkg-config
#   b) messa in ./vendor/raylib      -> make RAYLIB_PATH=vendor/raylib
# ============================================================================

CC       ?= cc
TARGET   := frostmark
SRC_DIR  := src
BUILD_DIR:= build
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

CFLAGS_COMMON := -std=c99 -Wall -Wextra -Wno-unused-result -I$(SRC_DIR)
CFLAGS  ?= -O2
LDFLAGS :=
LDLIBS  := -lm

UNAME_S := $(shell uname -s)

# ---- raylib: percorso esplicito oppure pkg-config --------------------------
ifdef RAYLIB_PATH
  CFLAGS_COMMON += -I$(RAYLIB_PATH)/include
  LDFLAGS       += -L$(RAYLIB_PATH)/lib
  LDLIBS        := -lraylib $(LDLIBS)
else
  RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
  RAYLIB_LIBS   := $(shell pkg-config --libs   raylib 2>/dev/null)
  ifeq ($(strip $(RAYLIB_LIBS)),)
    LDLIBS := -lraylib $(LDLIBS)
  else
    CFLAGS_COMMON += $(RAYLIB_CFLAGS)
    LDLIBS        := $(RAYLIB_LIBS) $(LDLIBS)
  endif
endif

# ---- dipendenze specifiche per sistema operativo ---------------------------
ifeq ($(UNAME_S),Linux)
  LDLIBS += -lpthread -ldl -lrt -lX11
endif
ifeq ($(UNAME_S),Darwin)
  LDLIBS += -framework CoreVideo -framework IOKit -framework Cocoa \
            -framework GLUT -framework OpenGL
endif
ifeq ($(OS),Windows_NT)
  LDLIBS += -lopengl32 -lgdi32 -lwinmm
  TARGET := frostmark.exe
endif

.PHONY: all debug run clean web dirs

all: dirs $(TARGET)

debug: CFLAGS := -O0 -g -DDEBUG
debug: clean all

dirs:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS_COMMON) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "==> $(TARGET) pronto. Avvia con: ./$(TARGET) [seme]"

run: all
	./$(TARGET)

# ---- WebAssembly -----------------------------------------------------------
# Richiede emsdk attivo e raylib compilata per web in $(RAYLIB_WEB).
RAYLIB_WEB ?= vendor/raylib-web
web:
	emcc -o $(TARGET).html $(SRCS) \
	  -Os -Wall -std=c99 -I$(SRC_DIR) -I$(RAYLIB_WEB)/include \
	  $(RAYLIB_WEB)/lib/libraylib.a \
	  -s USE_GLFW=3 -s ASYNCIFY -s TOTAL_MEMORY=134217728 \
	  -s FORCE_FILESYSTEM=1 --preload-file assets \
	  -DPLATFORM_WEB

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe $(TARGET).html \
	       $(TARGET).js $(TARGET).wasm $(TARGET).data
