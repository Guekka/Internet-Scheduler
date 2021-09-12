cmake_minimum_required(VERSION 3.5)

project(InternetCut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(InternetCut
    main.cpp
    timer.hpp
)
target_link_libraries(InternetCut PRIVATE IPHLPAPI)
