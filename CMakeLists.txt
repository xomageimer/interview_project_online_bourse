cmake_minimum_required(VERSION 3.21)
project(Bourse)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
ADD_DEFINITIONS(-DBoost_USE_STATIC_LIBS=ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# ****************************************************************
# FIND_PACKAGE(Boost 1.71.0 COMPONENTS system REQUIRED)
if (WIN32)
    set(BOOST_ROOT "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(BOOST_LIBRARYDIR "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(Boost_COMPILER mgw81)
endif ()
# ****************************************************************

find_package(Boost 1.71.0 COMPONENTS system thread REQUIRED)

set (OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")

find_package(OpenSSL REQUIRED)

find_package(PostgreSQL REQUIRED)

add_subdirectory(contrib/libpqxx build-pqxx)

add_subdirectory(contrib/json)

include_directories(${Boost_INCLUDE_DIR})

include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(${PostgreSQL_INCLUDE_DIRS})

include_directories(contrib/libpqxx/include)

include_directories(${CMAKE_BINARY_DIR}/build-pqxx/include)

include_directories(contrib/json/include)

include_directories(src)

include_directories(src/utils)

add_library(helper SHARED src/utils/logger.cpp src/utils/logger.h src/utils/auxiliary.h src/utils/auxiliary.cpp)

add_compile_definitions(FILE_LOGGER)

add_library(bourse_core SHARED src/bourse/request.h src/bourse/request.cpp src/bourse/response.h src/bourse/response.cpp)

add_library(database_manager SHARED src/bourse/database_manager.h src/bourse/database_manager.cpp)

target_link_libraries(database_manager PUBLIC ${PostgreSQL_LIBRARIES} pqxx helper)

target_link_libraries(bourse_core PUBLIC nlohmann_json::nlohmann_json database_manager helper)

add_library(server SHARED src/network/server.h src/network/server.cpp)

target_link_libraries(server PUBLIC
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        -lws2_32 -lwsock32
        bourse_core
        helper
        )

add_library(client SHARED src/network/client.h src/network/client.cpp)

target_link_libraries(client PUBLIC
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        -lws2_32 -lwsock32
        bourse_core
        helper
        )

add_executable(bourse_server src/main_server.cpp)

target_link_libraries(bourse_server PUBLIC server)

add_executable(bourse_client src/main_client.cpp)

target_link_libraries(bourse_client PUBLIC client)

add_subdirectory(
        "contrib/googletest"
        "googletest"
)

add_executable(tests test/tests.cpp)

target_link_libraries(tests PUBLIC server gtest gtest_main gmock)

include(CTest)

enable_testing()
add_test(
        test tests
)