SRCS = $(shell find -name '*.c')
OBJS = $(addsuffix .o,$(basename $(SRCS)))

CXX = gcc

CXXFLAGS = -std=c++17 -Wall -Werror -oFast

fast_regex: $(OBJS)
	$(CXX) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^
clean:
	rm $(OBJS)
.PHONY: clean doc
