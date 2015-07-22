# By default, compile this library if the directory exist:
# ------------------------------------------------------------------------
IF(EXISTS "${FBLIB_LIBS_ROOT}/3rdparty/ssba")
	SET( CMAKE_FBLIB_HAS_SSBA 1)
ELSE(EXISTS "${FBLIB_LIBS_ROOT}/3rdparty/ssba")
	SET( CMAKE_FBLIB_HAS_SSBA 0)
ENDIF(EXISTS "${FBLIB_LIBS_ROOT}/3rdparty/ssba")

OPTION(DISABLE_SSBA "Disable the ssba library" "OFF")
MARK_AS_ADVANCED(DISABLE_SSBA)
IF(DISABLE_SSBA)
	SET(CMAKE_FBLIB_HAS_SSBA 0)
ENDIF(DISABLE_SSBA)