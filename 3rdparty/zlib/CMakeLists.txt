PROJECT(mvgzlib)

ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

ADD_LIBRARY(mvgzlib STATIC
  adler32.c    compress.c   crc32.c      deflate.c    infback.c
  inffast.c    inflate.c    inftrees.c    trees.c      uncompr.c
  zutil.c      crc32.h      deflate.h    inffast.h    inffixed.h   inflate.h
  inftrees.h   trees.h      zconf.h      zconf.h.in   zlib.h       zutil.h
)

IF(CMAKE_COMPILER_IS_GNUCXX AND UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF(CMAKE_COMPILER_IS_GNUCXX AND UNIX)

if(ENABLE_SOLUTION_FOLDERS)
	set_target_properties(mvgzlib PROPERTIES FOLDER "3rd party")
else(ENABLE_SOLUTION_FOLDERS)
	SET_TARGET_PROPERTIES(mvgzlib  PROPERTIES PROJECT_LABEL "(3rd party) mvgzlib")
endif(ENABLE_SOLUTION_FOLDERS)
