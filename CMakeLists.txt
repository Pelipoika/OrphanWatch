cmake_minimum_required(VERSION 3.21)
project(OrphanWatch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(json)

# Collect sources
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

# WIN32 flag gives us WinMain entry point (no console window)
add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    resources/resource.rc
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    wbemuuid
    ole32
    oleaut32
    shell32
    comctl32
    shlwapi
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Copy config directory next to the executable after each build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "Copying config directory to output directory"
)

# MSVC-specific settings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
    # Disable linker-generated manifest (we embed our own via resource.rc)
    target_link_options(${PROJECT_NAME} PRIVATE /MANIFEST:NO)
endif()
