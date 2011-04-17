/* ---- Do not edit, this file is automatically by flashConfig.pl from partitiontable_android.ptt */

/* Be careful to change the partition layout. Need to care about SD Download and EUU download */

static struct mtd_partition nand_partitions[] = 
{
#if defined(CONFIG_ANDROID_RECOVERY)
	{ .name = "modemfs" , .size =   1024*SZ_1K, .offset = 0x00100000, },
	{ .name = "modemfs2", .size =   1024*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "system"  , .size =  98304*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "userdata", .size = 122880*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "cache"   , .size =   8192*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "misc"    , .size =   2048*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "recovery", .size =   6144*SZ_1K, .offset = MTDPART_OFS_APPEND, },
	{ .name = "boot"    , .size =   6144*SZ_1K, .offset = MTDPART_OFS_APPEND, },
#else
	/* ACER Bright Lee, 2010/5/5, A41.B-739, dynamic partition layout { */
	{ .name = "modemfs" ,    .size =   1*1024 * SZ_1K,  .offset = 0x00200000 , .index = 7 , },
	{ .name = "rootfs" ,     .size =   4*1024 * SZ_1K,  .index = 8 , },
	{ .name = "paramfs" ,    .size =   1*1024 * SZ_1K,  .offset = 0x00700000 , .index = 9 , },
	{ .name = "system" ,     .size = 180*1024 * SZ_1K,  .index = 16 , },
	{ .name = "userdata" ,   .size =   262016 * SZ_1K,  .index = 19 , },
	{ .name = "cache" ,      .size =  20*1024 * SZ_1K,  .index = 18 , },
	{ .name = "rootfs_ftm" , .size =  12*1024 * SZ_1K,  .index = 15 , },
	{ .name = "recovery" ,   .size =   3*1024 * SZ_1K,  .index = 14 , },
	{ .name = "ftm" ,        .size =      768 * SZ_1K,  .offset = 0x00100000 , .index = 4 , },
	{ .name = "ftmmode" ,    .size =      128 * SZ_1K,  .index = 5 , },
	{ .name = "modules" ,    .size =   6*1024 * SZ_1K,  .index = 13 , },
	{ .name = "hidden" ,     .size =   5*1024 * SZ_1K,  .index = 17 , },
	{ .name = "boot" ,       .size =      128 * SZ_1K,  .offset = 0x00000000 , .index = 1 , },
	{ .name = "splash" ,     .size =      384 * SZ_1K,  .index = 2 , },
	{ .name = "u_boot" ,     .size =      512 * SZ_1K,  .index = 3 , },
	{ .name = "ramdump" ,    .size =      384 * SZ_1K,  .index = 10 , },
	{ .name = "kernel" ,     .size =   4*1024 * SZ_1K,  .index = 11 , },
	{ .name = "modem" ,      .size =   7*1024 * SZ_1K,  .index = 12 , },
	{ .name = "pidinfo" ,    .size =      128 * SZ_1K,  .offset = 0x001E0000 , .index = 6 , },   
#endif
#if defined(CONFIG_MTD_NAND_BBM)                                                
	{ .name = "rsv" ,        .size =    11008 * SZ_1K,  .offset = 0x1F540000, .index = 20, },
#endif
	/* } ACER Bright Lee, 2010/5/5 */
};

