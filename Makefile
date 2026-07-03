# :( openbench and fishtest require make
TARGET = engine

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra
OPTFLAGS ?= -O3
ifeq ($(LTO), yes)
    OPTFLAGS += -flto
endif
ifeq ($(debug),no)
    CXXFLAGS += -DNDEBUG
endif
ARCH ?= native

ifeq ($(ARCH), native)
    CXXFLAGS += -march=native -mtune=native
endif
ifeq ($(ARCH), avx2)
    CXXFLAGS += -march=haswell -mavx2 -mbmi -mbmi2 -msse4.1
endif
ifeq ($(ARCH), bmi2)
    CXXFLAGS += -mbmi2
endif
ifeq ($(ARCH), sse41)
    CXXFLAGS += -msse4.1
endif
ifeq ($(ARCH), x86-64)
    CXXFLAGS += -march=x86-64
endif
deps:
	test -d deps/chesslib || git clone https://github.com/winapiadmin/chesslib deps/chesslib
	test -d deps/tbprobe || git clone https://github.com/winapiadmin/tb_probing_tool deps/tbprobe
SRCS = $(wildcard *.cpp) $(wildcard deps/chesslib/*.cpp) $(wildcard deps/tbprobe/*.cpp)
OBJS = $(SRCS:.cpp=.o)

all: deps $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -Ideps/chesslib -Ideps/tbprobe/syzygy -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
