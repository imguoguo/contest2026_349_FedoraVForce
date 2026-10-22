# app/monitor/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_MONITOR)
  nuttx_add_application(
    NAME monitor
    SRCS monitor_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
    STACKSIZE 16384)
endif()
