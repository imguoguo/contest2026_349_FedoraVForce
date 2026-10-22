# app/bench/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_BENCH)
  nuttx_add_application(
    NAME bench
    SRCS bench_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
    STACKSIZE 32768)
endif()
