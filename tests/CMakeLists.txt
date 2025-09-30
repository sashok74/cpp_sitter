if(BUILD_TESTS)
    include(GoogleTest)

    # Copy test fixtures
    file(COPY ${CMAKE_SOURCE_DIR}/tests/fixtures
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    # Helper library
    add_library(test_helpers INTERFACE)
    target_include_directories(test_helpers
        INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

    # Subdirectories will be added as we implement tests
    # add_subdirectory(core)
    # add_subdirectory(mcp)
    # add_subdirectory(tools)
    # add_subdirectory(integration)
endif()
