# Compiler and Flags
CXX = g++

# Source files and object files
SRC = main.cpp eval.cpp search.cpp
OBJ = $(SRC:.cpp=.o)

# Output file
EXEC = chess_engine

# Default target: build the project
all: $(EXEC)

# Linking the object files into the executable
$(EXEC): $(OBJ)
	$(CXX) $(LDFLAGS) -o $(EXEC) $(OBJ)

# Rule to build object files from source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -DNDEBUG -std=c++17 -c $< -o $@

# Clean up the compiled files
clean:
	rm -f $(OBJ) $(EXEC)

# To run the program after compilation
run: $(EXEC)
	./$(EXEC)

# Rebuild the project
rebuild: clean all

