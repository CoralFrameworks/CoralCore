# ==================================================================================================
# Tests
#
# Observer is header-only; the test executable is the only thing this module builds.
# ==================================================================================================

add_executable(tests_Observer tests_Observer.cpp test_main.cpp)
target_link_libraries(tests_Observer PRIVATE observer gtest)
set_target_properties(tests_Observer PROPERTIES FOLDER Tests)

add_test(NAME tests_Observer COMMAND tests_Observer)
