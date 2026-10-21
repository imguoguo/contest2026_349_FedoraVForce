############################################################################
# Contest 2026 team 349 launcher - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = launcher
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_349_LAUNCHER)

CSRCS   = launcher_digits.c launcher_model.c launcher_bench.c
CSRCS  += $(wildcard fonts/*.c)
MAINSRC = launcher_main.c

include $(APPDIR)/Application.mk
