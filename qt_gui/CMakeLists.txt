cmake_minimum_required(VERSION 3.16)
project(LanChatQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(EGG_DELIVERY_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# Opening qt_gui/CMakeLists.txt directly in Qt Creator also works. When this
# directory is built from the repository root, reuse the existing target.
if(NOT TARGET file_manager)
    add_library(file_manager STATIC
        "${EGG_DELIVERY_ROOT}/file_manager.cpp"
        "${EGG_DELIVERY_ROOT}/file_manager.h"
    )
    target_include_directories(file_manager PUBLIC "${EGG_DELIVERY_ROOT}")
endif()

add_executable(LanChatQt WIN32
    src/main.cpp
    src/app_types.h
    src/wire_protocol.h
    src/wire_protocol.cpp
    src/tcp_connection.h
    src/tcp_connection.cpp
    src/lan_network_client.h
    src/lan_network_client.cpp
    src/lan_network_client_part1.inc
    src/lan_network_client_part2.inc
    src/lan_network_client_part3.inc
    src/lan_network_client_part4.inc
    src/lan_network_client_part5.inc
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow_part1.inc
    src/mainwindow_part2.inc
    src/mainwindow_part3.inc
    src/mainwindow_part4.inc
    src/mainwindow_part5.inc
    src/sticker_catalog.h
    resources.qrc
)

target_include_directories(LanChatQt PRIVATE
    "${EGG_DELIVERY_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(LanChatQt PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    file_manager
)

if(MSVC)
    target_compile_options(LanChatQt PRIVATE /W4)
else()
    target_compile_options(LanChatQt PRIVATE -Wall -Wextra -Wpedantic)
endif()
