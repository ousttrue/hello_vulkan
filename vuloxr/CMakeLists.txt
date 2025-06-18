set(TARGET_NAME vuloxr)
add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME}
                           INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(${TARGET_NAME} INTERFACE magic_enum::magic_enum)
