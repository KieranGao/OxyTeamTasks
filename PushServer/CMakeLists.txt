cmake_minimum_required(VERSION 3.16)
project(PushServer CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
# 设置Boost路径
set(BOOST_ROOT "/usr/include/boost")
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# 查找Boost，如果没有Boost库，CMake会报错
find_package(Boost REQUIRED COMPONENTS filesystem system)

# 设置jsoncpp路径
set(JSONCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../jsoncpp")
set(JSONCPP_INCLUDE_DIR "${JSONCPP_ROOT}/include")
set(JSONCPP_SOURCES
    ${JSONCPP_ROOT}/src/lib_json/json_reader.cpp
    ${JSONCPP_ROOT}/src/lib_json/json_writer.cpp
    ${JSONCPP_ROOT}/src/lib_json/json_value.cpp
)

# 设置Gateserver源文件
set(CHATSERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AsyncTaskPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PushServerMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IOContextPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RedisConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RedisManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLDao.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RPCConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StatusGrpcClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PushGrpcServiceImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LogicSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MainServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KafkaProducer.cpp
)

# 查找protobuf包
find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

# 查找hiredis库
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

# 查找gRPC包
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
message(STATUS "Using gRPC ${GRPC_VERSION}")

# 查找MySQLConnect库
find_path(MYSQLCPPCONN_INCLUDE_DIR NAMES mysql_driver.h PATHS /usr/include /usr/local/include)
find_library(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)


# 包含目录
include_directories(${Boost_INCLUDE_DIRS})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${JSONCPP_INCLUDE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${HIREDIS_INCLUDE_DIR})
include_directories(${MYSQLCPPCONN_INCLUDE_DIR})

# 设置proto文件路径
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/message.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_SOURCE_DIR}/message.pb.h")
set(GRPC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/message.grpc.pb.cc")
set(GRPC_HDR "${CMAKE_CURRENT_SOURCE_DIR}/message.grpc.pb.h")

# 生成protobuf和grpc文件
add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
    COMMAND protobuf::protoc
    ARGS --grpc_out "${CMAKE_CURRENT_SOURCE_DIR}"
         --cpp_out "${CMAKE_CURRENT_SOURCE_DIR}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_PREFIX}/bin/grpc_cpp_plugin
         "${PROTO_FILE}"
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf and gRPC files from ${PROTO_FILE}"
)

# 源文件
set(SOURCES
    ${CHATSERVER_SOURCES}
    ${JSONCPP_SOURCES}
    ${PROTO_SRC}
    ${GRPC_SRC}
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
    COMMENT "Copying config.ini to build directory"
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    ${Boost_LIBRARIES}
    ${GRPC_LIBRARIES}
    protobuf::libprotobuf
    ${HIREDIS_LIBRARY}
    ${MYSQLCPPCONN_LIBRARY}
    rdkafka
)