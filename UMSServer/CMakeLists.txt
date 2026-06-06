cmake_minimum_required(VERSION 3.16)
project(UMSServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

set(BOOST_ROOT "/usr/include/boost")
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS filesystem system)

# Source files
set(UMS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UMSGrpcServiceImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MailerGrpcClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RedisConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RedisManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLDao.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MySQLManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RPCConnectPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StatusGrpcClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KafkaProducer.cpp
)

# Protobuf
find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

# Hiredis
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

# gRPC
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
message(STATUS "Using gRPC ${GRPC_VERSION}")

# MySQL connect
find_path(MYSQLCPPCONN_INCLUDE_DIR NAMES mysql_driver.h PATHS /usr/include /usr/local/include)
find_library(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${HIREDIS_INCLUDE_DIR})
include_directories(${MYSQLCPPCONN_INCLUDE_DIR})

# Proto generation
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/message.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_SOURCE_DIR}/message.pb.h")
set(GRPC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/message.grpc.pb.cc")
set(GRPC_HDR "${CMAKE_CURRENT_SOURCE_DIR}/message.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
    COMMAND protobuf::protoc
    ARGS --grpc_out "${CMAKE_CURRENT_SOURCE_DIR}"
         --cpp_out "${CMAKE_CURRENT_SOURCE_DIR}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_PREFIX}/bin/grpc_cpp_plugin
         "${PROTO_FILE}"
    DEPENDS ${PROTO_FILE}
)

set(SOURCES ${UMS_SOURCES} ${PROTO_SRC} ${GRPC_SRC})
add_executable(${PROJECT_NAME} ${SOURCES})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
)

target_link_libraries(${PROJECT_NAME}
    ${Boost_LIBRARIES}
    ${GRPC_LIBRARIES}
    protobuf::libprotobuf
    ${HIREDIS_LIBRARY}
    ${MYSQLCPPCONN_LIBRARY}
    rdkafka
)
