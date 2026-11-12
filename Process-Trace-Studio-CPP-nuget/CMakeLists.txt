cmake_minimum_required(VERSION 3.24)
project(ProcessTraceStudioSDK VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanProcessTraceStudio STATIC
  "sdk/SDK/ProcessTrace.cpp"
)
add_library(ProcessTraceStudioSDK::sdk ALIAS TitanProcessTraceStudio)
set_target_properties(TitanProcessTraceStudio PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanProcessTraceStudio PUBLIC cxx_std_17)
target_compile_definitions(TitanProcessTraceStudio PUBLIC )
target_compile_definitions(TitanProcessTraceStudio PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanProcessTraceStudio PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanProcessTraceStudio PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanProcessTraceStudio PUBLIC advapi32 comctl32 comdlg32 crypt32 gdi32 newdev ole32 setupapi shell32 user32 wintrust)
install(TARGETS TitanProcessTraceStudio EXPORT ProcessTraceStudioSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/process-trace-studio)
install(EXPORT ProcessTraceStudioSDKTargets NAMESPACE ProcessTraceStudioSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProcessTraceStudioSDK)
configure_file("ProcessTraceStudioSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/ProcessTraceStudioSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ProcessTraceStudioSDKConfigVersion.cmake" VERSION 0.1.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ProcessTraceStudioSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/ProcessTraceStudioSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProcessTraceStudioSDK)
