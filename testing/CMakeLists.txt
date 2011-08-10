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
set (theora_test_SRCS
	theora_test.c 
)

add_executable(theora_test ${theora_test_SRCS})

if (USE_TCL_STUBS)
	target_link_libraries(theora_test ${TCL_STUB_LIBRARY} ${TCLARGV_LIBRARY} ${GLIB2_LIBRARIES} ogg theoradec)
else (USE_TCL_STUBS)
	target_link_libraries(theora_test ${TCL_LIBRARY} ${TCLARGV_LIBRARY} ${GLIB2_LIBRARIES} ogg theoradec)
endif (USE_TCL_STUBS)

########### install files ###############

install(TARGETS theora_test DESTINATION bin)
