/**
 * @brief open and read a 9p ADC file
 *
 * @param fileid  0 temperature or gpacd3 (s32 micro °c)
 *                1 vbat or gpadc1 not in burst (s32 micro volt)
 *                2 accessory or gpadc4 (s32 micro ohm)
 *                3 battery pack or gpadc5 (s32 micro ohm)
 *                4 gpadc1 (s32 raw value)
 *                5 gpadc2 (s32 raw value)
 *                6 gpadc3 (s32 raw value)
 *                7 gpadc4 (s32 raw value)
 *                8 gpadc5 (s32 raw value)
 * @param void(*callback) : function called after measurement completion
 *                          (unsigned long) parameter is measurment result.
  * @return
 */
#define FID_TEMP 0
#define FID_VBAT 1
#define FID_ACC 2
#define FID_BATPACK 3
#define FID_GPADC1 4
#define FID_GPADC2 5
#define FID_GPADC3 6
#define FID_GPADC4 7
#define FID_GPADC5 8
extern int adc_aread(int fileid,void (*callback)(unsigned long));
