CXXFLAGS += -std=c++14
LDLIBS = -lsfml-window -lsfml-system -lsfml-graphics -lGL
OUT = bin/chip
VPATH = ../src
OBJECTS = $(patsubst ../src/%.cpp, %.o, $(wildcard ../src/*cpp))

$(OUT): $(OBJECTS)
	mkdir -p $(@D) && \
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

clean: 
	rm -rf $(OBJECTS)
