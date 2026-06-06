cmake_minimum_required(VERSION 3.10)
project(LAN_Chat_FileManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 文件管理库
add_library(file_manager STATIC
    file_manager.cpp
    file_manager.h
)

target_include_directories(file_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(file_manager PRIVATE stdc++fs)

# 示例程序
add_executable(example example.cpp)
target_link_libraries(example PRIVATE file_manager)

# 单元测试
add_executable(test test.cpp)
target_link_libraries(test PRIVATE file_manager)

# 编译选项
if(MSVC)
    target_compile_options(file_manager PRIVATE /W4)
    target_compile_options(example PRIVATE /W4)
    target_compile_options(test PRIVATE /W4)
else()
    target_compile_options(file_manager PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 启用测试
enable_testing()
add_test(NAME FileManagerTests COMMAND test)
