# curve2dmap is to map curved screen image into the projector
# 4/15/2016 by Yang Yu (yuy@janelia.hhmi.org)

CXX := g++
CXXFLAGS := -Wall -g -I/usr/local/include
LDFLAGS := -L/usr/local/lib -framework OpenGL -lGLEW -lglfw
TARGET := $(shell basename $(PWD))
OBJECTS := $(patsubst %.cc,%.o,$(wildcard *.cc))

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

all: $(TARGET)

$(TARGET):  $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
