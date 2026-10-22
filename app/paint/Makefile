############################################################################
# app/paint/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = paint
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_PAINT)

MAINSRC = paint_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_digit

include $(APPDIR)/Application.mk
