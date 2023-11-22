cmake_minimum_required(VERSION 3.10)
project(frame)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(test_app frame.cpp text.cpp video.cpp)

target_link_libraries(test_app ${OpenCV_LIBS})
