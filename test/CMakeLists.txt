cmake_minimum_required(VERSION 3.10)

# Set the project name
project(giha-test)
option(GIHA_SLANG_RHI "Enable slang rhi code" ON)

# slang-rhi
if (GIHA_SLANG_RHI)
    FetchContent_Declare(
        slang-rhi
        GIT_REPOSITORY https://github.com/shader-slang/slang-rhi.git
        GIT_TAG main
    )

    FetchContent_MakeAvailable(slang-rhi)
    set_target_properties(slang-rhi PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

endif()

file(GLOB_RECURSE TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add an executable
add_executable(giha_test ${TEST_SOURCES})
target_include_directories(
    giha_test
    PUBLIC ${SLANG_INCLUDE_DIR} slang-rhi giha
)
target_link_libraries(giha_test PRIVATE ${SLANG_LIBRARY} slang-rhi giha)

if (TARGET giha)
    get_target_property(GIHA_RUNTIME giha RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(giha_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GIHA_RUNTIME}")
endif()
