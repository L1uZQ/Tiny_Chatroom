# 这是一个 CMakeLists.txt 文件
cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

include_directories("/usr/include/mysql")
link_directories("/usr/lib/x86_64-linux-gnu")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(../ProtocolHead)
find_package(jsoncpp REQUIRED)
file(GLOB SRC1_LIST ../ProtocolHead/*.cpp)
file(GLOB SRC2_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)


add_executable(main  ${SRC1_LIST} ${SRC2_LIST})

target_link_libraries(main PRIVATE mysqlclient jsoncpp_lib pthread)