include(FindProtobuf)

if (NOT PROTOBUF_PROTOC_EXECUTABLE)
    get_filename_component(PROTO_LIB_DIR ${PROTOBUF_LIBRARY} DIRECTORY)
    set (PROTOBUF_PROTOC_EXECUTABLE "${PROTO_LIB_DIR}/../bin/protoc")
endif()

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER rpc.proto storage.proto config.proto)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROTOBUF_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/third_party/json11
        ${PROJECT_SOURCE_DIR}/third_party/libco
        ${PROJECT_SOURCE_DIR}/third_party/pb2json/byReflection)

set(SRC  main.cpp Core/Log.cpp Core/MultiPaxos.cpp Core/MultiPaxos.h Core/Utils.cpp Core/Network.cpp Core/Server.cpp Core/Server.h Core/Storage.cpp Core/Storage.h Core/Storage_mem.cpp Core/Storage_mem.h)
add_executable(main main.cpp ${SRC} ${PROTO_SRC} ${PROTO_HEADER})
target_link_libraries(main colib_static json11  pb2json libprotobuf.a libleveldb.a librocksdb.a libz.a libbz2.a libsnappy.a pthread dl rt libm.a  libc.a)