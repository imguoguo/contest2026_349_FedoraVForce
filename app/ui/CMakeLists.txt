# app/ui/CMakeLists.txt
if(CONFIG_LVX_UI)
  nuttx_add_library(lvx_ui)
  file(GLOB FONTS ${CMAKE_CURRENT_LIST_DIR}/fonts/*.c)
  target_sources(lvx_ui PRIVATE lvx_ui.c ${FONTS})
endif()
