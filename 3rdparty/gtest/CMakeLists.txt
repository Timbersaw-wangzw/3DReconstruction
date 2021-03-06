PROJECT(mvggtest)

# Fix a "bug" in VS11 (MSVC 2012):
if(MSVC)
	add_definitions(-D_VARIADIC_MAX=10)
endif(MSVC)

include_directories(include)
INCLUDE_DIRECTORIES("${MVG_SOURCE_DIR}/3rdparty/gtest/")

file(GLOB include/gtest/*.h include/gtest/internal/*.h src/*.h)

ADD_LIBRARY(mvggtest 
	STATIC
	src/gtest.cc
             src/gtest-death-test.cc
             src/gtest-filepath.cc
             src/gtest-port.cc
             src/gtest-printers.cc
             src/gtest-test-part.cc
             src/gtest-typed-test.cc 
	)

IF(CMAKE_COMPILER_IS_GNUCXX AND UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF(CMAKE_COMPILER_IS_GNUCXX AND UNIX)


if(ENABLE_SOLUTION_FOLDERS)
	set_target_properties(mvggtest PROPERTIES FOLDER "3rd party")
else(ENABLE_SOLUTION_FOLDERS)
	SET_TARGET_PROPERTIES(mvggtest  PROPERTIES PROJECT_LABEL "(TEST) mvggtest")
endif(ENABLE_SOLUTION_FOLDERS)



