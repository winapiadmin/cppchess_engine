cmake_minimum_required(VERSION 3.10)
project(cppchess_engine)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
set(BUILD_TESTING OFF)
FetchContent_Declare(
    chesslib
    GIT_REPOSITORY https://github.com/winapiadmin/chesslib.git
    GIT_TAG main
)
FetchContent_Declare(
    tbprobe
    GIT_REPOSITORY https://github.com/winapiadmin/tb_probing_tool.git
    GIT_TAG main
)
set(BUILD_GAVIOTA OFF CACHE BOOL "Enable Gaviota TB probing" FORCE)
FetchContent_MakeAvailable(chesslib tbprobe)
add_executable(engine "main.cpp" "timeman.cpp" "timeman.h" "eval.h" "eval.cpp" "tune.h" "ucioption.h" "tune.cpp" "ucioption.cpp" "tt.h" "tt.cpp" "uci.cpp" "uci.h" "search.h" "search.cpp" "score.h" "score.cpp" "movepick.h" "movepick.cpp" "tb.h" "tb.cpp" "Weights.h" "tune_cmd.h" "tune_cmd.cpp")
target_link_libraries(engine PRIVATE chesslib syzygy_probe)
target_include_directories(engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${chesslib_SOURCE_DIR})
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    set(BUILD_VERSION "debug-${GIT_SHA}")
elseif(GIT_TAG)
    set(BUILD_VERSION "${GIT_TAG}")
else()
    set(BUILD_VERSION "release-${GIT_SHA}")
endif()

message(STATUS "Build Version: ${BUILD_VERSION}")
target_compile_definitions(engine PRIVATE BUILD_VERSION="${BUILD_VERSION}")
option(ENGINE_TUNING "enable Texel tuning (requires csv-parser)" OFF)
if (ENGINE_TUNING)
    FetchContent_Declare(
        csv
        GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(csv)
    target_link_libraries(engine PRIVATE csv)
    target_compile_definitions(engine PRIVATE USE_CSV_PARSER)
endif()
