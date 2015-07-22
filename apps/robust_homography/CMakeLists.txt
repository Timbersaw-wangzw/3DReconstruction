INCLUDE(../../cmake/AssureCMakeRootFile.cmake) # Avoid user mistake in CMake source directory

#-----------------------------------------------------------------
# CMake file for the FBLIB application:  robust_homography
#
#  Run with "cmake ." at the root directory
#
#  December 2015, fengbing <fengbing123@gmail.com>
#-----------------------------------------------------------------
PROJECT(robust_homography)

#MESSAGE(STATUS "Makefile for application: /apps/robust_homography ")
ADD_DEFINITIONS(-DTHIS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# ---------------------------------------------
# TARGET:
# ---------------------------------------------
# Define the executable target:
ADD_EXECUTABLE(robust_homography
               robust_homography.cpp
			    ${FBLIB_VERSION_RC_FILE})

SET(TMP_TARGET_NAME "robust_homography")

INCLUDE_DIRECTORIES("${FBLIB_SOURCE_DIR}/3rdparty/sift/vl")

# Add the required libraries for linking:
TARGET_LINK_LIBRARIES(${TMP_TARGET_NAME} ${FBLIB_LINKER_LIBS} sift)

# Dependencies on FBLIB libraries:
#  Just mention the top-level dependency, the rest will be detected automatically, 
#  and all the needed #include<> dirs added (see the script DeclareAppDependencies.cmake for further details)
DeclareAppDependencies(${TMP_TARGET_NAME} fblib_base fblib_image fblib_feature fblib_multiview fblib_camera)

DeclareAppForInstall(${TMP_TARGET_NAME})
