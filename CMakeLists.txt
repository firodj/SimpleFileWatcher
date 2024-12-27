cmake_minimum_required(VERSION 3.3)
project(lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -D_REENTRANT -DLINUX")

set(SRCS
    source/FileWatcher.cpp
)

if(APPLE)
    list(APPEND SRCS "source/FileWatcherOSX.cpp")
elseif(WIN32)
    list(APPEND SRCS "source/FileWatcherWin32.cpp")
else()
    list(APPEND SRCS "source/FileWatcherLinux.cpp")
endif()

include_directories(
    .
    include/
)

add_library(SimpleFileWatcher ${SRCS})

add_executable(SimpleDemo SimpleDemo.cpp)
target_link_libraries(SimpleDemo SimpleFileWatcher)