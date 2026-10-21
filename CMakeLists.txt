cmake_minimum_required(VERSION 3.20)
project(XSISelectFix LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "XSISelectFix must be built as a 64-bit Windows project")
endif()

add_library(XSISelectFix SHARED src/XSISelectFix.cpp)
target_compile_features(XSISelectFix PRIVATE cxx_std_17)
target_compile_definitions(XSISelectFix PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(XSISelectFix PRIVATE psapi)
set_target_properties(XSISelectFix PROPERTIES OUTPUT_NAME "XSISelectFix")

add_executable(XSISelectInjector src/XSISelectInjector.cpp)
target_compile_features(XSISelectInjector PRIVATE cxx_std_17)
target_compile_definitions(XSISelectInjector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
  foreach(target XSISelectFix XSISelectInjector)
    target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endforeach()
endif()

add_custom_command(TARGET XSISelectFix POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/startup/XSISelectFixLoader.py"
          "$<TARGET_FILE_DIR:XSISelectFix>/XSISelectFixLoader.py"
  COMMAND ${CMAKE_COMMAND} -E touch
          "$<TARGET_FILE_DIR:XSISelectFix>/XSISelectFix.enable"
  COMMENT "Preparing automatic-startup plug-in files")
