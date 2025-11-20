# Compiler and flags
CC = gcc
CFLAGS = -Iinclude -IC:/msys64/mingw64/include -Wall -Wextra -std=c99 -pedantic
LDFLAGS = -LC:/msys64/mingw64/lib -lportaudio -lmpg123 -lvorbis -lvorbisfile -lFLAC -lole32 -lwinmm

# Auto-detect all source files in src and its subdirectories
SRC = $(wildcard src/*.c) $(wildcard src/**/*.c)
# Convert src/path/file.c to obj/path/file.o
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))
TARGET = MusicPlayer.exe

# Create obj directory structure
$(shell if not exist obj mkdir obj)

# Build
$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

# Pattern rule for object files in obj directory
obj/%.o: src/%.c
	@if not exist $(@D) mkdir $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Generate compile_commands.json for LSP
compile_commands.json:
	@echo [ > $@
	@( \
		echo   { & \
		echo     "directory": "$(CURDIR)", & \
		echo     "command": "$(CC) $(CFLAGS) -c src/main.c -o obj/main.o", & \
		echo     "file": "src/main.c" & \
		echo   } \
	) >> $@
	@echo ] >> $@

# Run
run: $(TARGET)
	@echo "Starting $(TARGET)..."
	./$(TARGET)

# Clean
clean:
	if exist obj rmdir /S /Q obj
	del $(TARGET) 2>nul || exit 0

# Print detected sources (helpful for debugging)
print-sources:
	@echo "Detected source files:"
	@echo $(SRC)
	@echo "Object files will be:"
	@echo $(OBJ)

.PHONY: clean print-sources run
