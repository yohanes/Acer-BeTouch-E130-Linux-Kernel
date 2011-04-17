#ifndef _CM3623_H_
#define _CM3623_H_

#define CM3623_INT                0x20
#define CM3623_INT_ADDR           0x03
///#define CM3623_INITIAL_ADDR       0x22
#define CM3623_INITIAL_INT_DATA    0x10  /* interrupt mode */ 
#define CM3623_INITIAL_POL_DATA    0x20  /* polling mode */
///#define CM3623_PS_CMD             0xB0
///#define CM3623_PS_THD_CMD         0xB2
///#define CM3623_ALS_DATA_LSB       0x23
///#define CM3623_ALS_DATA_MSB       0x21
///#define CM3623_ALS_CMD            0x20

#define CM3623_ALS_GAIN1_OFFSET   7  /* Gain setting for Sensitiviry Range Selection */  
#define CM3623_ALS_GAIN0_OFFSET   6  
#define CM3623_ALS_THD1_OFFSET    5  /* Threshold window setting */
#define CM3623_ALS_THD0_OFFSET    4
#define CM3623_ALS_IT1_OFFSET     3  /* Integration Time setting */
#define CM3623_ALS_IT0_OFFSET     2
#define CM3623_ALS_WDM_OFFSET     1  /* Data mode setting, 1 for byte mode, 0 for word mode */
#define CM3623_ALS_SD_OFFSET      0  /* Shutdown mode setting, 1 for SD enable, 0 for SD disable */

#define CM3623_PS_DR1_OFFSET      7  /* IR LED on/off duty ratio setting */
#define CM3623_PS_DR0_OFFSET      6
#define CM3623_PS_IT1_OFFSET      5  /* Integration Time setting */
#define CM3623_PS_IT0_OFFSET      4
#define CM3623_PS_INTALS_OFFSET   3  /* ALS interruption setting */
#define CM3623_PS_INTPS_OFFSET    2  /* PS interruption setting */
#define CM3623_PS_RES_OFFSET      1  /* reserved, always 0 */
#define CM3623_PS_SD_OFFSET       0  /* Shutdown mode setting, 1 for SD enable, 0 for SD disable */

#define CM3623_INITIAL_ADDR_7BITS         /*0x11*/  0x49
#define CM3623_ALS_WRITE_ADDR_7BITS       /*0x10*/  0x48
#define CM3623_ALS_READ_MSB_ADDR_7BITS    /*0x10*/  0x48
#define CM3623_ALS_READ_LSB_ADDR_7BITS    /*0x11*/  0x49
#define CM3623_PS_WRITE_ADDR_7BITS        /*0x58*/  0x78
#define CM3623_PS_READ_ADDR_7BITS         /*0x58*/  0x78
#define CM3623_PS_THD_ADDR_7BITS          /*0x59*/  0x79
#define CM3623_INTERRUPT_ADDR_7BITS       0x0C
#endif /* _CM3623_H_ */

