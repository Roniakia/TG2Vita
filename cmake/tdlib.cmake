# TDLib is prepared/built separately because its generators must run on the host.
set(TDLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/tdlib" CACHE PATH "Pinned TDLib checkout")
set(TDLIB_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/tdlib-clang" CACHE PATH "Vita TDLib build")
if(NOT EXISTS "${TDLIB_BUILD_DIR}/libtdjson_static.a")
  message(FATAL_ERROR "Build native TDLib first with scripts/build-tdlib.sh")
endif()
file(GLOB_RECURSE TDLIB_ARCHIVES CONFIGURE_DEPENDS "${TDLIB_BUILD_DIR}/*.a")
add_library(vita_tdlib INTERFACE)
target_include_directories(vita_tdlib INTERFACE "${TDLIB_SOURCE_DIR}" "${TDLIB_BUILD_DIR}")
target_compile_definitions(vita_tdlib INTERFACE TDJSON_STATIC_DEFINE)
target_link_libraries(vita_tdlib INTERFACE -Wl,--start-group ${TDLIB_ARCHIVES} -Wl,--end-group ssl crypto z pthread)

# GCC emulated TLS tests the weak pthread_cancel symbol to detect threading.
# A static link must retain it or TLS silently becomes shared between workers.
target_link_options(vita_tdlib INTERFACE -Wl,-u,pthread_cancel -Wl,-u,pthread_once)

# The app exceeds Thumb's direct branch range. Legacy ld emits fixed-address
# veneers without relocation entries; these fail when hardware rebases the SELF.
# Use relative linker-generated veneers (independent of compile-time PIC/GOT).
target_link_options(vita_tdlib INTERFACE -Wl,--pic-veneer)
