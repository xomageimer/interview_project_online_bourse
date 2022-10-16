cmake_minimum_required(VERSION 3.21)
project(Bourse)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Boost COMPONENTS system thread REQUIRED)

find_package(OpenSSL REQUIRED)

find_package(PostgreSQL REQUIRED)

set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core Charts Widgets REQUIRED)

include_directories( ${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Charts_INCLUDE_DIRS})

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

add_compile_definitions(CONSOLE_LOGGER)

add_library(bourse_core SHARED src/bourse/request.h src/bourse/request.cpp src/bourse/response.h src/bourse/response.cpp)

add_library(database_manager SHARED src/bourse/database_manager.h src/bourse/database_manager.cpp)

target_link_libraries(database_manager PUBLIC ${PostgreSQL_LIBRARIES} pqxx helper)

target_link_libraries(bourse_core PUBLIC nlohmann_json::nlohmann_json database_manager helper)

add_library(server SHARED src/network/server.h src/network/server.cpp)

target_link_libraries(server PUBLIC
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        bourse_core
        helper
        )

if (WIN32)
    target_link_libraries(server PUBLIC-lws2_32 -lwsock32)
endif()

set(main_window src/gui/mainwindow.cpp src/gui/mainwindow.h)

qt5_wrap_ui(main_window ui_forms/mainwindow.ui)

set(auth_window src/gui/authwindow.h src/gui/authwindow.cpp)

qt5_wrap_ui(auth_window ui_forms/authwindow.ui)

set(input_window src/gui/inputwindow.h src/gui/inputwindow.cpp)

qt5_wrap_ui(input_window ui_forms/inputwindow.ui)

add_library(authwindow SHARED ${auth_window})

add_library(mainwindow SHARED ${main_window})

add_library(inputwindow SHARED ${input_window})

target_link_libraries(inputwindow PUBLIC Qt5::Core Qt5::Charts Qt5::Widgets)

target_link_libraries(authwindow PUBLIC Qt5::Core Qt5::Charts Qt5::Widgets)

target_link_libraries(mainwindow PUBLIC authwindow inputwindow Qt5::Core Qt5::Charts Qt5::Widgets)

add_library(client SHARED src/network/client.h src/network/client.cpp)

target_link_libraries(client PUBLIC
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        mainwindow
        bourse_core
        helper
        )

if (WIN32)
    target_link_libraries(client PUBLIC-lws2_32 -lwsock32)
endif()

add_executable(bourse_server src/main_server.cpp)

target_link_libraries(bourse_server PUBLIC server)

add_executable(bourse_client src/main_client.cpp)

target_link_libraries(bourse_client PUBLIC client)

set_target_properties(bourse_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/server)

set_target_properties(bourse_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/client)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dh2048.pem ${CMAKE_BINARY_DIR}/bin/server/dh2048.pem COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/server.key ${CMAKE_BINARY_DIR}/bin/server/server.key COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/server_certificate.crt ${CMAKE_BINARY_DIR}/bin/server/server_certificate.crt COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/server_certificate.crt ${CMAKE_BINARY_DIR}/bin/client/server_certificate.crt COPYONLY)

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