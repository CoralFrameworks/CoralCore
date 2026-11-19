# ==================================================================================================
# Tests
#
# libs/utils had the empty section marker but no tests, which is how `src/Utf.cpp` could sit in no build
# target: nothing in this directory was ever compiled or run. The first suite is therefore the one for
# the file that was missing from the library.
# ==================================================================================================

set(TEST_TARGET test_utils)

add_executable(${TEST_TARGET}
    test_main.cpp
    test_JobSystem.cpp
    test_Utf.cpp
)

target_link_libraries(${TEST_TARGET} PRIVATE ${TARGET} gtest)

set_target_properties(${TEST_TARGET} PROPERTIES FOLDER Tests)

add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
