include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.cmake")

cmrc_add_resource_library(resources-shader ALIAS rc::shader NAMESPACE shader WHENCE "${PROTEGON_SHADER_DIR}" ${PROTEGON_SHADERS} "${PROTEGON_SHADER_DIR}/manifest.json")