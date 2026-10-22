# app/paint/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_PAINT)
  nuttx_add_application(
    NAME paint
    SRCS paint_main.c digit.c digit_model.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
    STACKSIZE 32768)
endif()
