cmake_minimum_required(VERSION 3.24)

project(midiPipe VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RTMIDI_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG ae843cb
)
FetchContent_MakeAvailable(rtmidi)

find_package(Curses REQUIRED)

add_executable(midiPipe
    src/main.cpp
    src/MidiRouter.cpp
    src/ConsoleUi.cpp
)

target_link_libraries(midiPipe PRIVATE rtmidi ${CURSES_LIBRARIES})
target_include_directories(midiPipe PRIVATE ${CURSES_INCLUDE_DIR})

target_include_directories(midiPipe PRIVATE ${rtmidi_SOURCE_DIR})
