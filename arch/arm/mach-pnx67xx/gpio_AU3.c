/*
 *  linux/arch/arm/mach-pnx67xx/gpio_AU3.c
 *
 * Description: ACER gpio specific setting
 *
 * Copyright (C) 2008 NXP Semiconductors, Le Mans
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 **********************************************************************/
/* ACER Jen chang, 2010/03/18, IssueKeys:, add ACER gpio mux setting { */
#include <linux/sched.h>
#include <mach/gpio.h>
#include <mach/scon.h>

/**
 * SCON initial settings
 * Allows to define the PIN multiplexing for all the platform (Linux and Modem)
 */
struct pnx_scon_config pnx_scon_init_config[ SCON_REGISTER_NB] =
{
 {
 (void __iomem*) SCON_SYSMUX0_REG,
 0 |
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A0  & 0xF)))| /* GPIOA0 -         FM_IRQ                                  */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A1  & 0xF)))| /* RXD2 -           BT_UART2_RXD                            */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A2  & 0xF)))| /* CTS2_n -         BT_UART2_CTSn                           */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A3  & 0xF)))| /* GPIOA3 -         KEY_LED_CTRL                            */
 ( GPIO_MODE_MUX3 << (2 * (GPIO_A4  & 0xF)))| /* SIMOFF_n_copy -  SIM_OFF                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A5  & 0xF)))| /* GPIOA5 -         AGPS_SPI2_SS_N_GPIOA5                   */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A6  & 0xF)))| /* PWM1 -           BKLIGHT_LCD_ON_OFF                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A7  & 0xF)))| /* GPIOA7 -         BT_POWERDN                              */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A8  & 0xF)))| /* GPIOA8 -         DVM_SEL2_GPIO5                          */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A9  & 0xF)))| /* GPIOA9 -         DVM_SEL2_GPIO6                          */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A10 & 0xF)))| /* RTS2_n -         BT_UART2_RTSn                           */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A11 & 0xF)))| /* TXD2 -           BT_UART2_TXD                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A12 & 0xF)))| /* GPIOA12 -        OVP_FAULT                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A13 & 0xF)))| /* GPIOA13 -        SPI1_CS                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A14 & 0xF)))| /* GPIOA14 -        POWER_IRQ                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A15 & 0xF)))  /* GPIOA15 -        RF_ANT_DET                              */
 },
 {
 (void __iomem*) SCON_SYSMUX1_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A16 & 0xF)))| /* FCIDATA3 -       SD_DATA3                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A17 & 0xF)))| /* GPIOA17 -        HSDET                                   */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A18 & 0xF)))| /* FCIDATA2 -       SD_DATA2                                */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A19 & 0xF)))| /* FCIDATA1 -       SD_DATA1                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A20 & 0xF)))| /* GPIOA20 -        HM_INT_GPIOA20                          */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A21 & 0xF)))| /* GPIOA21 -        RF3G_REG_ON                             */
 ( GPIO_MODE_MUX2 << (2 * (GPIO_A22 & 0xF)))| /* RF3GGPO14 -      PDNB_2G                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A23 & 0xF)))| /* GPIOA23 -        RF2G_RESETn                             */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A24 & 0xF)))| /* GPIOA24 -        BT_HOST_WAKE                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A25 & 0xF)))| /* GPIOA25 -        TP_IRQ                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A26 & 0xF)))| /* GPIOA26 -        CAM_MCLK_SW                             */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A27 & 0xF)))| /* SCLK1 -          SPI1_CLK      (SPI1)                    */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_A28 & 0xF)))| /* SDATIO1 -        SPI1_DATA_OUT (SPI1)                    */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A29 & 0xF)))| /* GPIOA29 -        CAM_RESET2                              */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_A30 & 0xF)))| /* GPIOA30 -        AGPS_LDO1V2_EN_GPIOA30                  */
 ( GPIO_MODE_MUX2 << (2 * (GPIO_A31 & 0xF)))  /* SDATIN1 -        SPI1_DATA_I   (SPI1)                    */
 },
 {
 (void __iomem*) SCON_SYSMUX2_REG,
 0 |
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B0  & 0xF)))| /* DD -             BT_DD_OUT                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B1  & 0xF)))| /* DU -             BT_DU_IN                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B2  & 0xF)))| /* FSC -            BT_FSC_FSYNC                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B3  & 0xF)))| /* DCL -            BT_DCL_CLK                              */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_B4  & 0xF)))| /* GPIOB4 -         HM_EN_GPIOB4                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B5  & 0xF)))| /* TXD1 -           UART_TXD1                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B6  & 0xF)))| /* NFI_RDY -        NFI_RDY                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B7  & 0xF)))| /* VDE_EOFI -       VDE_EOFI_TE                             */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B8  & 0xF)))| /* RFEN0 -          RFEN0                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B9  & 0xF)))| /* FCICMD -         SD_CMD                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B10 & 0xF)))| /* FCICLK -         SD_CLK                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B11 & 0xF)))| /* FCIDATA0 -       SD_DATA0                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B12 & 0xF)))| /* RFSIG6 -         BROADCAST_TX_BLANKING                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B13 & 0xF)))| /* RFSIG7 -         GSM_PA_BAND_1                           */
 ( GPIO_MODE_MUX3 << (2 * (GPIO_B14 & 0xF)))| /* CLK32O -         NC                                      */                                                                             
 ( GPIO_MODE_MUX0 << (2 * (GPIO_B15 & 0xF)))  /* RFDATA -         RFDATA                                  */
 },
 {
 (void __iomem*) SCON_SYSMUX3_REG,
 0
 },
 {
 (void __iomem*) SCON_SYSMUX4_REG,
 0 |
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C0  & 0xF)))| /* VDE_D0 -         VDE_D_0                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C1  & 0xF)))| /* VDE_D1 -         VDE_D_1                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C2  & 0xF)))| /* VDE_D2 -         VDE_D_2                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C3  & 0xF)))| /* VDE_D3 -         VDE_D_3                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C4  & 0xF)))| /* VDE_D4 -         VDE_D_4                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C5  & 0xF)))| /* VDE_D5 -         VDE_D_5                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C6  & 0xF)))| /* VDE_D6 -         VDE_D_6                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C7  & 0xF)))| /* VDE_D7 -         VDE_D_7                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C8  & 0xF)))| /* VDE_D8 -         VDE_D_8                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C9  & 0xF)))| /* VDE_A0 -         VDE_A0_RS                               */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_C10 & 0xF)))| /* GPIOC10 -        BT_WAKE                                 */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C11 & 0xF)))| /* VDE_CS0_n -      VDE_CS0_n                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C12 & 0xF)))| /* VDE_WR_n -       VDE_WR_n                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C13 & 0xF)))| /* VDE_RD_n -       VDE_RD_n                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C14 & 0xF)))| /* KCOL0 -          KCOL0                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C15 & 0xF)))  /* KCOL1 -          KCOL1                                   */
 },
 {
 (void __iomem*) SCON_SYSMUX5_REG,
 0 |
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C16 & 0xF)))| /* KCOL2 -          KCOL2                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C17 & 0xF)))| /* KCOL3 -          KCOL3                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C18 & 0xF)))| /* KCOL4 -          KCOL4                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C19 & 0xF)))| /* KROW0 -          KROW0                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C20 & 0xF)))| /* KROW1 -          KROW1                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C21 & 0xF)))| /* KROW2 -          KROW2                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C22 & 0xF)))| /* KROW3 -          KROW3                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C23 & 0xF)))| /* KROW4 -          KROW4                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C24 & 0xF)))| /* RF3GSPIEN0 -     RF3GSPIEN                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C25 & 0xF)))| /* RF3GSPIDATA -    RF3GSPIDATA                             */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C26 & 0xF)))| /* RF3GSPICLK -     RF3GSPICLK                              */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C27 & 0xF)))| /* RFSM_OUT0 -      ANT_SEL0                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C28 & 0xF)))| /* RFSM_OUT1 -      ANT_SEL1                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C29 & 0xF)))| /* RFSM_OUT2 -      ANT_SEL2                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C30 & 0xF)))| /* SDA2 -           CAM_FM_I2C2_SDA                         */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_C31 & 0xF)))  /* SCL2 -           CAM_FM_I2C2_SCL                         */
 },
 {
 (void __iomem*) SCON_SYSMUX6_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D0  & 0xF)))| /* GPIOD0 -         CAM_PWRDN1                              */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D1  & 0xF)))| /* CAMDATA0 -       CAM_D0                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D2  & 0xF)))| /* CAMDATA1 -       CAM_D1                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D3  & 0xF)))| /* CAMDATA2 -       CAM_D2                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D4  & 0xF)))| /* CAMDATA3 -       CAM_D3                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D5  & 0xF)))| /* CAMDATA4 -       CAM_D4                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D6  & 0xF)))| /* CAMDATA5 -       CAM_D5                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D7  & 0xF)))| /* CAMDATA6 -       CAM_D6                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D8  & 0xF)))| /* CAMDATA7 -       CAM_D7                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D9  & 0xF)))| /* CAMDATA8 -       GND                                     */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D10 & 0xF)))| /* CAMDATA9 -       GND                                     */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D11 & 0xF)))| /* CAMVS -          CAM_VS                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D12 & 0xF)))| /* CAMHS -          CAM_HS                                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D13 & 0xF)))| /* CAMCLKI -        CAM_PCLK                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D14 & 0xF)))| /* CAMCLKO -        CAM_MCLK                                */
 ( GPIO_MODE_MUX2 << (2 * (GPIO_D15 & 0xF)))  /* VDDC2EN -        3G_MODEM_ON_OFF                         */
 },
 {
 (void __iomem*) SCON_SYSMUX7_REG,
 0 |
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D16 & 0xF)))| /* RF3GGPO9 -       WCDMA_PA_ON0                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D17 & 0xF)))| /* RF3GGPO8 -       WCDMA_PA_ON1                            */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D18 & 0xF)))| /* GPIOD18 -        SD_CARD_DET                             */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D19 & 0xF)))| /* GPIOD19 -        SLIDE_DET                               */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D20 & 0xF)))| /* RXD1 -           UART_RXD1                               */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D21 & 0xF)))| /* GPIOD21 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D22 & 0xF)))| /* GPIOD22 -        OVP_EN_GPIOA17                          */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D23 & 0xF)))| /* GPIOD23 -        CAM_RESET1                              */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D24 & 0xF)))| /* GPIOD24 -        CCSET_SEL                               */
 ( GPIO_MODE_MUX2 << (2 * (GPIO_D25 & 0xF)))| /* RFSIG3 -         MODE_SELECT_1                           */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D26 & 0xF)))| /* RF3GGPO6 -       PA_VMODE2_1                             */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D27 & 0xF)))| /* RF3GGPO7 -       WCDMA_PA_ON2                            */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D28 & 0xF)))| /* RF3GGPO5 -       RF3GRESETN_1                            */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_D29 & 0xF)))| /* GPIOD29 -        USB_SUSPEND                             */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D30 & 0xF)))| /* RF3GGPO4 -       NC                                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_D31 & 0xF)))  /* RFCLK -          RFCLK                                   */
 },
{
 (void __iomem*) SCON_SYSMUX8_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E0  & 0xF)))| /* GPIOE0 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E1  & 0xF)))| /* GPIOE1 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E2  & 0xF)))| /* GPIOE2 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E3  & 0xF)))| /* GPIOE3 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E4  & 0xF)))| /* GPIOE4 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E5  & 0xF)))| /* GPIOE5 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E6  & 0xF)))| /* GPIOE6 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E7  & 0xF)))| /* GPIOE7 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E8  & 0xF)))| /* GPIOE8 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E9  & 0xF)))| /* GPIOE9 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E10 & 0xF)))| /* GPIOE10 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E11 & 0xF)))| /* GPIOE11 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E12 & 0xF)))| /* GPIOE12 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E13 & 0xF)))| /* GPIOE13 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E14 & 0xF)))| /* GPIOE14 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E15 & 0xF)))  /* GPIOE15 -        NC                                      */
 },
 {
 (void __iomem*) SCON_SYSMUX9_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E16 & 0xF)))| /* GPIOE16 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E17 & 0xF)))| /* GPIOE17 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E18 & 0xF)))| /* GPIOE18 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E19 & 0xF)))| /* GPIOE19 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E20 & 0xF)))| /* GPIOE20 -        NC                                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_E21 & 0xF)))| /* SDATIO2 -        AGPS_SPI2_DATIO (SPI2)                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_E22 & 0xF)))| /* SDATIN2 -        AGPS_SPI2_DATIN (SPI2)                  */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_E23 & 0xF)))| /* SCLK2 -          AGPS_SPI2_CLK   (SPI2)                  */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E24 & 0xF)))| /* GPIOE24 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E25 & 0xF)))| /* GPIOE25 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E26 & 0xF)))| /* GPIOE26 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E27 & 0xF)))| /* GPIOE27 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E28 & 0xF)))| /* GPIOE28 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E29 & 0xF)))| /* GPIOE29 -        NC                                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_E30 & 0xF)))| /* RFSM_OUT3 -      ANT_SEL3                                */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_E31 & 0xF)))  /* GPIOE31 -        CAM_PWRDN2                              */
 },
 {
 (void __iomem*) SCON_SYSMUX10_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F0  & 0xF)))| /* GPIOF0 -         CAM_LDO_ONOFF                           */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F1  & 0xF)))| /* GPIOF1 -         AGPS_FRAM_SYNC_GPIOF1                   */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F2  & 0xF)))| /* GPIOF2 -         FTM_MODE                                */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_F3  & 0xF)))| /* KCOL5 -          KCOL5                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_F4  & 0xF)))| /* KCOL6 -          KCOL6                                   */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F5  & 0xF)))| /* GPIOF5 -         NC                                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_F6  & 0xF)))| /* KROW5 -          KROW5                                   */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_F7  & 0xF)))| /* KROW6 -          KROW6                                   */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F8  & 0xF)))| /* GPIOF8 -         NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F9  & 0xF)))| /* GPIOF9 -         AGPS_RSTn                               */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F10 & 0xF)))| /* GPIOF10 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F11 & 0xF)))| /* GPIOF11 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F12 & 0xF)))| /* GPIOF12 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F13 & 0xF)))| /* GPIOF13 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F14 & 0xF)))| /* GPIOF14 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F15 & 0xF)))  /* GPIOF15 -        NC                                      */
 },
 {
 (void __iomem*) SCON_SYSMUX11_REG,
 0 |
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F16 & 0xF)))| /* GPIOF16 -        NC                                      */
 ( GPIO_MODE_MUX1 << (2 * (GPIO_F17 & 0xF)))| /* GPIOF17 -        NC                                      */
 ( GPIO_MODE_MUX0 << (2 * (GPIO_F18 & 0xF)))  /* NFI_CE_n -       NFI_CEn                                */
 },
 /* Configure PAD Value */
 {
 (void __iomem*) SCON_SYSPAD0_REG,
 0 |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A0   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A1   & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A2   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A3   & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A4   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A5   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A6   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A7   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A8   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A9   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A10  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A11  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A12  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A13  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A14  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A15  & 0xF)))
 },
 {
 (void __iomem*) SCON_SYSPAD1_REG,
 0 |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A16  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A17  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A18  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A19  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A20  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A21  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A22  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A23  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A24  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_A25  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A26  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A27  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A28  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A29  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A30  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_A31  & 0xF)))
 },
 {
 (void __iomem*) SCON_SYSPAD2_REG,
 0 |
 (SCON_PAD_PULL_DOWN   << (2 * (GPIO_B0   & 0xF))) |
 (SCON_PAD_PULL_DOWN   << (2 * (GPIO_B1   & 0xF))) |
 (SCON_PAD_PULL_DOWN   << (2 * (GPIO_B2   & 0xF))) |
 (SCON_PAD_PULL_DOWN   << (2 * (GPIO_B3   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B4   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B5   & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_B6   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B7   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B8   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B9   & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B10  & 0xF))) |
 (SCON_PAD_PLAIN_INPUT << (2 * (GPIO_B11  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B12  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B13  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B14  & 0xF))) |
 (SCON_PAD_REPEATER    << (2 * (GPIO_B15  & 0xF)))
 },
};

