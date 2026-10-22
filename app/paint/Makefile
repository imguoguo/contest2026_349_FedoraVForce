############################################################################
# app/paint/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = paint
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_PAINT)

MAINSRC = paint_main.c
CSRCS   = digit.c digit_model.c digit_samples.c

# Recognition on TFLite Micro, same include paths and defines as
# apps/mlearning/tflite-micro builds it with

ifneq ($(CONFIG_TFLITEMICRO),)
CXXEXT    = .cc
CXXSRCS   = digit_tflm.cc
CSRCS    += digit_tflm_model.c
CXXFLAGS += ${INCDIR_PREFIX}$(APPDIR)/mlearning/tflite-micro/tflite-micro
CXXFLAGS += ${INCDIR_PREFIX}$(APPDIR)/math/gemmlowp/gemmlowp
CXXFLAGS += ${INCDIR_PREFIX}$(APPDIR)/math/ruy/ruy
CXXFLAGS += -DTFLITE_EMULATE_FLOAT -DTF_LITE_DISABLE_X86_NEON
CXXFLAGS += -DTF_LITE_STRIP_ERROR_STRINGS
CXXFLAGS += -DTFLITE_WITH_STABLE_ABI=0 -DTFLITE_USE_OPAQUE_DELEGATE=0
CXXFLAGS += -DTFLITE_SINGLE_ROUNDING=0
endif

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui

include $(APPDIR)/Application.mk
