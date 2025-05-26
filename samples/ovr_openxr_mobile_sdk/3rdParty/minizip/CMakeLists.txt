add_library(
  minizip STATIC
  src/ioapi.c
  src/iowin32.c
  src/miniunz.c
  src/minizip.c
  src/mztools.c
  src/unzip.c
  src/zip.c)
target_include_directories(minizip PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(minizip PUBLIC zlibstatic)
