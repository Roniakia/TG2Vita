# Keep the installed SDK untouched; apply policy compatibility in compiler probes too.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(NOT DEFINED ENV{VITASDK})
  message(FATAL_ERROR "Set VITASDK to your installed VitaSDK directory.")
endif()
include("$ENV{VITASDK}/share/vita.toolchain.cmake")

# Wrapper sets host dylib paths after macOS protected build tools launch.
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../scripts/vita-cxx.sh" CACHE FILEPATH "Vita C++ compiler" FORCE)

set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../scripts/vita-cc.sh" CACHE FILEPATH "Vita C compiler" FORCE)

if(DEFINED ENV{VITA_TG_USE_CLANG} AND "$ENV{VITA_TG_USE_CLANG}" STREQUAL "1")
  set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../scripts/vita-clang-cxx.sh" CACHE FILEPATH "Vita Clang C++ compiler" FORCE)
  set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../scripts/vita-clang-cc.sh" CACHE FILEPATH "Vita Clang C compiler" FORCE)
endif()
