/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/src/vsai_logicpi_a1_boardinit.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>

#include <syslog.h>

#include <arch/board/board.h>

#ifdef CONFIG_FS_PROCFS
#  include <nuttx/fs/fs.h>
#endif

#ifdef CONFIG_S6_MIPI_DSI
#  include <nuttx/video/fb.h>
#endif

#ifdef CONFIG_S6_I2C
#  include "s6_i2c.h"
#endif

#ifdef CONFIG_S6_ETH
#  include "s6_eth.h"
#endif

#ifdef CONFIG_SMP
#  include <nuttx/cache.h>
#  include "arm64_internal.h"
#endif

#if defined(CONFIG_SMP) && !defined(CONFIG_BOARD_EARLY_INITIALIZE)
#  error "SMP needs CONFIG_BOARD_EARLY_INITIALIZE to flush the idle stacks"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void s6_board_initialize(void)
{
  /* DDR, clocks, pin mux and UART are initialized by BL31/U-Boot. */
}

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
#ifdef CONFIG_SMP
  /* CPU1-3 push their first stack frames with the D-cache off and pop
   * them with it on, so the lines CPU0 wrote there in up_initial_state()
   * must not survive in any cache.  The set/way flush in nx_smp_start()
   * is not enough on S6, clean and invalidate them by VA to PoC.
   */

  up_flush_dcache((uintptr_t)g_cpu_idlestackalloc[1],
                  (uintptr_t)g_cpu_idlestackalloc +
                  sizeof(g_cpu_idlestackalloc));
#endif
}
#endif

/* This tree's NSH enables CONFIG_BOARDCTL (mkrd), and boards/boardctl.c
 * references board_app_initialize() unconditionally.
 */

#ifdef CONFIG_BOARDCTL
int board_app_initialize(uintptr_t arg)
{
  UNUSED(arg);
  return OK;
}
#endif

/* BL31 implements PSCI SYSTEM_RESET, which brings the board back to
 * U-Boot.
 */

#ifdef CONFIG_BOARDCTL_RESET
int board_reset(int status)
{
  UNUSED(status);
  up_systemreset();
  return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret = OK;

#ifdef CONFIG_FS_PROCFS
  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_S6_MIPI_DSI
  ret = fb_register(0, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: fb_register() failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_S6_I2C)
  struct i2c_master_s *i2c = s6_i2cbus_initialize();

#ifdef CONFIG_I2C_DRIVER
  ret = i2c_register(i2c, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: i2c_register(0) failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_VSAI_LOGICPI_A1_TOUCH
  ret = vsai_logicpi_a1_touch_register("/dev/input0", i2c);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: touch register failed: %d\n", ret);
    }
#endif
#endif

#ifdef CONFIG_S6_ETH
  ret = s6_eth_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: s6_eth_initialize() failed: %d\n", ret);
    }
#endif

  UNUSED(ret);
}
#endif
