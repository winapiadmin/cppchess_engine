# :( openbench and fishtest require make
TARGET = engine

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra
OPTFLAGS ?= -O3
ifeq ($(LTO), yes)
    OPTFLAGS += -flto
    ifeq ($(findstring clang++,$(CXX)),clang++)
        LDFLAGS += -fuse-ld=lld
    endif
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
# Tuning is not required on Makefile, use CMake.
CHESSLIB_SRCS := $(filter-out %tests.cpp,$(wildcard deps/chesslib/*.cpp))
SRCS := \
    $(filter-out tune_cmd.cpp,$(wildcard *.cpp)) \
    $(CHESSLIB_SRCS) \
    deps/tbprobe/syzygy/tbprobe.cpp
OBJS = $(SRCS:.cpp=.o)
SHA := $(shell git rev-parse --short HEAD 2>/dev/null)
TAG := $(shell git describe --tags --exact-match 2>/dev/null)
ifeq ($(debug),yes)
    BUILD_VERSION := debug-$(SHA)
else ifneq ($(TAG),)
    BUILD_VERSION := $(TAG)
else
    BUILD_VERSION := release-$(SHA)
endif

CXXFLAGS += -DBUILD_VERSION=\"$(BUILD_VERSION)\"
.PHONY: all clean deps
all: deps $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -Ideps/chesslib -Ideps/tbprobe/syzygy -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
