# ============================================================================
#  Frostmark - Makefile
#
#   make            build ottimizzata      -> ./frostmark
#   make debug      con simboli e -O0
#   make run        compila ed esegue
#   make baker      lo strumento che cuoce il mondo -> ./baker
#   make mondo      cuoce assets/world/ se non c'e' gia'
#   make mondo-forza  ricuoce cancellando le modifiche fatte a mano
#   make verifica-mondo  confronta il mondo cotto con quello generato
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
TOOL_DIR := tools
BUILD_DIR:= build
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# ---- baker -----------------------------------------------------------------
# Cuoce assets/world/ dal seme. Compila il rumore procedurale (tools/noise.c) e
# la generazione (tools/worldgen.c), che il GIOCO non contiene piu': dopo la
# fase 3 del piano (docs/05) il mondo e' un dato, non una funzione del seme.
# Del gioco riusa il caricatore, cosi' '--verifica' confronta il mondo cotto con
# quello generato usando esattamente il codice che poi ci gioca.
BAKER      := baker
BAKER_SRCS := $(TOOL_DIR)/baker.c $(TOOL_DIR)/worldgen.c $(TOOL_DIR)/noise.c \
              $(SRC_DIR)/worldio.c $(SRC_DIR)/dataparse.c $(SRC_DIR)/fmath.c
BAKER_OBJS := $(patsubst %.c,$(BUILD_DIR)/baker-%.o,$(notdir $(BAKER_SRCS)))

# -MMD -MP: genera i file .d con le dipendenze dagli header, cosi' modificare
# config.h o player.h ricompila quello che serve invece di lasciare oggetti
# vecchi in build/.
CFLAGS_COMMON := -std=c99 -Wall -Wextra -Wno-unused-result \
                 -I$(SRC_DIR) -I$(TOOL_DIR) -MMD -MP
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
  RAYLIB_INC    := -I$(RAYLIB_SRC)
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

.PHONY: all debug run clean dirs raylib raylib-clean mondo mondo-forza \
        verifica-mondo valida prove

# Sotto WSL si gioca con l'eseguibile Windows - li' il mouse funziona, vedi
# src/rawmouse.h - quindi "make" costruisce anche quello: altrimenti si
# ricompila il binario Linux, si avvia il .exe di ieri e si guarda una
# modifica che non c'e'. Ci vogliono pochi secondi in piu'.
ifneq ($(wildcard /dev/dxg),)
ifneq ($(WSL_DISTRO_NAME)$(WSL_INTEROP),)
  ALSO_WINDOWS := windows
endif
endif

all: dirs $(TARGET) $(ALSO_WINDOWS)

debug: CFLAGS := -O0 -g -DDEBUG
debug: clean all

dirs:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS_COMMON) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(RAYLIB_DEP)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "==> $(TARGET) pronto. Avvia con: ./$(TARGET)"

run: all
	./$(TARGET)

valida: all
	./$(TARGET) --valida

# ---- prove -----------------------------------------------------------------
# Non c'e' un framework: ogni prova e' un eseguibile che stampa una riga per
# controllo. Includono il .c che provano, perche' cio' che vale la pena provare
# e' quasi sempre 'static' - quindi ognuna ha la sua riga di collegamento, e i
# moduli che include NON vanno anche collegati o si duplicano i simboli.
#
# Vanno lanciate dalla radice del repo: caricano gli asset per percorso
# relativo. Chi esce 77 non ha trovato un contesto OpenGL e viene contata come
# saltata, non fallita.
PROVE_DIR := $(BUILD_DIR)/prove
PROVE_CF  := -std=gnu99 -Wall -Wextra -I$(TOOL_DIR)/prove -I$(SRC_DIR) \
             -I$(TOOL_DIR) $(RAYLIB_INC) -O0

prove: $(RAYLIB_DEP)
	@mkdir -p $(PROVE_DIR)
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/scale.c \
	      $(SRC_DIR)/fmath.c $(SRC_DIR)/light.c $(SRC_DIR)/worldio.c \
	      $(SRC_DIR)/dataparse.c $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/scale
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/normalmap.c \
	      $(SRC_DIR)/fmath.c $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/normalmap
	$(CC) $(PROVE_CF) $(TOOL_DIR)/prove/luce.c \
	      $(SRC_DIR)/fmath.c $(LDFLAGS) $(LDLIBS) -o $(PROVE_DIR)/luce
	@ok=1; for t in $(PROVE_DIR)/*; do \
	    echo "== $$t"; \
	    $$t; r=$$?; \
	    if [ $$r = 77 ]; then echo "   (saltata)"; \
	    elif [ $$r != 0 ]; then ok=0; fi; \
	  done; \
	  [ $$ok = 1 ] || { echo "==> PROVE FALLITE"; exit 1; }
	@echo "==> prove passate"

# ---- baker e mondo cotto ---------------------------------------------------
# 'baker' e' il file, non un target finto: 'make baker' lo costruisce.

# --- Eseguibile Windows nativo -------------------------------------------
# Serve perche' sotto WSLg il puntatore e' un dispositivo assoluto e non
# esiste movimento relativo del mouse (vedi src/rawmouse.h): la visuale non e'
# governabile, e non e' un difetto del gioco. Su Windows il mouse lo gestisce
# Win32 e la GPU e' diretta. Si avvia anche da qui:  ./frostmark.exe
WIN_CC := x86_64-w64-mingw32-gcc
WIN_AR := x86_64-w64-mingw32-ar
WIN_RL := $(BUILD_DIR)/win/libraylib.a

.PHONY: windows
windows: $(WIN_RL)
	$(WIN_CC) -std=c99 -O2 -I$(SRC_DIR) -I$(RAYLIB_SRC) $(SRCS) $(WIN_RL) \
	    -o frostmark.exe -lopengl32 -lgdi32 -lwinmm -lm -static
	@echo "==> frostmark.exe pronto. Avvialo da qui: ./frostmark.exe"

$(WIN_RL):
	@mkdir -p $(BUILD_DIR)/win
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP OS=Windows_NT \
	    CC=$(WIN_CC) AR=$(WIN_AR) RAYLIB_LIBTYPE=STATIC \
	    RAYLIB_RELEASE_PATH=$(CURDIR)/$(BUILD_DIR)/win
	@# gli oggetti Windows vivono nella stessa cartella di quelli Linux:
	@# lasciarli li' farebbe linkare architetture miste al prossimo make.
	rm -f $(RAYLIB_SRC)/*.o

$(BUILD_DIR)/baker-%.o: $(TOOL_DIR)/%.c | dirs
	$(CC) $(CFLAGS_COMMON) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/baker-%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS_COMMON) $(CFLAGS) -c $< -o $@

$(BAKER): $(BAKER_OBJS) $(RAYLIB_DEP) | dirs
	$(CC) $(BAKER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "==> $(BAKER) pronto. Cuoci il mondo con: make mondo"

# Cuoce solo se non c'e': il mondo cotto e' versionato nel repository, e
# ricuocerlo cancella le modifiche fatte a mano.
mondo: $(BAKER)
	./$(BAKER)

mondo-forza: $(BAKER)
	./$(BAKER) --forza

verifica-mondo: $(BAKER)
	./$(BAKER) --verifica

-include $(OBJS:.o=.d)
-include $(BAKER_OBJS:.o=.d)

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
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe $(BAKER) $(BAKER).exe
