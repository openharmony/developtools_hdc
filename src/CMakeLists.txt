cmake_minimum_required (VERSION 3.4)

project (hdc)

get_filename_component(ohos_top "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -ggdb -Wall -pg")

include_directories(include common
    ${ohos_top}/third_party/bounds_checking_function/include
    ${ohos_top}/utils/native/base/include)

add_subdirectory(daemon)
add_subdirectory(host)
