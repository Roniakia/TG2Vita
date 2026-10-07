# Redirect only curl's CLOEXEC requests around the legacy libc ENOSYS stub.
# Nonblocking socket setup remains curl's native SO_NONBLOCK implementation.
set(VITA_TG_CURL_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/libcurl-vita-tg.a")
add_custom_command(OUTPUT "${VITA_TG_CURL_ARCHIVE}"
  COMMAND "${CMAKE_OBJCOPY}" --redefine-sym fcntl=vita_tg_curl_fcntl
    "${VITASDK}/arm-vita-eabi/lib/libcurl.a" "${VITA_TG_CURL_ARCHIVE}"
  DEPENDS "${VITASDK}/arm-vita-eabi/lib/libcurl.a"
  VERBATIM)
add_custom_target(vita_tg_curl_archive DEPENDS "${VITA_TG_CURL_ARCHIVE}")
add_library(vita_tg_curl STATIC IMPORTED)
set_target_properties(vita_tg_curl PROPERTIES IMPORTED_LOCATION "${VITA_TG_CURL_ARCHIVE}")
add_dependencies(vita_tg_curl vita_tg_curl_archive)
