# app/lvdemo/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_LVDEMO)
  set(DEMOS ${NUTTX_APPS_DIR}/packages/demos)
  nuttx_add_application(
    NAME lvdemo
    SRCS lvdemo_main.c lvdemo_snake.c lvdemo_whackmole.c
    INCLUDE_DIRECTORIES ${DEMOS}/contest2026_349_ui ${DEMOS}/snake_game
                        ${DEMOS}/Whackmole ${DEMOS}/Whackmole/pic
    STACKSIZE 65536)
endif()
