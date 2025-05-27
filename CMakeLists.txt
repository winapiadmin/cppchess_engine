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

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

# Platform-specific threading configurations
if(WIN32)
    if(MSVC)
        # Windows with MSVC - uses Windows threading API
        target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
    elseif(MINGW)
        # MinGW on Windows - uses pthread
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
        # MinGW might need explicit pthread linking
        if(CMAKE_THREAD_LIBS_INIT)
            target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
        endif()
    endif()
elseif(UNIX)
    # Unix/Linux systems - uses pthread
    if(CMAKE_THREAD_LIBS_INIT)
        target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
    endif()
    # Some older systems might need explicit -pthread flag
    if(CMAKE_USE_PTHREADS_INIT)
        target_compile_options(${PROJECT_NAME} PRIVATE -pthread)
        target_link_options(${PROJECT_NAME} PRIVATE -pthread)
    endif()
endif()

# Optional: Add atomic library support for older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    target_link_libraries(${PROJECT_NAME} atomic)
endif()
