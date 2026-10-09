# Contest 2026 team 349 launcher
if(CONFIG_LVX_USE_DEMO_CONTEST2026_349_LAUNCHER)
  file(GLOB FONT_SRCS ${CMAKE_CURRENT_LIST_DIR}/fonts/*.c)
  nuttx_add_application(
    NAME launcher
    SRCS launcher_main.c launcher_digits.c launcher_model.c launcher_bench.c
         ${FONT_SRCS}
    STACKSIZE 32768)
endif()
