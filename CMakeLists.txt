cmake_minimum_required(VERSION 4.0)
project(radish)

set(CMAKE_CXX_STANDARD 23)

add_executable(
    radish src/main.cpp
    include/RadishStore.h
    include/helpers/Operation.h
    include/helpers/Types.h
    include/helpers/BinaryFile.h
    include/helpers/Concepts.h
    include/file/PersistenceLog.h
    include/file/Serializable.h
    include/file/RadishEvent.h
    include/file/CompactStrategy.h
)
