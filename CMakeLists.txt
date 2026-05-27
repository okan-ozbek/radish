cmake_minimum_required(VERSION 4.0)
project(radish)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-0
)
FetchContent_MakeAvailable(asio)

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
    include/persistence/CompactStrategy.h
    include/persistence/LogWriter.h
    include/persistence/LogReader.h
)

target_include_directories(radish PRIVATE ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(radish PRIVATE ws2_32)