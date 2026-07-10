EXE = engine

all:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build
	cp build/engine $(EXE)