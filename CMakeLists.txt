set (PACKAGE tcltheora)
project (${PACKAGE})
cmake_minimum_required (VERSION 2.6)

option (BUILD_SHARED_LIB "Build Shared Libraries." ON)

# Set up some variables that we can use in our code.
set (TCLTHEORA_VERSION 0.1)
set (TCLTHEORA_MAJOR_VERSION 0)
set (TCLTHEORA_MINOR_VERSION 1)
set (TCLTHEORA_PATCH_LEVEL 0)
set (TCLSCRIPTDIR ${CMAKE_INSTALL_PREFIX}/lib/${PACKAGE})
set (VERSION ${TCLTHEORA_VERSION})
add_definitions (-DHAVE_CONFIG_H)
include_directories (${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Fix for static library linking to shared library on 64 bit systems:
# with -fPIC
IF(UNIX AND NOT WIN32)
  FIND_PROGRAM(CMAKE_UNAME uname /bin /usr/bin /usr/local/bin )
  IF(CMAKE_UNAME)
    EXEC_PROGRAM(uname ARGS -m OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR)
    SET(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR} CACHE INTERNAL
			"processor type (i386 and x86_64)")
		 IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
		   ADD_DEFINITIONS(-fPIC)
		 ENDIF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  ENDIF(CMAKE_UNAME)
ENDIF(UNIX AND NOT WIN32)

add_subdirectory(src)
#add_subdirectory(scripts)
add_subdirectory(testing)

# the following lines are needed to build debian/ubuntu packages.
set (CPACK_DEBIAN_PACKAGE_NAME ${PACKAGE})
set (CPACK_DEBIAN_PACKAGE_VERSION ${VERSION})
set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set (CPACK_DEBIAN_PACKAGE_DEPENDS "tcl8.5 (>=8.5.8-2), tk8.5 (>=8.5.8-2), zlib1g (>=1:1.2.3.3.dfsg-15ubuntu1), libglib2.0-0 (>=2.24.1-0ubuntu1), libfftw3-3 (>=3.2.2-1), libpango1.0-0 (>=1.28.0-0ubuntu2.2), libfreetype6 (>=2.3.11-1ubuntu2.4), libfontconfig1 (>=2.8.0-2ubuntu1), libexpat1 (>=2.0.1-7ubuntu1), libtheora0 (>=1.1.1+dfsg.1-3), libogg0 (>=1.2.0~dfsg-1)")
set (CPACK_DEBIAN_PACKAGE_MAINTAINER "Sam Bromley <sam@sambromley.com>")
set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "Tcl Interface to Reading Ogg Theora Video")
set (CPACK_DEBIAN_PACKAGE_SECTION "devel")
set (CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set (DEBIAN_PACKAGE_BUILDS_DEPENDS "tcl8.5-dev (>=8.5.8-2), tk8.5-dev (>=8.5.8-2), zlib1g-dev (>=1:1.2.3.3.dfsg-15ubuntu1), libglib2.0-dev (>=2.24.1-0ubuntu1), libfftw3-dev (>=3.2.2-1), libpango1.0-dev (>=1.28.0-0ubuntu2.2), libfreetype6-dev (>=2.3.11-1ubuntu2.4), libfontconfig1-dev (>=2.8.0-2ubuntu1), libexpat1-dev (>=2.0.1-7ubuntu1), libtheora-dev (>=1.1.1+dfsg.1-3), libogg-dev (>=1.2.0~dfsg-1)")
set (CPACK_GENERATOR "DEB")
set (CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_PATH_PREFIX}/${PACKAGE}/")
set (CPACK_COMPONENTS_ALL libraries headers executables)
set (CPACK_SET_DESTDIR TRUE)
include(CPack)
