CXX = g++
CXXFLAGS = -std=c++11 -Wall
TARGET = kubsh.exe
SOURCES = main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^

clean:
	del $(OBJECTS) $(TARGET)

.PHONY: all clean