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
    include(FetchContent)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(
        radish_tests
        tests/RadishDBTests.cpp
        tests/RadishEventTests.cpp
        tests/RadishStoreTests.cpp
        tests/PersistenceLogTests.cpp
    )
    target_link_libraries(radish_tests PRIVATE radish_headers Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(radish_tests)

    add_custom_target(
        app-test
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
        DEPENDS radish_tests
        USES_TERMINAL
    )

    add_custom_target(
        app-test-verbose
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --verbose
        DEPENDS radish_tests
        USES_TERMINAL
    )
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
