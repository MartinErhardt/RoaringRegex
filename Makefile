SRCS = $(shell find -wholename './src/*.cc' -not -path './src/test/*')
SRCS += $(shell find -wholename './src/*.cpp' -not -path './src/test/*')
OBJS = $(addprefix ./bin/,$(addsuffix .o,$(basename $(SRCS))))

CXX = g++
CXXFLAGS = -std=c++17 -I src/inc -flto -oFast -mavx2 -Wall -Werror
#LIBLTO=$(shell locate liblto_plugin.so | tail -1)
all: | bin_dirs CRoaring librregex.a test_regex
bin_dirs:
	mkdir -p bin
	mkdir -p bin/src
bin/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^
bin/%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^
librregex.a:  $(OBJS)
	$(AR) rcsT  $@ $^
test_regex: librregex.a
	$(CXX) $(CXXFLAGS) -I src/inc -c -o ./bin/main.o ./src/test/main.cpp
	$(CXX) -o $@ -flto ./bin/main.o librregex.a
CRoaring:
	git clone https://github.com/RoaringBitmap/CRoaring.git
	cd CRoaring && ./amalgamation.sh
clean:
	rm -r bin/*
.PHONY: clean doc
