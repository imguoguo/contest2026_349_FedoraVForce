/****************************************************************************
 * tools/preview/stub/nuttx/arch.h
 *
 * Perf counter in microseconds of virtual time.
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_NUTTX_ARCH_H
#define __TOOLS_PREVIEW_STUB_NUTTX_ARCH_H

#include <time.h>

clock_t up_perf_gettime(void);
unsigned long up_perf_getfreq(void);

#endif /* __TOOLS_PREVIEW_STUB_NUTTX_ARCH_H */