/* TODO A21 and D30 not in the following list, Ask to HW team an HSI update */
/* GPIO def settings to avoid HW issue */
struct pnx_gpio_config pnx_gpio_init_config[] = {
  /*============================================================================================*/
  /* GPIO A bank */
  /*============================================================================================*/
  /* {.gpio = GPIO_A0,    .dir =                ,   .value =  , },*/  /* FM_IRQ                 */
  /* {.gpio = GPIO_A1,    .dir =                ,   .value =  , },*/  /* BT_UART2_RXD           */
  /* {.gpio = GPIO_A2,    .dir =                ,   .value =  , },*/  /* BT_UART2_CTSn          */
     {.gpio = GPIO_A3,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* SD_CARD_DET            */
  /* {.gpio = GPIO_A4,    .dir =                ,   .value =  , },*/  /* SIM_OFF                */
     {.gpio = GPIO_A5,    .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* AGPS_SPI2_SS_N_GPIOA5  */
     {.gpio = GPIO_A6,    .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* BKLIGHT_LCD_ON_OFF     */
     {.gpio = GPIO_A7,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* BT_POWERDN             */
     {.gpio = GPIO_A8,    .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* DVM_SEL1_GPIO5         */
     {.gpio = GPIO_A9,    .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* DVM_SEL2_GPIO6         */
  /* {.gpio = GPIO_A10    .dir =                ,   .value =  , },*/  /* BT_UART2_RTSn          */
  /* {.gpio = GPIO_A11,   .dir =                ,   .value =  , },*/  /* BT_UART2_TXD           */
  /* {.gpio = GPIO_A12,   .dir =                ,   .value =  , },*/  /* OVP_FAULT              */
     {.gpio = GPIO_A13,   .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* SPI1_CS                */
  /* {.gpio = GPIO_A14,   .dir =                ,   .value =  , },*/  /* POWER_IRQ              */
  /* {.gpio = GPIO_A15,   .dir =                ,   .value =  , },*/  /* RF_ANT_DET             */
  /* {.gpio = GPIO_A16,   .dir =                ,   .value =  , },*/  /* SD_DATA3               */
  /* {.gpio = GPIO_A17,   .dir =                ,   .value =  , },*/  /* HSDET                  */
  /* {.gpio = GPIO_A18,   .dir =                ,   .value =  , },*/  /* SD_DATA2               */
  /* {.gpio = GPIO_A19,   .dir =                ,   .value =  , },*/  /* SD_DATA1               */
  /* {.gpio = GPIO_A20,   .dir =                ,   .value =  , },*/  /* HM_INT_GPIOA20         */
  /* {.gpio = GPIO_A21,   .dir =                ,   .value =  , },*/  /* RF3G_REG_ON            */
  /* {.gpio = GPIO_A22,   .dir =                ,   .value =  , },*/  /* PDNB_2G                */
     {.gpio = GPIO_A23,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* RF2G_RESETn            */
  /* {.gpio = GPIO_A24,   .dir =                ,   .value =  , },*/  /* BT_HOST_WAKE           */
  /* {.gpio = GPIO_A25,   .dir =                ,   .value =  , },*/  /* TP_IRQ                 */
     {.gpio = GPIO_A26,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* CAM_MCLK_SW            */
  /* {.gpio = GPIO_A27,   .dir =                ,   .value =  , },*/  /* SPI1_CLK               */
  /* {.gpio = GPIO_A28,   .dir =                ,   .value =  , },*/  /* SPI1_DATA_OUT          */
     {.gpio = GPIO_A29,   .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* CAM_RESET2             */
     {.gpio = GPIO_A30,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* AGPS_LDO1V2_EN_GPIOA30 */
     {.gpio = GPIO_A31,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* SPI1_DATA_I            */

  /*============================================================================================*/
  /* GPIO B bank */
  /*============================================================================================*/
  /* {.gpio = GPIO_B0,    .dir =                ,   .value =  , },*/  /* BT_DD_OUT              */
  /* {.gpio = GPIO_B1,    .dir =                ,   .value =  , },*/  /* BT_DU_IN               */
  /* {.gpio = GPIO_B2,    .dir =                ,   .value =  , },*/  /* BT_FSC_FSYNC           */
  /* {.gpio = GPIO_B3,    .dir =                ,   .value =  , },*/  /* BT_DCL_CLK             */
     {.gpio = GPIO_B4,    .dir =GPIO_DIR_OUTPUT ,   .value = 1, },    /* HM_EN_GPIOB4           */
  /* {.gpio = GPIO_B5,    .dir =                ,   .value =  , },*/  /* UART_TXD1              */
  /* {.gpio = GPIO_B6,    .dir =                ,   .value =  , },*/  /* NFI_RDY                */
  /* {.gpio = GPIO_B7,    .dir =                ,   .value =  , },*/  /* VDE_EOFI_TE            */
  /* {.gpio = GPIO_B8,    .dir =                ,   .value =  , },*/  /* RFEN0                  */
  /* {.gpio = GPIO_B9,    .dir =                ,   .value =  , },*/  /* SD_CMD                 */
  /* {.gpio = GPIO_B10,   .dir =                ,   .value =  , },*/  /* SD_CLK                 */
  /* {.gpio = GPIO_B11,   .dir =                ,   .value =  , },*/  /* SD_DATA0               */
  /* {.gpio = GPIO_B12,   .dir =                ,   .value =  , },*/  /* BROADCAST_TX_BLANKING  */
  /* {.gpio = GPIO_B13,   .dir =                ,   .value =  , },*/  /* GSM_PA_BAND_1          */
     /* if defined CONFIG_GPIOB14_FOR_RF_CLK_320 will make GPIOB14 be a function pin */
  /* {.gpio = GPIO_B14,   .dir =                ,   .value =  , },*/  /* Not Connected          */
  /* {.gpio = GPIO_B15,   .dir =                ,   .value =  , },*/  /* RFDATA                 */

  /*============================================================================================*/
  /* GPIO C bank */
  /*============================================================================================*/
  /* {.gpio = GPIO_C0,    .dir =                ,   .value =  , },*/  /* VDE_D_0                */
  /* {.gpio = GPIO_C1,    .dir =                ,   .value =  , },*/  /* VDE_D_1                */
  /* {.gpio = GPIO_C2,    .dir =                ,   .value =  , },*/  /* VDE_D_2                */
  /* {.gpio = GPIO_C3,    .dir =                ,   .value =  , },*/  /* VDE_D_3                */
  /* {.gpio = GPIO_C4,    .dir =                ,   .value =  , },*/  /* VDE_D_4                */
  /* {.gpio = GPIO_C5,    .dir =                ,   .value =  , },*/  /* VDE_D_5                */
  /* {.gpio = GPIO_C6,    .dir =                ,   .value =  , },*/  /* VDE_D_6                */
  /* {.gpio = GPIO_C7,    .dir =                ,   .value =  , },*/  /* VDE_D_7                */
  /* {.gpio = GPIO_C8,    .dir =                ,   .value =  , },*/  /* VDE_D_8                */
  /* {.gpio = GPIO_C9,    .dir =                ,   .value =  , },*/  /* VDE_A0_RS              */
  /* {.gpio = GPIO_C10    .dir =                ,   .value =  , },*/  /* BT_WAKE                */
  /* {.gpio = GPIO_C11,   .dir =                ,   .value =  , },*/  /* VDE_CS0_n              */
  /* {.gpio = GPIO_C12,   .dir =                ,   .value =  , },*/  /* VDE_WR_n               */
  /* {.gpio = GPIO_C13,   .dir =                ,   .value =  , },*/  /* VDE_RD_n               */
  /* {.gpio = GPIO_C14,   .dir =                ,   .value =  , },*/  /* KCOL0                  */
  /* {.gpio = GPIO_C15,   .dir =                ,   .value =  , },*/  /* KCOL1                  */
  /* {.gpio = GPIO_C16,   .dir =                ,   .value =  , },*/  /* KCOL2                  */
  /* {.gpio = GPIO_C17,   .dir =                ,   .value =  , },*/  /* KCOL3                  */
  /* {.gpio = GPIO_C18,   .dir =                ,   .value =  , },*/  /* KCOL4                  */
  /* {.gpio = GPIO_C19,   .dir =                ,   .value =  , },*/  /* KROW0                  */
  /* {.gpio = GPIO_C20,   .dir =                ,   .value =  , },*/  /* KROW1                  */
  /* {.gpio = GPIO_C21,   .dir =                ,   .value =  , },*/  /* KROW2                  */
  /* {.gpio = GPIO_C22,   .dir =                ,   .value =  , },*/  /* KROW3                  */
  /* {.gpio = GPIO_C23,   .dir =                ,   .value =  , },*/  /* KROW4                  */
  /* {.gpio = GPIO_C24,   .dir =                ,   .value =  , },*/  /* RF3GSPIEN              */
  /* {.gpio = GPIO_C25,   .dir =                ,   .value =  , },*/  /* RF3GSPIDATA            */
  /* {.gpio = GPIO_C26,   .dir =                ,   .value =  , },*/  /* RF3GSPICLK             */
  /* {.gpio = GPIO_C27,   .dir =                ,   .value =  , },*/  /* ANT_SEL0               */
  /* {.gpio = GPIO_C28,   .dir =                ,   .value =  , },*/  /* ANT_SEL1               */
  /* {.gpio = GPIO_C29,   .dir =                ,   .value =  , },*/  /* ANT_SEL2               */
  /* {.gpio = GPIO_C30,   .dir =                ,   .value =  , },*/  /* CAM_FM_I2C2_SDA        */
  /* {.gpio = GPIO_C31,   .dir =                ,   .value =  , },*/  /* CAM_FM_I2C2_SCL        */

  /*============================================================================================*/
  /* GPIO D bank */
  /*============================================================================================*/
     {.gpio = GPIO_D0,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* CAM_PWRDN1             */
  /* {.gpio = GPIO_D1,    .dir =                ,   .value =  , },*/  /* CAM_D0                 */
  /* {.gpio = GPIO_D2,    .dir =                ,   .value =  , },*/  /* CAM_D1                 */
  /* {.gpio = GPIO_D3,    .dir =                ,   .value =  , },*/  /* CAM_D2                 */
  /* {.gpio = GPIO_D4,    .dir =                ,   .value =  , },*/  /* CAM_D3                 */
  /* {.gpio = GPIO_D5,    .dir =                ,   .value =  , },*/  /* CAM_D4                 */
  /* {.gpio = GPIO_D6,    .dir =                ,   .value =  , },*/  /* CAM_D5                 */
  /* {.gpio = GPIO_D7,    .dir =                ,   .value =  , },*/  /* CAM_D6                 */
  /* {.gpio = GPIO_D8,    .dir =                ,   .value =  , },*/  /* CAM_D7                 */
  /* {.gpio = GPIO_D9,    .dir =                ,   .value =  , },*/  /* GND                    */
  /* {.gpio = GPIO_D10,   .dir =                ,   .value =  , },*/  /* GND                    */
  /* {.gpio = GPIO_D11,   .dir =                ,   .value =  , },*/  /* CAM_VS                 */
  /* {.gpio = GPIO_D12,   .dir =                ,   .value =  , },*/  /* CAM_HS                 */
  /* {.gpio = GPIO_D13,   .dir =                ,   .value =  , },*/  /* CAM_PCLK               */
  /* {.gpio = GPIO_D14,   .dir =                ,   .value =  , },*/  /* CAM_MCLK               */
  /* {.gpio = GPIO_D15,   .dir =                ,   .value =  , },*/  /* 3G_MODEM_ON_OFF        */
  /* {.gpio = GPIO_D16,   .dir =                ,   .value =  , },*/  /* WCDMA_PA_ON0           */
  /* {.gpio = GPIO_D17,   .dir =                ,   .value =  , },*/  /* WCDMA_PA_ON1           */
  /* {.gpio = GPIO_D18,   .dir =                ,   .value =  , },*/  /* SD_CARD_DET            */
  /* {.gpio = GPIO_D19,   .dir =                ,   .value =  , },*/  /* SLIDE_DET              */
  /* {.gpio = GPIO_D20,   .dir =                ,   .value =  , },*/  /* UART_RXD1              */
     {.gpio = GPIO_D21,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_D22,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* OVP_EN_GPIOA17         */
  /* {.gpio = GPIO_D23,   .dir =                ,   .value =  , },*/  /* CAM_RESET1             */
     {.gpio = GPIO_D24,   .dir = GPIO_DIR_OUTPUT,   .value = 1, },    /* CCSET_SEL              */
  /* {.gpio = GPIO_D25,   .dir =                ,   .value =  , },*/  /* MODE_SELECT_1          */
     {.gpio = GPIO_D26,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* PA_VMODE2_1            */
  /* {.gpio = GPIO_D27,   .dir =                ,   .value =  , },*/  /* WCDMA_PA_ON2           */
  /* {.gpio = GPIO_D28,   .dir =                ,   .value =  , },*/  /* RF3GRESETN_1           */
     {.gpio = GPIO_D29,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* USB_SUSPEND            */
     {.gpio = GPIO_D30,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
  /* {.gpio = GPIO_D31,   .dir =                ,   .value =  , },*/  /* RFCLK                  */

  /*============================================================================================*/
  /* GPIO E bank */
  /*============================================================================================*/
     {.gpio = GPIO_E0,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E1,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E2,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E3,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E4,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E5,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E6,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E7,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E8,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E9,    .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E10,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E11,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E12,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E13,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E14,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E15,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E16,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E17,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E18,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E19,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E20,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
  /* {.gpio = GPIO_E21,   .dir =                ,   .value =  , },*/  /* AGPS_SPI2_CLK          */
  /* {.gpio = GPIO_E22,   .dir =                ,   .value =  , },*/  /* AGPS_SPI2_DATIN        */
  /* {.gpio = GPIO_E23,   .dir =                ,   .value =  , },*/  /* AGPS_SPI2_CLK          */
     {.gpio = GPIO_E24,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E25,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E26,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E27,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E28,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
     {.gpio = GPIO_E29,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* Not Connected          */
  /* {.gpio = GPIO_E30,   .dir =                ,   .value =  , },*/  /* ANT_SEL3               */
     {.gpio = GPIO_E31,   .dir = GPIO_DIR_OUTPUT,   .value = 0, },    /* CAM_PWRDN2             */

  /*============================================================================================*/
  /* GPIO F bank */
  /*============================================================================================*/
     {.gpio = GPIO_F0,    .dir = GPIO_DIR_OUTPUT,  .value = 0,  },    /* CAM_LDO_ONOFF          */
     {.gpio = GPIO_F1,    .dir = GPIO_DIR_OUTPUT,  .value = 0,  },    /* AGPS_FRAM_SYNC_GPIOF1  */
  /* {.gpio = GPIO_F2,    .dir =                ,  .value =  , },*/   /* FTM_MODE               */
  /* {.gpio = GPIO_F3,    .dir =                ,  .value =  , },*/   /* KCOL5                  */
  /* {.gpio = GPIO_F4,    .dir =                ,  .value =  , },*/   /* KCOL6                  */
     {.gpio = GPIO_F5,    .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
  /* {.gpio = GPIO_F6,    .dir =                ,  .value =  , },*/   /* KROW5                  */
  /* {.gpio = GPIO_F7,    .dir =                ,  .value =  , },*/   /* KROW6                  */
     {.gpio = GPIO_F8,    .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F9,    .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* AGPS_RSTn              */
     {.gpio = GPIO_F10,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F11,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F12,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F13,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F14,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F15,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F16,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
     {.gpio = GPIO_F17,   .dir = GPIO_DIR_OUTPUT,  .value = 0, },     /* Not Connected          */
  /* {.gpio = GPIO_F18,   .dir =                ,  .value =  , },*/   /* NFI_CEn                */
};

u32 gpio_to_configure = ARRAY_SIZE(pnx_gpio_init_config);
/* } ACER Jen Chang, 2010/03/18*/
