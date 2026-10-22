/****************************************************************************
 * tools/preview/stub/nuttx/config.h
 *
 * The app options the preview builds with.
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H
#define __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H

#define CONFIG_LVX_LAUNCHER_APPS "paint bench monitor touch snake " \
                                 "whackmole lvgl_benchmark " \
                                 "lvgl_widgets"

#ifndef CONFIG_LVX_LAUNCHER_PINNED
#  define CONFIG_LVX_LAUNCHER_PINNED "paint bench monitor"
#endif

#define CONFIG_SMP_NCPUS 4

#endif /* __TOOLS_PREVIEW_STUB_NUTTX_CONFIG_H */
