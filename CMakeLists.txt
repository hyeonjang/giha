cmake_minimum_required(VERSION 3.25)
project(giha LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
 set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# options
option(GIHA_SLANG "Enable slang" ON)

# slang library import and setting
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadGithubRelease)
include(FetchContent)

# slang
if (GIHA_SLANG)
    set(SLANG_VERSION "2025.23.2") 
    set(SLANG_DIR "${CMAKE_BINARY_DIR}/external/slang")

    download_github_release(
        "${SLANG_DIR}"
        REPO https://github.com/shader-slang/slang
        TAG  "v${SLANG_VERSION}"
        VERSION "${SLANG_VERSION}"
        SYSTEM windows-x86_64
        ASSET_TEMPLATE "slang-{version}-{system}.zip"
    )

    # add_library(slang SHARED IMPORTED GLOBAL)                                                                                               
    # set_target_properties(slang PROPERTIES                                                                                                  
    #     INTERFACE_INCLUDE_DIRECTORIES "${SLANG_DIR}/include"                                                                               
    #     IMPORTED_IMPLIB "${SLANG_DIR}/lib/slang.lib"                                                                                       
    #     IMPORTED_LOCATION "${SLANG_DIR}/bin/slang.dll"
    # )   

    find_path(SLANG_INCLUDE_DIR 
        NAMES slang.h 
        PATHS 
            ${SLANG_DIR}/include
            ${SLANG_DIR}/slang/include
            ${SLANG_DIR}/source/slang/include
        NO_DEFAULT_PATH
    )

    find_library(SLANG_LIBRARY 
        NAMES slang libslang
        PATHS 
            ${SLANG_DIR}/lib
            ${SLANG_DIR}/bin
            ${SLANG_DIR}/build/lib
            ${SLANG_DIR}/build/bin
        NO_DEFAULT_PATH
    )

    if(NOT SLANG_INCLUDE_DIR)
        message(WARNING "Slang headers not found, using fallback paths")
        set(SLANG_INCLUDE_DIR ${SLANG_DIR})
    endif()
endif()


set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(giha ${SOURCES})

# Add external libraries
set(GIHA_INCLUDES)
set(GIHA_LIBS)

# slang
if (GIHA_SLANG)
    message(STATUS "Enable slang")
    target_compile_definitions(giha PUBLIC GIHA_SLANG=1)
    list(APPEND GIHA_INCLUDES ${SLANG_INCLUDE_DIR})
    list(APPEND GIHA_LIBS ${SLANG_LIBRARY})
endif()

# 
# make library
# 
target_include_directories(
    giha
    PUBLIC include
    PRIVATE ${GIHA_INCLUDES}
)

target_compile_definitions(giha PRIVATE
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>)
target_link_libraries(giha PRIVATE ${GIHA_LIBS})
target_compile_features(giha PUBLIC cxx_std_20)

set_target_properties(giha PROPERTIES                                                                                                 
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"                                                                            
)   

#
# test cases
#
add_subdirectory(test)
