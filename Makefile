# ============================================================================
#  Frostmark - Makefile
#
#   make            build ottimizzata      -> ./frostmark
#   make debug      con simboli e -O0
#   make run        compila ed esegue
#   make raylib     compila raylib dal submodule vendor/raylib (una volta sola)
#   make clean
#
#  raylib viene cercata in quest'ordine:
#   1) RAYLIB_PATH=dir   release precompilata, con sottocartelle include/ e lib/
#   2) vendor/raylib     submodule dei sorgenti -> libreria statica ('make raylib')
#   3) pkg-config        installazione di sistema
#   4) -lraylib          ultimo tentativo, sperando che il linker la trovi
# ============================================================================

CC       ?= cc
TARGET   := frostmark
SRC_DIR  := src
BUILD_DIR:= build
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# -MMD -MP: genera i file .d con le dipendenze dagli header, cosi' modificare
# config.h o player.h ricompila quello che serve invece di lasciare oggetti
# vecchi in build/.
CFLAGS_COMMON := -std=c99 -Wall -Wextra -Wno-unused-result -I$(SRC_DIR) -MMD -MP
CFLAGS  ?= -O2
LDFLAGS :=
LDLIBS  := -lm

UNAME_S := $(shell uname -s)

# ---- raylib ----------------------------------------------------------------
RAYLIB_SRC    := vendor/raylib/src
RAYLIB_STATIC := $(RAYLIB_SRC)/libraylib.a

ifdef RAYLIB_PATH
  CFLAGS_COMMON += -I$(RAYLIB_PATH)/include
  LDFLAGS       += -L$(RAYLIB_PATH)/lib
  LDLIBS        := -lraylib $(LDLIBS)
else ifneq ($(wildcard $(RAYLIB_SRC)/raylib.h),)
  # Submodule presente: si linka la statica, ricompilandola se manca.
  CFLAGS_COMMON += -I$(RAYLIB_SRC)
  RAYLIB_DEP    := $(RAYLIB_STATIC)
  LDLIBS        := $(RAYLIB_STATIC) $(LDLIBS)
  RAYLIB_GL_LIB := -lGL            # con la statica va linkata a mano
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
  LDLIBS += $(RAYLIB_GL_LIB) -lpthread -ldl -lrt -lX11
endif
ifeq ($(UNAME_S),Darwin)
  LDLIBS += -framework CoreVideo -framework IOKit -framework Cocoa \
            -framework GLUT -framework OpenGL
endif
ifeq ($(OS),Windows_NT)
  LDLIBS += -lopengl32 -lgdi32 -lwinmm
  TARGET := frostmark.exe
endif

.PHONY: all debug run clean dirs raylib raylib-clean

all: dirs $(TARGET)

debug: CFLAGS := -O0 -g -DDEBUG
debug: clean all

dirs:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS_COMMON) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(RAYLIB_DEP)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "==> $(TARGET) pronto. Avvia con: ./$(TARGET) [seme]"

run: all
	./$(TARGET)

-include $(OBJS:.o=.d)

# ---- raylib dal submodule --------------------------------------------------
# Serve una volta sola (~1 minuto). Su Linux richiede gli header di sviluppo di
# X11 e OpenGL: sudo apt install libx11-dev libxrandr-dev libxinerama-dev \
#                                 libxcursor-dev libxi-dev libgl1-mesa-dev
raylib: $(RAYLIB_STATIC)

$(RAYLIB_STATIC):
	@test -f $(RAYLIB_SRC)/raylib.h || { \
	  echo "vendor/raylib e' vuoto: git submodule update --init vendor/raylib"; \
	  exit 1; }
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC

raylib-clean:
	-$(MAKE) -C $(RAYLIB_SRC) clean

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe
