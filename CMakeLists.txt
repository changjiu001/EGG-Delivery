cmake_minimum_required(VERSION 3.16)
project(EGG_Delivery LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_GUI "Build the Qt Widgets GUI" ON)

# Module 3 - file and storage management
add_library(file_manager STATIC
    file_manager.cpp
    file_manager.h
)
target_include_directories(file_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# If your compiler does not provide std::filesystem in libstdc++ automatically
# (for example GCC < 9), enable the line below.
# target_link_libraries(file_manager PRIVATE stdc++fs)

# Example program
add_executable(example example.cpp)
target_link_libraries(example PRIVATE file_manager)

# Unit tests
add_executable(test test.cpp)
target_link_libraries(test PRIVATE file_manager)

if(BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)

    if(QT_FOUND)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

        add_executable(egg_delivery_gui
            modules/ui/main.cpp
            modules/ui/main_window.cpp
            modules/ui/main_window.h
            modules/ui/file_transfer_controller.cpp
            modules/ui/file_transfer_controller.h
        )

        target_include_directories(egg_delivery_gui PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/modules/ui
        )

        target_link_libraries(egg_delivery_gui PRIVATE
            file_manager
            Qt${QT_VERSION_MAJOR}::Widgets
        )

        if(MSVC)
            target_compile_options(egg_delivery_gui PRIVATE /W4 /utf-8)
        else()
            target_compile_options(egg_delivery_gui PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    else()
        message(WARNING "Qt Widgets was not found, so egg_delivery_gui will not be built. Install Qt or configure with -DBUILD_GUI=OFF.")
    endif()
endif()

if(MSVC)
    target_compile_options(file_manager PRIVATE /W4)
    target_compile_options(example PRIVATE /W4)
    target_compile_options(test PRIVATE /W4)
else()
    target_compile_options(file_manager PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME FileManagerTests COMMAND test)
