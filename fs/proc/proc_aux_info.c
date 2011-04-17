#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/compile.h>

//#include <mach/msm_iomap.h>
//#include <mach/msm_smd.h>
//acer E130
#define ACER_K3_PR2
#define AUX_PROJECT_NAME "Acer_E130"
#define ACER_BLUETOOTH_DEVICE_ID "STLC 2584"
#define ACER_FM_DEVICE_ID "TEA 5991"
#define ACER_GPS_DEVICE_ID "STE GNS7560"

static int proc_calc_metrics(char *page, char **start, off_t off,
				 int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int build_version_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", AUX_IMAGE_VERSION);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int device_model_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	char ver[24];

	strcpy(ver, AUX_PROJECT_NAME);

	len = snprintf(page, PAGE_SIZE, "%s\n",ver);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int baseband_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	char baseband[24];

#ifdef CONFIG_ARCH_PNX67XX
	strcpy(baseband, "PNX67XX");
#else
  strcpy(baseband, "Unknow");
#endif

	len = snprintf(page, PAGE_SIZE, "%s\n", baseband);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/* ACER Jen chang, 2009/09/01, IssueKeys:AU2.FC-169, Dump driver information { */
static int driver_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", "dirver info: ");

	return proc_calc_metrics(page, start, off, count, eof, len);
}
/* } ACER Jen Chang, 2009/09/01*/

/* ACER Jen chang, 2010/01/04, IssueKeys:AU2.B-3731 , Add for detecting EU/US band { */
#define US_band	0
#define EU_band	1
#define Unknow_band 2
extern char *saved_command_line;
/* } ACER Jen Chang, 2010/01/04*/

/* ACER Bright Lee, 2009/11/19, AU2.FC-730, Display hardware version { */
static int hardware_version_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	/* ACER Jen chang, 2010/01/04, IssueKeys:AU2.B-3731, Get EU/US band detection from boot { */
	int len;
	char *var;

	#if defined (ACER_AU2_PR2) || defined (ACER_AU3_PR2) || defined (ACER_AU4_PR2)
	static char *hw = "0.0";
	#elif defined (ACER_AU2_PCR) || defined (ACER_AU3_PCR) || defined (ACER_AU4_PCR)
	static char *hw = "1.0";
/* ACER BobIHLee@20100505, support AS1 project*/
        #elif defined (ACER_K2_PR1) || defined (ACER_K3_PR1)|| defined (ACER_AS1_PR1)
/* End BobIHLee@20100505*/
        static char *hw = "0.A";
        #elif defined (ACER_K2_PR2) || defined (ACER_K3_PR2)|| defined (ACER_AS1_PR2)
	static char *hw = "1.0";
	#else
	static char *hw = "Unknown";
	#endif

	var = strstr(saved_command_line, "hw.band");
	if(var == NULL)
	{
		var = "Unknown";
		printk("Get saved_command_line fail!!\n");
	}
	else
	{
		if((var[8] - 0x30) == US_band)
			var = "850";
		else
			var = "900";
	}

	len = snprintf(page, PAGE_SIZE, "HW %s_%s\n", hw, var);
	/* } ACER Jen Chang, 2010/01/04*/

	return proc_calc_metrics(page, start, off, count, eof, len);
}
/* } ACER Bright Lee, 2009/11/19 */

/* ACER Erace.Ma@20100106, provide virtual file for cupcake to cat device info */
static int flash_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
  int len;
  extern int nandSize;
  extern char nandID[10];

	len = snprintf(page, PAGE_SIZE, "%s %d\n",nandID,nandSize);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
static int BTdevinfo_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
  int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", ACER_BLUETOOTH_DEVICE_ID);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
static int FMdevinfo_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
  int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", ACER_FM_DEVICE_ID);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
static int GPSdevinfo_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
  int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", ACER_GPS_DEVICE_ID);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
/* End Erace.Ma@20100106*/

static struct {
		char *name;
		int (*read_proc)(char*,char**,off_t,int,int*,void*);
} *p, aux_info[] = {
	{"socinfo",	build_version_read_proc},
	{"devmodel",	device_model_read_proc},
	{"baseband",	baseband_read_proc},
	/* ACER Jen chang, 2009/09/01, IssueKeys:AU2.FC-169, Dump driver information { */
	{"driverinfo", driver_read_proc},
	/* } ACER Jen Chang, 2009/09/01*/
	/*{"version",	kernelversion_read_proc},*/  //erace remove, STE already provide it
	/* ACER Bright Lee, 2009/11/19, AU2.FC-730, Display hardware version { */
	{"hwversion",	hardware_version_read_proc},
	/* } ACER Bright Lee, 2009/11/19 */
        /* ACER Erace.Ma@20100106, provide virtual file for cupcake to cat device info */
	{"flashinfo", flash_read_proc},
	{"BTdevinfo", BTdevinfo_read_proc},
	{"FMdevinfo", FMdevinfo_read_proc},
	{"GPSdevinfo", GPSdevinfo_read_proc},
	/* End Erace.Ma@20100106*/
	{NULL,},
};

void aux_info_init(void)
{
	for (p = aux_info; p->name; p++)
		create_proc_read_entry(p->name, 0, NULL, p->read_proc, NULL);
}
EXPORT_SYMBOL(aux_info_init);

void aux_info_remove(void)
{
	for (p = aux_info; p->name; p++)
		remove_proc_entry(p->name, NULL);
}
EXPORT_SYMBOL(aux_info_remove);
