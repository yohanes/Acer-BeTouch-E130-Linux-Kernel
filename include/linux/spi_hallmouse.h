#ifndef _SPI_HALLMOUSE_H
#define _SPI_HALLMOUSE_H

#ifdef CONFIG_SPI_HALL_MOUSE
struct pnx_hall_mouse_pdata {
	int hm_en;
//	int hm_cs; /* Redondant */
	int hm_sck_pin;
	int hm_sck_mux;
	int hm_si_pin;
	int hm_si_mux;
	int hm_so_pin;
	int hm_so_mux;
	int hm_irq;
};

typedef void (* itsignalcallback)(unsigned  int key_id, int pressed);
extern void spi_hallmouse_registerSignalFct(itsignalcallback signalfct);
extern void spi_hallmouse_unregisterSignalFct(void);
#endif

#endif /* _SPI_HALLMOUSE_H */
