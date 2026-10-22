/****************************************************************************
 * tools/preview/stub/malloc.h
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_MALLOC_H
#define __TOOLS_PREVIEW_STUB_MALLOC_H

#include <stdlib.h>

struct mallinfo
{
  int arena;
  int ordblks;
  int mxordblk;
  int uordblks;
  int fordblks;
};

struct mallinfo mallinfo(void);
void *memalign(size_t align, size_t size);

#endif /* __TOOLS_PREVIEW_STUB_MALLOC_H */
