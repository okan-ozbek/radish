cmake_minimum_required(VERSION 3.25)
project(radish)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
find_package(Threads REQUIRED)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_library(radish_headers INTERFACE)
target_include_directories(radish_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(
    radish_network
    src/network/RespCodec.cpp
    src/network/TcpServer.cpp
)
target_include_directories(radish_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(radish_network PUBLIC ASIO_STANDALONE)
target_link_libraries(radish_network PUBLIC radish_headers Threads::Threads)

add_executable(radish-server src/main.cpp)
target_link_libraries(radish-server PRIVATE radish_network)

add_executable(radish-cli src/cli_main.cpp)
target_link_libraries(radish-cli PRIVATE radish_network)

include(CTest)
if(BUILD_TESTING)
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
        tests/RespCodecTests.cpp
        tests/TcpServerTests.cpp
    )
    target_link_libraries(radish_tests PRIVATE radish_network Catch2::Catch2WithMain)

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
