#Look for glib2.0. If found, defines:
# GTK_glib_INLUCDE_PATH
# GTK_glib_LIBRARY
# amoung others.
include  (FindTclStub)
option (USE_TCL_STUBS "Enable use of TCL stubs library")

include (FindPkgConfig)
ADD_DEFINITIONS("`pkg-config --cflags glib-2.0`")
pkg_check_modules (GLIB2 REQUIRED glib-2.0)

include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${TCL_INCLUDE_PATH})
include_directories(${TK_INCLUDE_PATH})
include_directories(.)
include_directories(./base)

########### next target ###############
set (tcltheora_SRCS
	tcltheora_Init.c 
)

add_library(tcltheora MODULE ${tcltheora_SRCS})

if (USE_TCL_STUBS)
	target_link_libraries(tcltheora ${TCL_STUB_LIBRARY} ${TCLARGV_LIBRARY} ogg theora)
else (USE_TCL_STUBS)
	target_link_libraries(tcltheora ${TCL_LIBRARY} ${TCLARGV_LIBRARY} ogg theora)
endif (USE_TCL_STUBS)

set_target_properties (tcltheora PROPERTIES VERSION 0.1 SOVERSION 0 PREFIX "" INSTALL_RPATH_USE_LINK_PATH on)


########### install files ###############

install(TARGETS tcltheora DESTINATION lib/${PACKAGE})
