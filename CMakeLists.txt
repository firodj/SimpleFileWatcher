cmake_minimum_required(VERSION 3.3)
project(lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -D_REENTRANT -DLINUX")

set(SOURCE_FILES
    source/FileWatcher.cpp
    source/FileWatcherLinux.cpp
)

include_directories(
    .
    include/
)

add_library(SimpleFileWatcher ${SOURCE_FILES})

LINK_DIRECTORIES(/usr/lib/x86_64-linux-gnu/)

# TARGET_LINK_LIBRARIES(main stdc++fs glfw GLEW GLU GL pulse-simple pulse pthread libs/ffts/libffts.a ${CMAKE_SOURCE_DIR}/libs/SimpleFileWatcher/lib/Debug/libSimpleFileWatcher.a)

