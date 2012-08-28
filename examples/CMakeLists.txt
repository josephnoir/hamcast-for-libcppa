cmake_minimum_required(VERSION 2.6)
project(group_chat CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wextra -Wall -pedantic") 

# Set up environment paths to cmake modules and libcppa
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(group_chat group_chat.cpp)

# search for libs
if(NOT cppa_LIBRARY)
    find_package(Libcppa REQUIRED)
endif (NOT cppa_LIBRARY)

if(NOT hamcast_LIBRARY)
    find_package(LibHamcast REQUIRED)
endif (NOT hamcast_LIBRARY)

if(NOT hamcast_for_libcppa_LIBRARY)
    find_package(Libhamcast_for_libcppa REQUIRED)
endif (NOT hamcast_for_libcppa_LIBRARY)

find_package(Boost COMPONENTS system thread REQUIRED)

link_directories(${Boost_LIBRARY_DIRS} ${CPPA_INCLUDE} ${HAMCAST_INCLUDE} ${HAMCAST_FOR_LIBCPPA_INCLUDE})
include_directories(. ${CPPA_INCLUDE} ${HAMCAST_INCLUDE} ${Boost_INCLUDE_DIRS} ${HAMCAST_FOR_LIBCPPA_INCLUDE})

set(EXAMPLE_LIBS ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${HAMCAST_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${HAMCAST_FOR_LIBCPPA_LIBRARY})

target_link_libraries(group_chat ${EXAMPLE_LIBS})
