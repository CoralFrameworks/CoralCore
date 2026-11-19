# ==================================================================================================
# Usage examples
#
# Every example is a standalone executable that exercises one module and checks what it observed, so
# running it is also the verification that the standalone checkout compiles, links and executes. They
# are built when CORALCORE_BUILD_DEMOS is on, which is the default when CoralCore is the top-level
# CMake project and off when it is added to an application tree.
#
# Each demo links only the module it demonstrates: the set of include paths and libraries a consumer
# of that module needs is the same set the demo needs.
# ==================================================================================================

function(coralcore_add_demo demo_name)
  set(target "coralcore_demo_${demo_name}")

  add_executable(${target} ${demo_name}.cpp)

  target_link_libraries(${target} PRIVATE ${ARGN})

  set_target_properties(${target} PROPERTIES FOLDER Demos)

  # When testing is enabled, the demos double as smoke tests, so `ctest` verifies the build as well.
  if (CORAL_BUILD_TESTS)
    add_test(NAME ${target} COMMAND ${target})
  endif ()
endfunction()

coralcore_add_demo(hello core)
coralcore_add_demo(utils utils)
coralcore_add_demo(observer observer)
coralcore_add_demo(event_loop event)
coralcore_add_demo(job_system utils)
