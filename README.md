# cppchess_engine

A minimal chess engine (not UCI, not XBoard, but only search and evaluation functions)

**Source**: [Disservin/chess-library](https://github.com/Disservin/chess-library)

---

After watching ["BEST C++ CODE ever written" // Code Review](https://www.youtube.com/watch?v=NeHjMNBsVfs) and exploring [NicholasMaurer2005/ChessConsole](https://github.com/NicholasMaurer2005/ChessConsole), I noticed several issues that should be addressed:

---

### Problems in the Code:

1. **Displacement of methods:**
   - [Engine::makeMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L478)
   - [Engine::printBoard](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L376)
   - [Engine::inputAndParseMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L394)
   - [Engine::squareToIndex](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L470)
   - [Engine::kingInCheck](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L346)

2. **Use of `std::endl`:**
   - [Link to std::endl](https://en.cppreference.com/w/cpp/io/manip/endl)

3. **Use of `rand()`:**
   - [How good is the current implementations of rand() in C?](https://scicomp.stackexchange.com/questions/30479/how-good-are-current-implementations-of-rand-in-c)
   - [Faster than rand()?](https://stackoverflow.com/questions/26237419/faster-than-rand)

4. **`using namespace std;` in header.**

5. **Optimize branch prediction:**
   - [MoveList::FindMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/MoveList.cpp#L35)
   - [MoveList::FindCastleMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/MoveList.cpp#L35)
