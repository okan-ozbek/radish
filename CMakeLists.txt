cmake_minimum_required(VERSION 4.0)
project(radish)

set(CMAKE_CXX_STANDARD 23)

add_executable(
    radish src/main.cpp
    include/Recorder.h
    include/Replayer.h
    include/Radish.h
        include/helpers/Operation.h
        include/Timer.h
        include/helpers/Types.h
)
