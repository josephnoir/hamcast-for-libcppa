cmake_minimum_required(VERSION 2.4)
project(hamcast_for_libcppa CXX)

set(LIBHAMCAST_FOR_LIBCPPA_VERSION_MAJOR 0)
set(LIBHAMCAST_FOR_LIBCPPA_VERSION_MINOR 1)
set(LIBHAMCAST_FOR_LIBCPPA_VERSION_PATCH 0)

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
    set(CXXFLAGS_PROVIDED true)
endif ()
if (CXXFLAGS_PROVIDED)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CMAKE_CXX_FLAGS                "-std=c++11 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if (NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if (CompilerVersion VERSION_GREATER 4.6)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion}.")
  endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  if (CompilerVersion VERSION_GREATER 3.1)
    message(STATUS "Found clang++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
  endif ()
  if (NOT CXXFLAGS_PROVIDED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DEBUG")
else ()
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

if (DISABLE_CONTEXT_SWITCHING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPA_DISABLE_CONTEXT_SWITCHING")
endif ()

# me did: renamed from LIBCPPA_SRC to HAMCAST_FOR_LIBCPPA_SRC
set(LIBHAMCAST_FOR_LIBCPPA_SRC 
    src/hamcast_group_module.cpp
)

add_library(libhamcast_for_libcppa SHARED ${LIBHAMCAST_FOR_LIBCPPA_SRC})

set(LIBRARY_VERSION ${LIBHAMCAST_FOR_LIBCPPA_VERSION_MAJOR}.${LIBHAMCAST_FOR_LIBCPPA_VERSION_MINOR}.${LIBHAMCAST_FOR_LIBCPPA_VERSION_PATCH})
set(LIBRARY_SOVERSION ${LIBHAMCAST_FOR_LIBCPPA_VERSION_MAJOR})

set_target_properties(libhamcast_for_libcppa 
                      PROPERTIES
                      SOVERSION ${LIBRARY_SOVERSION}
                      VERSION ${LIBRARY_VERSION}
                      OUTPUT_NAME hamcast_for_libcppa)

#find boost
find_package(Boost COMPONENTS thread system  REQUIRED)
link_directories(${Boost_LIBRARY_DIRS})

# find cppa
find_package(LibCPPA REQUIRED)

# find hamcast
find_package(LibHamcast REQUIRED)

include_directories(. ${Boost_INCLUDE_DIRS}  ${CPPA_INCLUDE} ${HAMCAST_INCLUDE})
target_link_libraries(libhamcast_for_libcppa ${CMAKE_LD_LIBS} ${CPPA_LIBRARY} ${HAMCAST_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})

# install shared library
install(TARGETS libhamcast_for_libcppa LIBRARY DESTINATION lib)

# install includes
install(DIRECTORY hc_cppa/ DESTINATION include/hc_cppa FILES_MATCHING PATTERN "*.hpp")

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# add uninstall target
add_custom_target(uninstall
                  COMMAND ${CMAKE_COMMAND} -P
                  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

if (LIBRARY_OUTPUT_PATH)
  set (HAMCAST_FOR_LIBCPPA_LIBRARY_OUTPUT_PATH ${LIBRARY_OUTPUT_PATH})
  set (HAMCAST_FOR_LIBCPPA_LIBRARY_PATH ${LIBRARY_OUTPUT_PATH})
else ()
  set (HAMCAST_FOR_LIBCPPA_LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)
  set (HAMCAST_FOR_LIBCPPA_LIBRARY_PATH ${CPPA_LIBRARY_OUTPUT_PATH})
  set (LIBRARY_OUTPUT_PATH ${HAMCAST_FOR_LIBCPPA_LIBRARY_OUTPUT_PATH} CACHE PATH "Single directory for all libraries")
endif ()

# setting path to hamcast_for_libcppa headers and libhamcast_for_libcppa
set (HAMCAST_FOR_LIBCPPA_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/hc_cppa)
set (HAMCAST_FOR_LIBCPPA_INCLUDE ${HAMCAST_FOR_LIBCPPA_INCLUDE_PATH})

if (APPLE)
  set (HAMCAST_FOR_LIBCPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libhamcast_for_libcppa.dylib)
elseif (UNIX)
  set (HAMCAST_FOR_LIBCPPA_LIBRARY ${LIBRARY_OUTPUT_PATH}/libhamcast_for_libcppa.so)
else ()
  message (SEND_FATAL "Host platform not supported ...")
endif ()

if (EXECUTABLE_OUTPUT_PATH)
else ()
  set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin CACHE PATH "Single directory for all executables")
endif ()

enable_testing()

# set up subdirectories
add_subdirectory(examples)

# add hamcast_for_libcppa as a dependency to all binaries
add_dependencies(group_chat libhamcast_for_libcppa)

# set optional flags
string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
set(CONTEXT_SWITCHING true)
if (DISABLE_CONTEXT_SWITCHING)
    set(CONTEXT_SWITCHING false)
endif ()

# check for doxygen and add custom "doc" target to Makefile
#find_package(Doxygen)
#if (DOXYGEN_FOUND)
#  configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_SOURCE_DIR}/Doxyfile
#                 @ONLY)
#  add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/Doxyfile
#                    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
#                    COMMENT "Generating API documentation with Doxygen"
#                    VERBATIM)
#endif ()

# done (print summary)
message("\n====================|  Build Summary  |===================="
        "\n"
        "\nhamcast_for_libcppa version:   ${LIBRARY_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nDebug mode:        ${ENABLE_DEBUG}"
        "\nContext switching: ${CONTEXT_SWITCHING}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type}}"
        "\n"
        "\nSource directory:  ${CMAKE_SOURCE_DIR}"
        "\nBuild directory:   ${CMAKE_BINARY_DIR}"
        "\nExecutable path:   ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:      ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:    ${CMAKE_INSTALL_PREFIX}"
        "\n"
        "\nBoost:             ${Boost_INCLUDE_DIR}"
        "\n"
        "\n===========================================================\n")
