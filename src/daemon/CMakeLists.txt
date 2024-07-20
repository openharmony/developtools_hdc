add_definitions(-DUSE_CONFIG_UV_THREADS -DSIZE_THREAD_POOL=16 -DJS_JDWP_CONNECT -DTEST_HASH -DHDC_MSG_HASH="TEST")

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_LIST_DIR} DAEMON_SRCS)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_LIST_DIR}/../common COMMON_SRCS)

link_libraries(uv crypto
    ${ohos_top}/out/sdk/clang_x64/obj/third_party/bounds_checking_function/libsec_static.a)

add_executable(hdcd ${COMMON_SRCS} ${DAEMON_SRCS})
