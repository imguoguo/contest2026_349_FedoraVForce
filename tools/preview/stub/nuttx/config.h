/****************************************************************************
 * tools/preview/stub/nuttx/config.h
 *
 * The app options the preview builds with.
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H
#define __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H

#ifndef CONFIG_LVX_LAUNCHER_APPS
#  define CONFIG_LVX_LAUNCHER_APPS "paint bench monitor"
#endif

#define CONFIG_LV_DEF_REFR_PERIOD 33
#define CONFIG_SMP_NCPUS 4

#endif /* __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H */
