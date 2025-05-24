cmake_minimum_required(VERSION 3.10)
project(chess_engine)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: enable warnings (MSVC specific flags)
if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Add the source files
set(SOURCES
    main.cpp
    eval.cpp
    search.cpp
    tt.cpp
    move_ordering.cpp
)

# Define the executable
add_executable(${PROJECT_NAME} ${SOURCES})
