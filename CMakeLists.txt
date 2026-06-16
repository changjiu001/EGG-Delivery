cmake_minimum_required(VERSION 3.10)
project(EGG_Delivery)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module 3 — 文件与存储管理
add_library(file_manager STATIC
    file_manager.cpp
    file_manager.h
)
target_include_directories(file_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 如果编译器未内置 std::filesystem（GCC < 9），取消下面这行的注释：
# target_link_libraries(file_manager PRIVATE stdc++fs)

# 示例程序
add_executable(example example.cpp)
target_link_libraries(example PRIVATE file_manager)

# 单元测试：避免将 target 直接命名为 CMake 保留名称 test
add_executable(file_manager_tests test.cpp)
target_link_libraries(file_manager_tests PRIVATE file_manager)

# Qt GUI 默认关闭，因此原有命令行构建不依赖 Qt。
option(BUILD_QT_GUI "Build the Qt Widgets GUI" OFF)
if(BUILD_QT_GUI)
    add_subdirectory(qt_gui)
endif()

if(MSVC)
    target_compile_options(file_manager PRIVATE /W4)
    target_compile_options(example PRIVATE /W4)
    target_compile_options(file_manager_tests PRIVATE /W4)
else()
    target_compile_options(file_manager PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(file_manager_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME FileManagerTests COMMAND file_manager_tests)
