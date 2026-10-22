# app/paint/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_PAINT)
  set(SRCS paint_main.c digit.c digit_model.c digit_samples.c)
  set(INCDIR ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui)
  set(DEFS)

  # Recognition on TFLite Micro, with the include paths and defines that
  # apps/mlearning/tflite-micro/CMakeLists.txt builds it with

  if(CONFIG_TFLITEMICRO)
    list(APPEND SRCS digit_tflm.cc digit_tflm_model.c)
    list(APPEND INCDIR ${NUTTX_APPS_DIR}/mlearning/tflite-micro/tflite-micro
         ${NUTTX_APPS_DIR}/math/gemmlowp/gemmlowp
         ${NUTTX_APPS_DIR}/math/ruy/ruy)
    list(APPEND DEFS TF_LITE_STATIC_MEMORY TF_LITE_DISABLE_X86_NEON
         TFLITE_WITH_STABLE_ABI=0 TFLITE_USE_OPAQUE_DELEGATE=0
         TFLITE_SINGLE_ROUNDING=0)
  endif()

  nuttx_add_application(
    NAME paint
    SRCS ${SRCS}
    INCLUDE_DIRECTORIES ${INCDIR}
    DEFINITIONS ${DEFS}
    STACKSIZE 32768)
endif()
