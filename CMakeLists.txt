cmake_minimum_required(VERSION 3.3)
project(assignment1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG -lboost_serialization -g -Wall -std=gnu++98")

include_directories(include)
file(GLOB_RECURSE INCLUDE_DIRECTORIES "include/*.h")
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

find_package(Boost)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

add_executable (assignment1 ${INCLUDE_DIRECTORIES} ${SOURCE_FILES})