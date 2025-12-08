cmake_minimum_required(VERSION 2.8)

project(ChatServer)

#设置c++标椎 允许使用c++11 标椎
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#添加可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/chatServer.cpp
)

#添加头文件路径
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/header
)

#添加线程支持库
target_link_libraries(ChatServer pthread)