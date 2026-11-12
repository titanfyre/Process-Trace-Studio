vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Process-Trace-Studio/releases/download/sdk-v0.1.0/process-trace-studio-sdk-0.1.0.tar.gz"
    FILENAME "process-trace-studio-sdk-0.1.0.tar.gz"
    SHA512 559fcf2c5f7bbdbb61fbe0e52591478daf085f67ce6dab4017d890d904cb879f9644fbb759ded384b021fde029943a53fc07f2a3799b03e74ad7b1926f04ec6f
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ProcessTraceStudioSDK CONFIG_PATH lib/cmake/ProcessTraceStudioSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
