cmake_minimum_required(VERSION 3.25)
project(radish)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(radish_headers INTERFACE)
target_include_directories(radish_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(radish src/main.cpp)
target_link_libraries(radish PRIVATE radish_headers)

include(CTest)
if(BUILD_TESTING)
    add_executable(radish_tests tests/PersistenceTests.cpp)
    target_link_libraries(radish_tests PRIVATE radish_headers)
    add_test(NAME radish_tests COMMAND radish_tests)
endif()

set(RADISH_HEADERS
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
