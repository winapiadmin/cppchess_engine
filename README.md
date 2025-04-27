# cppchess_engine

A minimal chess engine (not UCI, not XBoard, but only search and evaluation functions)

Source: Disservin/chess-library

After seeing ["BEST C++ CODE ever written" // Code Review](https://www.youtube.com/watch?v=NeHjMNBsVfs) and [NicholasMaurer2005/ChessConsole](https://github.com/NicholasMaurer2005/ChessConsole) (the github page related) I see some major problems:
- Displacement of methods [1] [2] [3] [4] [5]
- Use `std::endl` [6]
- Use `rand()` [7] [8]
- `using namespace std;` in header (already covered)
- Optimize branch prediction [9] [10]
Citations:
- [1]: [Engine::makeMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L478)
- [2]: [Engine::printBoard](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L376)
- [3]: [Engine::inputAndParseMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L394)
- [4]: [Engine::squareToIndex](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L470)
- [5]: [Engine::kingInCheck](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/Engine.cpp#L346)
- [6]: [std::endl](https://en.cppreference.com/w/cpp/io/manip/endl)
- [7]: [How good is the current implementations of rand() in C?](https://scicomp.stackexchange.com/questions/30479/how-good-are-current-implementations-of-rand-in-c)
- [8]: [Faster than rand()?](https://stackoverflow.com/questions/26237419/faster-than-rand)
- [9]: [MoveList::FindMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/MoveList.cpp#L35)
- [10]: [MoveList::FindCastleMove](https://github.com/NicholasMaurer2005/ChessConsole/blob/master/ChessConsole/MoveList.cpp#L35)
