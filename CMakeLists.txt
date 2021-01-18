cmake_minimum_required(VERSION 3.10.2)

project("Antik Examples" VERSION 0.1.0 DESCRIPTION "Antik C++ Library Example Programs")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra")

# Build Antik library

add_subdirectory(antik)

# Get example program list

file( GLOB EXAMPLE_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" )

# Build each program

foreach( EXAMPLE_PROGRAM ${EXAMPLE_SOURCES} )
    string( REPLACE ".cpp" "" EXAMPLE_TARGET ${EXAMPLE_PROGRAM} )
    add_executable( ${EXAMPLE_TARGET} ${EXAMPLE_PROGRAM} )
    target_link_libraries( ${EXAMPLE_TARGET} antik )
    install(TARGETS ${EXAMPLE_TARGET} DESTINATION bin)
endforeach( EXAMPLE_PROGRAM ${EXAMPLE_SOURCES} )



