CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude $(shell pkg-config --cflags libavformat libavcodec libavutil libswscale)
LDFLAGS := $(shell pkg-config --libs libavformat libavcodec libavutil libswscale)

TARGET := loopfetch
SRC := main.cpp core.cpp
OBJ := $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

install:
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: all clean
