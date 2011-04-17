/* ---- Do not edit, this file is automatically by flashConfig.pl from partitiontable.ptt */

static struct mtd_partition nand_partitions[] = {
	{ .name = "modemfs" , .size =   1024*SZ_1K, .offset = 0x00100000, },
	{ .name = "rootfs"  , .size =   8192*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "paramfs" , .size =   1024*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "usrfs"   , .size =  65536*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "homefs"  , .size = 134144*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "diskfs"  , .size =  32768*SZ_1K, .offset = MTDPART_OFS_APPEND, },
#if defined(CONFIG_MTD_NAND_BBM)
	{ .name = "rsv"     , .size =   5760*SZ_1K, .offset = 0x0FA60000, },
#endif
};
