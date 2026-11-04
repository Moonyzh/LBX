cmake_minimum_required(VERSION 3.14)
project(lab1_ride_microservices LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The course base image provides tRPC-Cpp and its code generators. Students
# build only this small application layer during the 90-minute lab.
set(TRPC_INSTALL_PATH "/usr/local/trpc-cpp/trpc" CACHE PATH "tRPC-Cpp install directory")
include(${TRPC_INSTALL_PATH}/cmake/config/trpc_config.cmake)
include(${TRPC_INSTALL_PATH}/cmake/tools/trpc_utils.cmake)
include_directories(${INCLUDE_INSTALL_PATHS})
link_directories(${LIBRARY_INSTALL_PATHS})

set(PB_PROTOC ${TRPC_INSTALL_PATH}/bin/protoc)
set(TRPC_CPP_PLUGIN ${TRPC_INSTALL_PATH}/bin/trpc_cpp_plugin)

set(PRICING_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/proto/pricing.proto)
set(TRIP_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/proto/trip.proto)
set(DHT_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/proto/dht.proto)
COMPILE_PROTO(PRICING_PB_SRCS "${PRICING_PROTO}" ${PB_PROTOC} ${CMAKE_CURRENT_SOURCE_DIR})
TRPC_COMPILE_PROTO(PRICING_TRPC_PB_SRCS "${PRICING_PROTO}" ${PB_PROTOC} ${TRPC_CPP_PLUGIN} ${CMAKE_CURRENT_SOURCE_DIR})
COMPILE_PROTO(TRIP_PB_SRCS "${TRIP_PROTO}" ${PB_PROTOC} ${CMAKE_CURRENT_SOURCE_DIR})
TRPC_COMPILE_PROTO(TRIP_TRPC_PB_SRCS "${TRIP_PROTO}" ${PB_PROTOC} ${TRPC_CPP_PLUGIN} ${CMAKE_CURRENT_SOURCE_DIR})
COMPILE_PROTO(DHT_PB_SRCS "${DHT_PROTO}" ${PB_PROTOC} ${CMAKE_CURRENT_SOURCE_DIR})
TRPC_COMPILE_PROTO(DHT_TRPC_PB_SRCS "${DHT_PROTO}" ${PB_PROTOC} ${TRPC_CPP_PLUGIN} ${CMAKE_CURRENT_SOURCE_DIR})

set(TRPC_LIBRARIES trpc ${LIBS_BASIC})

add_executable(pricing_server
  src/pricing_server.cc
  src/pricing_service.cc
  ${PRICING_PB_SRCS}
  ${PRICING_TRPC_PB_SRCS})
target_include_directories(pricing_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pricing_server ${TRPC_LIBRARIES})

add_executable(trip_server
  src/trip_server.cc
  src/trip_service.cc
  ${PRICING_PB_SRCS}
  ${PRICING_TRPC_PB_SRCS}
  ${TRIP_PB_SRCS}
  ${TRIP_TRPC_PB_SRCS})
target_include_directories(trip_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(trip_server ${TRPC_LIBRARIES})

add_executable(ride_client
  src/ride_client.cc
  ${TRIP_PB_SRCS}
  ${TRIP_TRPC_PB_SRCS})
target_include_directories(ride_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ride_client ${TRPC_LIBRARIES})

add_executable(dht_server
  src/dht_server.cc
  src/dht_service.cc
  src/dht_common.cc
  ${DHT_PB_SRCS}
  ${DHT_TRPC_PB_SRCS})
target_include_directories(dht_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dht_server ${TRPC_LIBRARIES})

add_executable(dht_smoke_client
  tests/dht_smoke_client.cc
  ${DHT_PB_SRCS}
  ${DHT_TRPC_PB_SRCS})
target_include_directories(dht_smoke_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dht_smoke_client ${TRPC_LIBRARIES})

add_executable(dht_common_test
  tests/dht_common_test.cc
  src/dht_common.cc)
target_include_directories(dht_common_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_test(NAME dht_common_test COMMAND dht_common_test)
