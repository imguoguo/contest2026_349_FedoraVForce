# app/digit/CMakeLists.txt
if(CONFIG_LVX_DIGIT)
  nuttx_add_library(lvx_digit)
  target_sources(lvx_digit PRIVATE digit.c digit_model.c digit_samples.c)

  # TFLite Micro, with the include paths and defines that
  # apps/mlearning/tflite-micro/CMakeLists.txt builds it with

  if(CONFIG_TFLITEMICRO)
    target_sources(lvx_digit PRIVATE digit_tflm.cc digit_tflm_model.c)
    target_include_directories(
      lvx_digit
      PRIVATE ${NUTTX_APPS_DIR}/mlearning/tflite-micro/tflite-micro
              ${NUTTX_APPS_DIR}/math/gemmlowp/gemmlowp
              ${NUTTX_APPS_DIR}/math/ruy/ruy)
    target_compile_definitions(
      lvx_digit
      PRIVATE TF_LITE_STATIC_MEMORY TF_LITE_DISABLE_X86_NEON
              TFLITE_WITH_STABLE_ABI=0 TFLITE_USE_OPAQUE_DELEGATE=0
              TFLITE_SINGLE_ROUNDING=0)
  endif()
endif()
