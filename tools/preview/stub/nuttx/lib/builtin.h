/****************************************************************************
 * tools/preview/stub/nuttx/lib/builtin.h
 ****************************************************************************/

#ifndef __TOOLS_PREVIEW_STUB_NUTTX_LIB_BUILTIN_H
#define __TOOLS_PREVIEW_STUB_NUTTX_LIB_BUILTIN_H

typedef int (*main_t)(int argc, char *argv[]);

struct builtin_s
{
  const char *name;
  int priority;
  int stacksize;
  main_t main;
};

int builtin_isavail(const char *name);
const char *builtin_getname(int index);
const struct builtin_s *builtin_for_index(int index);
int task_create(const char *name, int priority, int stack_size,
                main_t entry, char * const argv[]);

#endif /* __TOOLS_PREVIEW_STUB_NUTTX_LIB_BUILTIN_H */
