# Makefile for Darklands
# C++ project using SDL graphics library

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
CPPFLAGS = -I. -Ilibjgame

# SDL flags
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

# Source files
SOURCES = darklands.cpp Catalog.cpp PICImage.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = darklands

# Dependencies from libjgame
LIBJGAME_DIR = libjgame
LIBJGAME_LIB = $(LIBJGAME_DIR)/libjgame.a

# Final target
EXECUTABLE = $(TARGET)

.PHONY: all clean rebuild libjgame help

all: $(LIBJGAME_LIB) $(EXECUTABLE)

# Build the libjgame submodule
$(LIBJGAME_LIB):
	$(MAKE) -C $(LIBJGAME_DIR)

# Compile main executable
$(EXECUTABLE): $(OBJECTS) $(LIBJGAME_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL_LIBS) -L$(LIBJGAME_DIR)

# Compile object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
	$(MAKE) -C $(LIBJGAME_DIR) clean

# Rebuild from scratch
rebuild: clean all

# Help target
help:
	@echo "Darklands - Makefile targets:"
	@echo "  make all       - Build the project (default)"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make rebuild   - Clean and rebuild"
	@echo "  make help      - Show this help message"
