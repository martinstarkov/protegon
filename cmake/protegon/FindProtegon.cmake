# Copy SDL2 dlls for windows machines and link library.

function(add_protegon_to TARGET_NAME)
  target_link_libraries(${PROJECT_NAME} protegon)
  
  if(WIN32)
      get_property(PROTEGON_EXTERNAL_DLLS_TEMP GLOBAL PROPERTY PROTEGON_EXTERNAL_DLLS)
      foreach(item IN LISTS PROTEGON_EXTERNAL_DLLS_TEMP)
          #message(STATUS "Found External DLL = ${item}")
          add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
              COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${item}" $<TARGET_FILE_DIR:${TARGET_NAME}>
          )
      endforeach()

  endif()
endfunction()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(PROTEGON DEFAULT_MSG)