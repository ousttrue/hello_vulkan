add_library(minizip STATIC src/ioapi.c src/miniunz.c src/minizip.c
                           src/mztools.c src/unzip.c src/zip.c)
target_compile_options(minizip PRIVATE -Wno-deprecated-non-prototype)
if(ANDROID)
  target_link_libraries(minizip PUBLIC z)
else()
  target_sources(minizip PRIVATE src/iowin32.c)
  target_link_libraries(minizip PUBLIC zlibstatic)
endif()
target_include_directories(minizip PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
