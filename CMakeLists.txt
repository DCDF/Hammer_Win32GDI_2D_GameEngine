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
add_executable(${PROJECT_NAME} WIN32 ${ALL_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Msimg32)


# 静态编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/W3 /MTd /Zi /EHsc /utf-8>
    $<$<CONFIG:Release>:/O2 /Ob2 /Oi /GL /Gw /fp:fast /GR- /DNDEBUG /MT /EHsc /utf-8>
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF>
)

# 资源复制
if(EXISTS "${CMAKE_SOURCE_DIR}/res")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying res/ -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    )
else()
    message(STATUS "res/ not found in project root; skipping copy of resources.")
endif()

# UPX 压缩
set(UPX_PATH "${CMAKE_SOURCE_DIR}/upx.exe" CACHE PATH "UPX 可执行文件路径（可选）")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "UPX found at ${UPX_PATH} -> ${PROJECT_NAME} UPX will run only for Release builds"
    COMMAND $<$<CONFIG:Release>:${UPX_PATH}> $<$<CONFIG:Release>:--best> $<$<CONFIG:Release>:--lzma> $<$<CONFIG:Release>:--force> $<$<CONFIG:Release>:$<TARGET_FILE:${PROJECT_NAME}>>
    COMMENT "Compressing exe with UPX (only for Release)"
)

# 调试信息
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
