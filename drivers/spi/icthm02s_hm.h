/*
 * Register definitions for the ICTHM02S Hall Mouse
 */
#define HM_DEBUG
/*Add by Erace.Ma@20091023, STE patch SPI driver*/
#ifdef HM_DEBUG
#define HM_SHOW_CONFIG
#endif
/*End by Erace.Ma@20091023*/

enum HM_DIR
{
  HM_U = 0,
  HM_R,
  HM_D,
  HM_L,
  HM_C,
  HM_N,
};
  
/*We get X,Y position from ICTHM02S Hall Mouse move data, -7~0~7, total 15 values for each.
  By using a mapping table to transform index to direction*/
static u8 dir_ary[15][15] = 
{
/*   -7    -6    -5    -4    -3    -2    -1     0     1     2     3     4     5     6     7 */  
  {HM_N, HM_N, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_N, HM_N}, //  7
  {HM_N, HM_N, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_N, HM_N}, //  6
  {HM_L, HM_L, HM_N, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_N, HM_R, HM_R}, //  5
  {HM_L, HM_L, HM_L, HM_N, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_U, HM_N, HM_R, HM_R, HM_R}, //  4
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, //  3
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, //  2
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_C, HM_C, HM_C, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, //  1
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_C, HM_C, HM_C, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, //  0
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_C, HM_C, HM_C, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, // -1
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, // -2
  {HM_L, HM_L, HM_L, HM_L, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_N, HM_R, HM_R, HM_R, HM_R}, // -3
  {HM_L, HM_L, HM_L, HM_N, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_N, HM_R, HM_R, HM_R}, // -4
  {HM_L, HM_L, HM_N, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_N, HM_R, HM_R}, // -5
  {HM_N, HM_N, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_N, HM_N}, // -6
  {HM_N, HM_N, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_D, HM_N, HM_N}  // -7
};

#ifdef HM_DEVFS
static int hallmouse_ioctl(struct inode *inode, struct file *instance, unsigned int cmd, unsigned long arg);
static int hallmouse_open(struct inode * inode, struct file * instance);
static int hallmouse_close(struct inode * inode, struct file * instance);
static ssize_t hallmouse_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t hallmouse_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos);
#endif
