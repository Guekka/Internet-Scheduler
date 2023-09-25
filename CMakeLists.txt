cmake_minimum_required(VERSION 3.5)

project(InternetCut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/timer.hpp
    src/network_adapter.hpp
)

if(WIN32)
    list(APPEND SOURCES src/network_adapter_window.hpp)
    list(APPEND SOURCES src/network_adapter_window.cpp)
endif()

add_executable(InternetCut ${SOURCES})

target_compile_features(InternetCut PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(InternetCut PRIVATE IPHLPAPI)
endif()
