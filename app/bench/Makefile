############################################################################
# app/bench/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = bench
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_BENCH)

MAINSRC = bench_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_digit

include $(APPDIR)/Application.mk
