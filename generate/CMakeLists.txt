cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

# Generate files for mime type <-> extension conversions
# DEPENDS ON: gperf grep bash/powershell

file(MAKE_DIRECTORY auto)

set(TDMIME_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/auto/mime_type_to_extension.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/auto/extension_to_mime_type.cpp
)
set(TDMIME_AUTO
  ${TDMIME_SOURCE}
  PARENT_SCOPE
)

add_custom_target(tdmime_auto DEPENDS ${TDMIME_SOURCE})

if (NOT CMAKE_CROSSCOMPILING)
  set(GPERF_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/auto/mime_type_to_extension.gperf
    ${CMAKE_CURRENT_SOURCE_DIR}/auto/extension_to_mime_type.gperf
  )

  set(GPERF_GEN_SOURCE generate_mime_types_gperf.cpp)

  add_executable(generate_mime_types_gperf ${GPERF_GEN_SOURCE})

  add_custom_command(
    OUTPUT ${GPERF_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND generate_mime_types_gperf mime_types.txt ${GPERF_FILES}
    DEPENDS generate_mime_types_gperf mime_types.txt
  )

  if (WIN32)
    set(MimeToExtentionCmd powershell -ExecutionPolicy ByPass \" & gperf -m100 --output-file=auto/mime_type_to_extension.cpp auto/mime_type_to_extension.gperf\")
  else()
    set(MimeToExtentionCmd gperf -m100 auto/mime_type_to_extension.gperf | grep -v __gnu_inline__ > auto/mime_type_to_extension.cpp)
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/auto/mime_type_to_extension.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${MimeToExtentionCmd}
    DEPENDS auto/mime_type_to_extension.gperf
  )

  if (WIN32)
    set(ExtentionToMimeCmd powershell -ExecutionPolicy ByPass \" & gperf -m100 --output-file=auto/extension_to_mime_type.cpp auto/extension_to_mime_type.gperf\")
  else()
    set(ExtentionToMimeCmd gperf -m100 auto/extension_to_mime_type.gperf | grep -v __gnu_inline__ > auto/extension_to_mime_type.cpp)
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/auto/extension_to_mime_type.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${ExtentionToMimeCmd}
    DEPENDS auto/extension_to_mime_type.gperf
  )
endif()
