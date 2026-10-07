cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vita-toolchain.cmake" CACHE FILEPATH "Vita toolchain")
endif()
project(vita_tg VERSION 0.6.2 LANGUAGES CXX C)
# Compatibility for older installed SDK packaging helpers under CMake 4.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include("${VITASDK}/share/vita.cmake" REQUIRED)
# Packaging subprocesses need the same macOS host-library compatibility.
set(VITA_MAKE_FSELF "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vita-host-tool.sh;vita-make-fself")
set(VITA_PACK_VPK "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vita-host-tool.sh;vita-pack-vpk")
set(VITA_APP_NAME "Vita TG")
set(VITA_TITLEID "VTGC00001" CACHE STRING "Development title ID (nine characters)")
set(VITA_VERSION "00.62")
set(VITA_TG_VERSION_SUFFIX "-beta.1" CACHE STRING "Release version suffix; empty after user acceptance")
configure_file(src/version.hpp.in generated/version.hpp @ONLY)
include(cmake/tdlib.cmake)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/jansson-2.15.1/CMakeLists.txt")
  message(FATAL_ERROR "Prepare Jansson with scripts/setup-jansson.sh first")
endif()
set(JANSSON_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(JANSSON_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JANSSON_WITHOUT_TESTS ON CACHE BOOL "" FORCE)
set(JANSSON_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(.deps/jansson-2.15.1 build-jansson)
set_target_properties(jansson PROPERTIES POSITION_INDEPENDENT_CODE OFF)
add_executable(vita_tg src/update/updater.cpp src/main.cpp src/telegram/auth.cpp src/telegram/credentials.cpp src/telegram/app_credentials.cpp src/telegram/ime.cpp src/ui/login_form.cpp src/ui/app_view.cpp src/ui/media_viewer.cpp src/ui/media_image.cpp)
target_include_directories(vita_tg PRIVATE src)
target_compile_features(vita_tg PRIVATE cxx_std_17)
target_compile_options(vita_tg PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(vita_tg PRIVATE -Wl,--gc-sections)
target_link_libraries(vita_tg PRIVATE
  vita_tdlib jansson vita2d SceDisplay_stub SceGxm_stub SceSysmodule_stub SceCtrl_stub
  SceAppUtil_stub SceNet_stub SceNetCtl_stub ScePgf_stub SceCommonDialog_stub SceAppMgr_stub SceTouch_stub
  SceIofilemgr_stub SceKernelThreadMgr_stub SceKernelDmacMgr_stub
  SceAvPlayer_stub SceAudio_stub png jpeg curl ssl crypto zstd z m)
vita_create_self(vita_tg.self vita_tg)
vita_create_vpk(vita_tg.vpk ${VITA_TITLEID} vita_tg.self
  VERSION ${VITA_VERSION} NAME ${VITA_APP_NAME}
  FILE assets/icon0.png sce_sys/icon0.png
  FILE assets/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/template.xml sce_sys/livearea/contents/template.xml
  FILE assets/certs/cacert.pem assets/certs/cacert.pem
  FILE licenses/Jansson.txt licenses/Jansson.txt
  FILE licenses/TDLib.txt licenses/TDLib.txt
  FILE licenses/Twemoji.txt licenses/Twemoji.txt
  FILE assets/emoji/atlas.png assets/emoji/atlas.png)

# Vita3K accepts true-color artwork that Sony's installer rejects on hardware.
find_program(VITA_TG_HOST_PYTHON NAMES python3 NO_CMAKE_FIND_ROOT_PATH)
if(NOT VITA_TG_HOST_PYTHON)
  message(FATAL_ERROR "Python 3 is required to validate LiveArea assets")
endif()
# Values remain outside tracked source and compiler command-line definitions.
option(VITA_TG_EMBED_APP_CREDENTIALS "Embed app-owned credentials for an install-only production client" ON)
set(APP_CREDENTIAL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/application_credentials.hpp")
set(APP_CREDENTIAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/secrets/telegram.conf")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/credential_mode.txt" CONTENT "${VITA_TG_EMBED_APP_CREDENTIALS}")
set(APP_CREDENTIAL_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/credential_mode.txt" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-app-credentials.py" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install-config.py")
if(VITA_TG_EMBED_APP_CREDENTIALS)
  if(NOT EXISTS "${APP_CREDENTIAL_SOURCE}")
    message(FATAL_ERROR "Fill secrets/telegram.conf before building the production client; config-only builds can set VITA_TG_EMBED_APP_CREDENTIALS=OFF")
  endif()
  list(APPEND APP_CREDENTIAL_DEPENDS "${APP_CREDENTIAL_SOURCE}")
endif()
add_custom_command(OUTPUT "${APP_CREDENTIAL_HEADER}"
  COMMAND "${VITA_TG_HOST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-app-credentials.py"
    --source "${APP_CREDENTIAL_SOURCE}" --output "${APP_CREDENTIAL_HEADER}" --enabled "${VITA_TG_EMBED_APP_CREDENTIALS}"
  DEPENDS ${APP_CREDENTIAL_DEPENDS}
  VERBATIM)
target_sources(vita_tg PRIVATE "${APP_CREDENTIAL_HEADER}")
target_include_directories(vita_tg PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_target(check_livearea_assets
  COMMAND "${VITA_TG_HOST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-livearea.py" "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  VERBATIM)
add_dependencies(vita_tg.vpk-vpk check_livearea_assets)

add_custom_target(check_link_veneers
  COMMAND "${VITA_TG_HOST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-link-veneers.py" "$<TARGET_FILE:vita_tg>"
  DEPENDS vita_tg
  VERBATIM)
add_dependencies(vita_tg.vpk-vpk check_link_veneers)
