############################################################################
# app/digit/Makefile
############################################################################

include $(APPDIR)/Make.defs

CSRCS = digit.c digit_model.c digit_samples.c

# TFLite Micro, same include paths and defines as
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

include $(APPDIR)/Application.mk
