# ----------------------------------------------------------------------------
#   				UPDATE CONFIG FILES & SCRIPTS:
#
#  CONFIGURE_FILE(InputFile OutputFile [COPYONLY] [ESCAPE_QUOTES] [@ONLY])
# If @ONLY is specified, only variables of the form @VAR@ will be
#  replaces and ${VAR} will be ignored.
#
#  A directory will be created for each platform so the "config.h" file is
#   not overwritten if cmake generates code in the same path.
# ----------------------------------------------------------------------------
SET(MVG_CONFIG_FILE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/mvg_config/" CACHE PATH "Where to create the platform-dependant config.h")
IF(UNIX)
	SET(MVG_CONFIG_FILE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/mvg_config/unix/" )
ENDIF(UNIX)
IF (WIN32)
	SET(MVG_CONFIG_FILE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/mvg_config/win32/")
ENDIF(WIN32)

FILE(MAKE_DIRECTORY  "${MVG_CONFIG_FILE_INCLUDE_DIR}")
FILE(MAKE_DIRECTORY  "${MVG_CONFIG_FILE_INCLUDE_DIR}/mvg")

#MESSAGE(STATUS "Parsing 'config.h.in'")
CONFIGURE_FILE("${CMAKE_SOURCE_DIR}/parse-files/config.h.in" "${MVG_CONFIG_FILE_INCLUDE_DIR}/mvg/config.h")

INCLUDE_DIRECTORIES("${MVG_CONFIG_FILE_INCLUDE_DIR}")
