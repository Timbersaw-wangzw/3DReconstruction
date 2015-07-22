# Declares the dependencies of an application:
# Usage: DeclareAppDependencies( appTargetName [fblib_xxx [fblib_yyy] ...] )
#
macro(DeclareAppDependencies name)
	# Set app names:
	if(ENABLE_SOLUTION_FOLDERS)
		set_target_properties(${name} PROPERTIES FOLDER "applications")
	else(ENABLE_SOLUTION_FOLDERS)
		SET_TARGET_PROPERTIES(${name} PROPERTIES PROJECT_LABEL "(APP) ${name}")	
	endif(ENABLE_SOLUTION_FOLDERS)
	
	# set deps:
	set(ALL_DEPS "")
	
	FOREACH(DEP ${ARGN})
		LIST(APPEND ALL_DEPS ${DEP})
	ENDFOREACH(DEP)

	FOREACH(DEP ${ARGN})
		# Only for "fblib_XXX" libs:
		IF (${DEP} MATCHES "fblib_")
			get_property(LIB_DEP GLOBAL PROPERTY "${DEP}_LIB_DEPS")
			LIST(APPEND ALL_DEPS ${LIB_DEP})
		ENDIF (${DEP} MATCHES "fblib_")
	ENDFOREACH(DEP)
	
	
	# Add the detected dependencies:
	list(REMOVE_DUPLICATES ALL_DEPS)

	set(AUX_ALL_DEPS_BUILD 1)  # Will be set to "0" if any dependency if not built

	IF (NOT "${ALL_DEPS}" STREQUAL "")
		#MESSAGE(STATUS "Adding deps: ${name} --> ${ALL_DEPS}")
		ADD_DEPENDENCIES(${name} ${ALL_DEPS})
		
		FOREACH (_DEP ${ALL_DEPS})
			# Link:
			IF(CMAKE_COMPILER_IS_GNUCXX OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
				get_property(_LIB_HDRONLY GLOBAL PROPERTY "${_DEP}_LIB_IS_HEADERS_ONLY")
				if (NOT _LIB_HDRONLY)
					TARGET_LINK_LIBRARIES(${name} ${_DEP}${FBLIB_LINKER_LIBS_POSTFIX})
				endif (NOT _LIB_HDRONLY)
			ENDIF()
			
			# Include:
			STRING(REGEX REPLACE "fblib_(.*)" "\\1" DEP_FBLIB_NAME ${_DEP})
			IF(NOT "${DEP_FBLIB_NAME}" STREQUAL "")
				INCLUDE_DIRECTORIES("${FBLIB_LIBS_ROOT}/${DEP_FBLIB_NAME}/include/")
			ENDIF(NOT "${DEP_FBLIB_NAME}" STREQUAL "")

			# Check if all dependencies are to be build: 
			if (${BUILD_fblib_${DEP_FBLIB_NAME}} STREQUAL "OFF")
				SET(AUX_ALL_DEPS_BUILD 0)
				MESSAGE(STATUS "*Warning*: App ${name} cannot be built because dependency fblib_${DEP_FBLIB_NAME} has been disabled!")
			endif (${BUILD_fblib_${DEP_FBLIB_NAME}} STREQUAL "OFF")

		ENDFOREACH (_DEP)
	ENDIF ()
	
	# Impossible to build? 
	if (NOT AUX_ALL_DEPS_BUILD)
		MESSAGE(STATUS "*Warning* ==> Forcing BUILD_APP_${name}=OFF for missing dependencies listed above (re-enable manually if needed).")
		SET(BUILD_APP_${name} OFF CACHE BOOL "Build ${name}" FORCE) # this var is checked in [FBLIB]/app/CMakeLists.txt
		mark_as_advanced(CLEAR BUILD_APP_${name})
	endif (NOT AUX_ALL_DEPS_BUILD)

endmacro(DeclareAppDependencies)

# Macro for adding links to the Start menu folder (for binary packages in Windows)
macro(AppStartMenuLink name title)
	get_property(_str GLOBAL PROPERTY "FBLIB_CPACK_PACKAGE_EXECUTABLES")
	set_property(GLOBAL PROPERTY "FBLIB_CPACK_PACKAGE_EXECUTABLES" "${_str}${name};${title};")
endmacro(AppStartMenuLink)


macro(DeclareAppForInstall name)
INSTALL(TARGETS ${name}
	RUNTIME DESTINATION ${fblib_apps_INSTALL_PREFIX}bin COMPONENT Apps
	LIBRARY DESTINATION ${fblib_apps_INSTALL_PREFIX}lib${LIB_SUFFIX} COMPONENT Apps
	ARCHIVE DESTINATION ${fblib_apps_INSTALL_PREFIX}lib${LIB_SUFFIX} COMPONENT Apps)
endmacro(DeclareAppForInstall)
