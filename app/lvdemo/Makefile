############################################################################
# app/lvdemo/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = lvdemo
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 65536
MODULE    = $(CONFIG_LVX_USE_DEMO_LVDEMO)

MAINSRC = lvdemo_main.c
CSRCS   = lvdemo_snake.c lvdemo_whackmole.c

DEMOS = $(APPDIR)/packages/demos

CFLAGS += ${INCDIR_PREFIX}$(DEMOS)/contest2026_349_ui
CFLAGS += ${INCDIR_PREFIX}$(DEMOS)/snake_game
CFLAGS += ${INCDIR_PREFIX}$(DEMOS)/Whackmole
CFLAGS += ${INCDIR_PREFIX}$(DEMOS)/Whackmole/pic

include $(APPDIR)/Application.mk
