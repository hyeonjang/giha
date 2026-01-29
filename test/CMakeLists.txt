cmake_minimum_required(VERSION 3.10)

# Set the project name
project(giha-test)
set(GIHA_SLANG_RHI ON CACHE BOOL "Enable slang rhi code" FORCE)

# slang-rhi
if (GIHA_SLANG_RHI)
    message(STATUS "Building giha-test with slang-rhi support")

    file(GLOB_RECURSE TEST_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )

    # Add an executable
    add_executable(giha_test ${TEST_SOURCES})
    target_include_directories(giha_test PUBLIC giha)
    target_link_libraries(giha_test PRIVATE giha)

    if (TARGET slang-rhi)
        target_link_libraries(giha_test PRIVATE slang-rhi)
    endif()

    if (TARGET giha)
        get_target_property(GIHA_RUNTIME giha RUNTIME_OUTPUT_DIRECTORY)
        set_target_properties(giha_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GIHA_RUNTIME}")
    endif()
endif()
