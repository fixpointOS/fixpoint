set (default_build_type "Release")
if (NOT (CMAKE_BUILD_TYPE_SHADOW STREQUAL CMAKE_BUILD_TYPE))
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message (STATUS "Setting build type to '${default_build_type}'")
        set (CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    else ()
        message (STATUS "Building in ${CMAKE_BUILD_TYPE} mode as requested.")
    endif ()
    set (CMAKE_BUILD_TYPE_SHADOW ${CMAKE_BUILD_TYPE} CACHE STRING "used to detect changes in build type" FORCE)
endif ()

message (STATUS "  NOTE: You can choose a build type by calling cmake with one of:")
message (STATUS "    -DCMAKE_BUILD_TYPE=Release   -- full optimizations")
message (STATUS "    -DCMAKE_BUILD_TYPE=Debug     -- better debugging experience in gdb")

message (STATUS "")

message (STATUS "  In addition, you can enable sanitizers by adding:")
message (STATUS "    -DUSE_ASAN=ON                -- address sanitizer")
message (STATUS "    -DUSE_UBSAN=ON               -- undefined behavior sanitizer")
message (STATUS "    -DUSE_TSAN=ON                -- thread sanitizer")
