# Compiler and Flags
CXX = g++
CXXFLAGS = -march=native -mtune=native -msse4.2 -std=c++17 $(FLAGS)

# Source files and object files
SRC = main.cpp eval.cpp search.cpp pieces.cpp pawns.cpp patterns.cpp
OBJ = $(SRC:.cpp=.o)

# Output file
EXEC = chess_engine

# Default target: build the project
all: $(EXEC)

# Linking the object files into the executable
$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(OBJ)

# Rule to build object files from source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the compiled files
clean:
	rm -f $(OBJ) $(EXEC)

# To run the program after compilation
run: $(EXEC)
	./$(EXEC)

# Rebuild the project
rebuild: clean all

