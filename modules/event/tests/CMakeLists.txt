# ==================================================================================================
# Tests
#
# Each test is a standalone executable named tests_<Thing>, registered with CTest.
# ==================================================================================================

add_executable(tests_EventLoop tests_EventLoop.cpp)
target_link_libraries(tests_EventLoop PRIVATE event)

add_test(NAME tests_EventLoop COMMAND tests_EventLoop)
