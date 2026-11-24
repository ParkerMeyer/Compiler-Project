# Parker Meyer - CSC-4510
# Parser Phase: Build the complete Lille scanner/parser project using GNU Make.

CXX = g++
CXXFLAGS = -std=c++20
TARGET = compiler.exe
SOURCES = compiler.cpp parser.cpp error_handler.cpp lille_exception.cpp scanner.cpp id_table.cpp symbol.cpp token.cpp lille_type.cpp lille_kind.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

# Headers are included to rebuild objects when parser declarations change.
%.o: %.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-cmd /C "del /Q /F $(OBJECTS) $(TARGET) 2>NUL"
else
	rm -f $(OBJECTS) $(TARGET)
endif
