# app/touch/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_TOUCH)
  nuttx_add_application(
    NAME touch
    SRCS touch_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
    STACKSIZE 16384)
endif()
