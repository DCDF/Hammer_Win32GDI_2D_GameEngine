cmake_minimum_required(VERSION 3.28)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
# 开启模块扫描
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# 自动收集源码
file(GLOB_RECURSE ALL_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "src/*.c"
    "src/*.cpp"
    "src/*.rc"
    "rc/*.rc"
    "src/*.cppm"
    "src/*.ixx"
)
# 可执行文件
add_executable(${PROJECT_NAME} ${ALL_SOURCES})


target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/W3 /MTd /Zi /EHsc /utf-8>
    $<$<CONFIG:Release>:/Os /Ob1 /Oi /GL /Gw /fp:fast /DNDEBUG /MT /EHsc /utf-8>  # 将/O2改为/Os，/Ob2改为/Ob1
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF>
)
