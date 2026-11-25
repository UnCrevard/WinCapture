TARGET   = WinCapture.exe

OBJ_DIR  = obj
SRCS     := $(wildcard C/*.c)
RC_SRCS  := $(wildcard C/*.rc)
OBJS     := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS))) \
            $(patsubst %.rc,$(OBJ_DIR)/%.res.o,$(notdir $(RC_SRCS)))

ifeq ($(OS),Windows_NT)
  ifeq ($(origin CC),default)
    CC := gcc
  endif
  WINDRES ?= windres
  MKDIR_OBJ = if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
  RM_OBJ    = if exist "$(OBJ_DIR)" rmdir /s /q "$(OBJ_DIR)"
  RM_BIN    = if exist "$(TARGET)" del /f /q "$(TARGET)"
  RUN_CMD   = $(TARGET)
else
  ifeq ($(origin CC),default)
    CC := x86_64-w64-mingw32-gcc
  endif
  WINDRES ?= x86_64-w64-mingw32-windres
  WINE     ?= wine
  MKDIR_OBJ = mkdir -p "$(OBJ_DIR)"
  RM_OBJ    = rm -rf "$(OBJ_DIR)"
  RM_BIN    = rm -f "$(TARGET)"
  RUN_CMD   = $(WINE) ./$(TARGET)
endif

ifeq ($(strip $(SRCS)),)
  $(error Aucun fichier source .c trouve dans C/, src/ ou la racine)
endif

vpath %.c C src .
vpath %.rc C src .

CFLAGS   = -O2 -Wall -Wextra -Iinclude -IC -Isrc
LDFLAGS  = -s -municode -mwindows
LDLIBS   = -lmfplat -lmfreadwrite -lmfuuid -lwindowscodecs \
           -lole32 -loleaut32 -luuid -lshlwapi -lshell32 \
           -lgdi32 -luser32 -lwinmm -ldwmapi -lcomdlg32 \
           -lcomctl32 -ldxgi -ld3d11

.PHONY: all clean fclean re run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c $(wildcard C/wincapture.h src/wincapture.h) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.res.o: %.rc $(wildcard C/resource.h C/app.manifest C/*.ico C/*.png) | $(OBJ_DIR)
	$(WINDRES) -I C -I src -I . -O coff $< -o $@

$(OBJ_DIR):
	$(MKDIR_OBJ)

clean:
	$(RM_OBJ)

fclean: clean
	$(RM_BIN)

re: fclean all

run: $(TARGET)
	$(RUN_CMD)
