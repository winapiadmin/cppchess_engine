# Compiler and Flags
CXX = g++
<<<<<<< HEAD

# Source files and object files
SRC = main.cpp eval.cpp search.cpp
=======
CXXFLAGS = -march=native -mtune=native -msse4.2 -std=c++17 $(FLAGS) -DNDEBUG

# Source files and object files
SRC = main.cpp eval.cpp search.cpp pieces.cpp pawns.cpp patterns.cpp
>>>>>>> 08822de3005f1e1458cbbb02037d1f714fcd46e7
OBJ = $(SRC:.cpp=.o)

# Output file
EXEC = chess_engine

# Default target: build the project
all: $(EXEC)

# Linking the object files into the executable
$(EXEC): $(OBJ)
<<<<<<< HEAD
	$(CXX) $(CXXFLAGS) -std=c++17 -o $(EXEC) $(OBJ)

# Rule to build object files from source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -std=c++17 -c $< -o $@
=======
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(OBJ)

# Rule to build object files from source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
>>>>>>> 08822de3005f1e1458cbbb02037d1f714fcd46e7

# Clean up the compiled files
clean:
	rm -f $(OBJ) $(EXEC)

# To run the program after compilation
run: $(EXEC)
	./$(EXEC)

# Rebuild the project
rebuild: clean all

