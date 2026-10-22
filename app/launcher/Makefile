############################################################################
# app/launcher/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = launcher
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_LAUNCHER)

MAINSRC = launcher_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui

include $(APPDIR)/Application.mk
