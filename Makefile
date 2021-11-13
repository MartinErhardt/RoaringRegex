SRCS = $(shell find -wholename './src/*.cpp')
OBJS = $(addsuffix .o,$(basename $(SRCS)))

CXX = g++

CXXFLAGS = -std=c++17 -Wall -Werror -oFast
all: | CRoaring fast_regex
fast_regex:  $(OBJS)
	$(CXX) -o $@ $^
CRoaring:
	git clone https://github.com/RoaringBitmap/CRoaring.git
	cd CRoaring && ./amalgamation.sh
bin/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^
clean:
	rm $(OBJS)
.PHONY: clean doc
