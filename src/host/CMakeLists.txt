add_definitions(-DUSE_CONFIG_UV_THREADS -DSIZE_THREAD_POOL=128 -DHDC_HOST -DHOST_LINUX -DHARMONY_PROJECT -DTEST_HASH -DHDC_MSG_HASH="TEST")

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_LIST_DIR} HOST_SRCS)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_LIST_DIR}/../common COMMON_SRCS)

get_filename_component(MY_RESULT_ABSOLUTE_PATH_VAR
                       "${CMAKE_CURRENT_LIST_DIR}/${MY_RELATIVE_PATH_VAR}"
                       ABSOLUTE)

include_directories(${ohos_top}/third_party/libusb/include)
link_libraries(uv crypto lz4
    ${ohos_top}/out/sdk/clang_x64/obj/third_party/bounds_checking_function/libsec_static.a
    ${ohos_top}/out/sdk/clang_x64/common/common/libusb_shared.so)

add_executable(hdc ${COMMON_SRCS} ${HOST_SRCS})
