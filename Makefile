# curve2dmap is to map curved screen image into the projector
# 4/15/2016 by Yang Yu (yuy@janelia.hhmi.org)

UNAME := $(shell uname)

CXX := g++
CXXFLAGS := -Wall -std=c++11 -g -I/usr/local/include

ifeq ($(UNAME), Linux)
LDFLAGS := -L/usr/local/lib -lOpenGL -lGLEW -lglfw
endif
ifeq ($(UNAME), Darwin)
LDFLAGS := -L/usr/local/lib -framework OpenGL -lGLEW -lglfw -lglbinding
endif

TARGET := $(shell basename $(PWD))
OBJECTS := $(patsubst %.cc,%.o,$(wildcard *.cc))

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $<

all: $(TARGET)

$(TARGET):  $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
