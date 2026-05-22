cmake_minimum_required(VERSION 4.0)
project(radish)

set(CMAKE_CXX_STANDARD 23)

add_executable(
    radish src/main.cpp
    include/RadishStore.h
    include/RadishDB.h
    include/RadishEvent.h
    include/enums/EventType.h
    include/helpers/Types.h
    include/helpers/Concepts.h
    include/persistence/BinaryFile.h
    include/persistence/PersistenceLog.h
    include/persistence/Serializable.h
    include/persistence/CompactStrategy.h
    include/persistence/LogWriter.h
    include/persistence/LogReader.h
)
