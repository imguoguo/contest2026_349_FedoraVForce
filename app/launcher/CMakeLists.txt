# app/launcher/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_LAUNCHER)
  nuttx_add_application(
    NAME launcher
    SRCS launcher_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
    STACKSIZE 32768)
endif()
