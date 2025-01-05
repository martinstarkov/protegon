# Search for protegon source and header files
file(GLOB_RECURSE PROTEGON_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

file(GLOB_RECURSE PROTEGON_CORE_SHADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/shader/core/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/src/shader/core/*.vert")

file(GLOB_RECURSE PROTEGON_ES_SHADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/shader/es/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/src/shader/es/*.vert")

file(GLOB_RECURSE PROTEGON_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")