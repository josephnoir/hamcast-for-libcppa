# - Try to find libcppa
# Once done this will define
#
#  HAMCAST_FOR_LIBCPPA_FOUND    - system has libcppa
#  HAMCAST_FOR_LIBCPPA_INCLUDE  - libcppa include dir
#  HAMCAST_FOR_LIBCPPA_LIBRARY  - link againgst libcppa
#

if (HAMCAST_FOR_LIBCPPA_LIBRARY AND HAMCAST_FOR_LIBCPPA_INCLUDE)
  set(HAMCAST_FOR_LIBCPPA_FOUND TRUE)
else (HAMCAST_FOR_LIBCPPA_LIBRARY AND HAMCAST_FOR_LIBCPPA_INCLUDE)

  find_path(HAMCAST_FOR_LIBCPPA_INCLUDE
    NAMES
      hc_cppa/hamcast_group_module.hpp
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      ${HAMCAST_FOR_LIBCPPA_INCLUDE_PATH}
      ${HAMCAST_FOR_LIBCPPA_LIBRARY_PATH}
      ${CMAKE_INCLUDE_PATH}
      ${CMAKE_INSTALL_PREFIX}/include
  )
  
  if (HAMCAST_FOR_LIBCPPA_INCLUDE)
    message (STATUS "Header files found ...")
  else (HAMCAST_FOR_LIBCPPA_INCLUDE)
    message (SEND_ERROR "Header files NOT found. Provide absolute path with -DHAMCAST_FOR_LIBCPPA_INCLUDE_PATH=<path-to-header>.")
  endif (HAMCAST_FOR_LIBCPPA_INCLUDE)

  find_library(HAMCAST_FOR_LIBCPPA_LIBRARY
    NAMES
      libhamcast_for_libcppa
      hamcast_for_libcppa
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ${HAMCAST_FOR_LIBCPPA_INCLUDE_PATH}
      ${HAMCAST_FOR_LIBCPPA_INCLUDE_PATH}/.libs
      ${HAMCAST_FOR_LIBCPPA_LIBRARY_PATH}
      ${HAMCAST_FOR_LIBCPPA_LIBRARY_PATH}/.libs
      ${CMAKE_LIBRARY_PATH}
      ${CMAKE_INSTALL_PREFIX}/lib
      ${LIBRARY_OUTPUT_PATH}
  )

  if (HAMCAST_FOR_LIBCPPA_LIBRARY) 
    message (STATUS "Library found ...")
  else (HAMCAST_FOR_LIBCPPA_LIBRARY)
    message (SEND_ERROR "Library NOT found. Provide absolute path with -DHAMCAST_FOR_LIBCPPA_LIBRARY_PATH=<path-to-library>.")
  endif (HAMCAST_FOR_LIBCPPA_LIBRARY)

  if (HAMCAST_FOR_LIBCPPA_INCLUDE AND HAMCAST_FOR_LIBCPPA_LIBRARY)
    set(HAMCAST_FOR_LIBCPPA_FOUND TRUE)
    set(HAMCAST_FOR_LIBCPPA_INCLUDE ${HAMCAST_FOR_LIBCPPA_INCLUDE})
    set(HAMCAST_FOR_LIBCPPA_LIBRARY ${HAMCAST_FOR_LIBCPPA_LIBRARY})
  else (HAMCAST_FOR_LIBCPPA_INCLUDE AND HAMCAST_FOR_LIBCPPA_LIBRARY)
    message (FATAL_ERROR "HAMCAST_FOR_LIBCPPA LIBRARY AND/OR HEADER NOT FOUND!")
  endif (HAMCAST_FOR_LIBCPPA_INCLUDE AND HAMCAST_FOR_LIBCPPA_LIBRARY)

endif (HAMCAST_FOR_LIBCPPA_LIBRARY AND HAMCAST_FOR_LIBCPPA_INCLUDE)
