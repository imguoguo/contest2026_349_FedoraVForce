############################################################################
# app/touch/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = touch
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_TOUCH)

MAINSRC = touch_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui

include $(APPDIR)/Application.mk
