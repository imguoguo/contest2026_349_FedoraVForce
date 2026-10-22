############################################################################
# app/monitor/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = monitor
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_MONITOR)

MAINSRC = monitor_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui

include $(APPDIR)/Application.mk
