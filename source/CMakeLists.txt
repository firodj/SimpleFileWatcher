set(SFW_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(
	GLOB_RECURSE SFW_INCLUDES 
	"${SFW_INCLUDE_DIR}/*.h"
	"${SFW_INCLUDE_DIR}/*.h"
)

if (WIN32)
	set(
		SFW_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcher.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcherWin32.cpp"
	)
ELSEIF(UNIX)
	set(
		SFW_SOURCES 
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcher.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcherLinux.cpp"
	)
ELSEIF(APPLE)
	set(
		SFW_SOURCES 
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcher.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/FileWatcherOSX.cpp"
	)
ENDIF()

add_library(sfw_lib STATIC ${SFW_SOURCES} ${SFW_INCLUDES})
target_include_directories(
	sfw_lib
	PUBLIC
	${SFW_INCLUDE_DIR}
)