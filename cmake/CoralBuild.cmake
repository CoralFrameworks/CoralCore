# ==================================================================================================
# Coral's cross-cutting build settings
# ==================================================================================================
# Every framework repository includes this file and calls `coral_apply_global_build_settings()`
# before adding targets. The application root does the same; keeping the settings here means a
# standalone framework build does not need the application's CMakeLists.txt just to get a usable
# C++ standard, export policy or warning baseline.
#
# Keep this file free of paths and third-party discovery: those are per-repository concerns.

function(coral_apply_global_build_settings)
  option(USE_STATIC_CRT "Link against the static runtime libraries." ON)

  if (MSVC)
    if (USE_STATIC_CRT)
      add_compile_options(
          $<$<CONFIG:>:/MT>
          $<$<CONFIG:Debug>:/MTd>
          $<$<CONFIG:Release>:/MT>
      )
    else ()
      add_compile_options(
          $<$<CONFIG:>:/MD>
          $<$<CONFIG:Debug>:/MDd>
          $<$<CONFIG:Release>:/MD>
      )
    endif ()
  endif ()

  # Windows: generate the .def that makes a DLL import library. CL.EXE does not produce one from
  # __declspec(dllexport) alone in every configuration.
  if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON PARENT_SCOPE)
  endif ()

  set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
  set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)

  # Accumulate every platform addition locally and publish CMAKE_CXX_FLAGS once.
  # A variable set with PARENT_SCOPE is not updated in this scope, so reading
  # CMAKE_CXX_FLAGS back after one of those calls silently discarded the flags
  # that had just been added (the old WIN32 block did exactly that).
  set(_coral_cxx_flags "${CMAKE_CXX_FLAGS}")

  if (MSVC)
    # The project builds with C++20 (set above); keep the historical /W0 and the __cplusplus value
    # that libraries such as nlohmann/json check.
    string(APPEND _coral_cxx_flags " /W0 /Zc:__cplusplus")
  else ()
    string(APPEND _coral_cxx_flags
        " -fstrict-aliasing -Wno-unknown-pragmas -Wno-unused-function -Wno-deprecated-declarations")
    if (APPLE)
      string(APPEND _coral_cxx_flags " -Wno-nullability-extension")
    endif ()
  endif ()

  if (WIN32)
    string(APPEND _coral_cxx_flags " -D_USE_MATH_DEFINES=1")
  endif ()

  set(CMAKE_CXX_FLAGS "${_coral_cxx_flags}" PARENT_SCOPE)
  if (APPLE)
    # `.mm` sources are Objective-C++ even when the project declares only CXX
    # (CMake lets the driver pick the language from the extension). Set the
    # Objective-C++ baseline too, for builds that do enable the OBJCXX language.
    set(CMAKE_OBJCXX_FLAGS "${_coral_cxx_flags}" PARENT_SCOPE)
  endif ()
endfunction()
