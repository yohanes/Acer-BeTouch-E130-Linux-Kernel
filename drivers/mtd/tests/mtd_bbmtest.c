/**
 *=============================================================================
 *
 * @file:     mtd_bbmtest.c
 *
 * @brief:
 *
 * @version:  1.0
 * created:   22.01.2009 17:17:02
 * compiler:  gcc
 *
 * @author:  Barre Ludovic
 * company:  stn-wireless at Le Mans
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
 * Tests:
 *
 * FIXME test8
 * find a good stop state
 * actualy this test is failed, PR LMSQBXXXX
 * when this test result is fail, the stop state is not good.
 *
 *=============================================================================
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/mtd/nand.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/device.h>

#define DEVICE_BBMTEST_NAME "bbm_test"

#define PRINT_WARNING KERN_WARNING "mtd_nand_bbmtest: "
#define PRINT_NOTICE KERN_NOTICE "mtd_nand_bbmtest: "
#define PRINT_INFO KERN_INFO "mtd_nand_bbmtest: "

#define RETRIES 3

static int mtdblk = -1;
module_param(mtdblk, int, S_IRUGO);
MODULE_PARM_DESC(mtdblk, "MTD device number to use");

static int test;
module_param(test, int, S_IRUGO);
MODULE_PARM_DESC(test, "test number to run");

static struct mtd_info *mtd;
static unsigned char *writebuf;
static unsigned char *readbuf;

static int pgsize;
static int ebcnt;
static int pgcnt;
static unsigned long next = 1;

static void report_corrupt(unsigned char *read, unsigned char *written);
static int countdiffs(unsigned char *buf, unsigned char *check_buf,
		      unsigned offset, unsigned len, unsigned *bytesp,
		      unsigned *bitsp);
static void print_bufs(unsigned char *read, unsigned char *written, int start,
		       int len);
/* Global variables of the driver */
struct class *bbm_test_class;
/*static*/ dev_t dev_num;

/*******************************************************************/
/*  Test tools box                                                 */
/*******************************************************************/
static inline unsigned int simple_rand(void)
{
	next = next * 1103515245 + 12345;
	return (unsigned int)((next / 65536) % 32768);
}

static inline void simple_srand(unsigned long seed)
{
	next = seed;
}

static int rand_eb(void)
{
	int eb;

	if (ebcnt < 32768)
		eb = simple_rand();
	else
		eb = (simple_rand() << 15) | simple_rand();
	eb %= (ebcnt - 1);
	return eb;
}

static int rand_offs(int nb_eb)
{
    int offs;
    offs = simple_rand();
    offs %= ebcnt-1-nb_eb;
    return offs;
}

static void set_random_data(unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		buf[i] = simple_rand();
}


static inline int write_eraseblock(int ebnum)
{
	int err;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;
	size_t len = mtd->erasesize;

	set_random_data(writebuf, mtd->erasesize);
	err = mtd->write(mtd, addr, len, &written, writebuf);
	if (err) {
		printk(PRINT_WARNING "error %d while writing EB %d, written %zd"
		      " bytes\n", err, ebnum, written);
		return err;
	}
	if (written != len) {
		printk(PRINT_WARNING "written only %zd bytes of %zd, "
				"but no error reported\n", written, len);
		return -EIO;
	}

	return 0;
}

static int verify_eraseblock(int ebnum)
{
	int err, retries = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;
	size_t len = mtd->erasesize;

	set_random_data(writebuf, mtd->erasesize);

retry:
	memset(readbuf, 0x00, len);
	err = mtd->read(mtd, addr, len, &read, readbuf);
	if (err == -EUCLEAN)
		printk(PRINT_WARNING "single bit flip occurred at EB %d "
		       "MTD reported that it was fixed.\n", ebnum);
	else if (err) {
		printk(PRINT_WARNING "error %d while reading EB %d, "
		       "read %zd\n", err, ebnum, read);
		return err;
	}

	if (read != len) {
		printk(PRINT_WARNING "failed to read %zd bytes from EB %d, "
		       "read only %zd, but no error reported\n",
		       len, ebnum, read);
		return -EIO;
	}

	if (memcmp(writebuf, readbuf, len)) {
		printk(PRINT_WARNING "read wrong data from EB %d\n", ebnum);
		report_corrupt(readbuf, writebuf);

		if (retries++ < RETRIES) {
			/* Try read again */
			yield();
			printk(PRINT_WARNING "re-try reading data from EB %d\n",
			       ebnum);
			goto retry;
		} else {
			printk(PRINT_WARNING "retried %d times, still errors, "
			       "give-up\n", RETRIES);
			return -EINVAL;
		}
	}

	if (retries != 0)
		printk(PRINT_WARNING "only attempt number %d was OK (!!!)\n",
		       retries);

	return 0;
}

/*
 * Erase eraseblock number @ebnum.
 */
static inline int erase_eraseblock(int ebnum)
{
	int err;
	struct erase_info ei;
	loff_t addr = ebnum * mtd->erasesize;

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd = mtd;
	ei.addr = addr;
	ei.len = mtd->erasesize;

	err = mtd->erase(mtd, &ei);
	if (err) {
		printk(PRINT_WARNING "error %d while erasing EB %d\n",
				err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		printk(PRINT_WARNING "some erase error occurred at EB %d\n",
		       ebnum);
		return -EIO;
	}

	return 0;
}

static int write_verify_all_mtd(int rand)
{
	int err = 0;
	int i;

	printk(PRINT_INFO "erase all eraseblocks\n");
	/* Erase all eraseblocks */
	for (i = 0; i < ebcnt; i++) {
		err = erase_eraseblock(i);
		if (err)
			goto out;
		cond_resched();
	}

	/* write all eraseblocks */
	simple_srand(rand);
	for (i = 0; i < ebcnt; i++) {
		err = write_eraseblock(i);
		if (err)
			goto out;
		if (i % 256 == 0)
			printk(PRINT_INFO "written up to eraseblock %u\n", i);
		cond_resched();
	}
	printk(PRINT_INFO "written %u eraseblocks\n", i);

	printk(PRINT_INFO "verifying all eraseblocks\n");
	simple_srand(rand);
	for (i = 0; i < ebcnt; i++) {
		err = verify_eraseblock(i);
		if (err)
			goto out;
		if ((i % 256) == 0)
			printk(PRINT_INFO "verified up to eraseblock %u\n", i);
		cond_resched();
	}
	printk(PRINT_INFO "verified %u eraseblocks\n", i);

out:
	return err;
}

static void print_markbad_err(void)
{
	printk(PRINT_WARNING
			"mark block failed (bbm pool empty?)\n");
	printk(PRINT_WARNING
			"if bbm pool is empty, bad block isn't replaced.\n");
	printk(PRINT_WARNING
			"if you use this block, an error is expected.\n");
}

/*******************************************************************/
/*  Tests                                                          */
/*******************************************************************/
struct bbm_test_case {
	const char *name;

	int (*prepare)(void);
	int (*run)(void);
	int (*check)(int);
};

/*
 * force the first block like bad, and write
 * then verify the blocks
 */
static int test2(void)
{
	printk(PRINT_INFO "mark bad block\n");
	return mtd->block_markbad(mtd, 0);
}

/*
 * double mark bad on block (with data verification)
 */
static int test3(void)
{
	int err;

	err = test2();
	if (err < 0)
		return err;
	return test2();
}

/*
 * Mark bad the last block (with data verification)
 */
static int test4(void)
{
	loff_t addr;

	addr = (ebcnt-1)*mtd->erasesize;
	printk(PRINT_INFO "mark bad block\n");
	return mtd->block_markbad(mtd, addr);
}

/*
 * Mark bad a random block (with data verification)
 */
static int test5(void)
{
	printk(PRINT_INFO "mark bad block\n");
	return mtd->block_markbad(mtd, rand_eb()*mtd->erasesize);
}
/*
 * Multi bad blocks adjacent (with data verification)
 */
static int test6(void)
{
	int nb_max_bblock, nb_blocks, eb;
	int err = 0, i;

	nb_max_bblock = DIV_ROUND_UP(ebcnt*2, 100);
	/* take 1/4 of 2% of mtd's blocks */
	nb_blocks = DIV_ROUND_UP(nb_max_bblock, 4);
	eb = rand_offs(nb_blocks);

	printk(PRINT_INFO "mark bad blocks\n");
	for (i = 0; i < nb_blocks; i++) {
		err = mtd->block_markbad(mtd, (eb+i)*mtd->erasesize);
		if (err < 0)
			break;
	}
	return err;
}

/*
 * Multi bad blocks random (with data verification)
 */
static int test7(void)
{
	int i, err = 0;
	int nb_max_bblock, nb_blocks;

	nb_max_bblock = DIV_ROUND_UP(ebcnt*2, 100);
	/* take 1/4 of 2% of mtd's blocks */
	nb_blocks = DIV_ROUND_UP(nb_max_bblock, 4);

	printk(PRINT_INFO "mark bad blocks\n");
	for (i = 0; i < nb_blocks; i++) {
		err = mtd->block_markbad(mtd, rand_eb()*mtd->erasesize);
		if (err < 0)
			break;
	}
	return err;
}

/*
 * Mark bad the same block until there aren't free block in rsv
 */
/*static int test8(void)*/
/*{*/
/*        int eb, ret1, ret2;*/
/*        int err = 0;*/
/*        loff_t addr;*/
/*        int count = 0;*/

/*        eb = rand_eb();*/
/*        addr = eb*mtd->erasesize;*/

/*        while ((ret1 = mtd->block_isbad(mtd, addr)) == 0) {*/
/*                printk(PRINT_INFO "block number %d is markbad %dth\n",*/
/*                                eb, count);*/
/*                ret2 = mtd->block_markbad(mtd, addr);*/

/*                err = erase_eraseblock(eb);*/
/*                if (err)*/
/*                        goto out;*/
/*                cond_resched();*/

/*                simple_srand(6);*/
/*                err = write_eraseblock(eb);*/
/*                if (err)*/
/*                        goto out;*/
/*                cond_resched();*/

/*                simple_srand(6);*/
/*                err = verify_eraseblock(eb);*/
/*                if (err)*/
/*                        goto out;*/
/*                cond_resched();*/
/*                count++;*/
/*        }*/
/*        ret2 = mtd->block_markbad(mtd, addr);*/

/*        if ((ret1 != 1) | (ret2 != 0)) {*/
/*                printk(PRINT_WARNING "\n"*/
/*                                "when there aren't free block "*/
/*                                "these funtions not return "*/
/*                                "the good value:\n");*/
/*                printk(PRINT_WARNING*/
/*                                " -block_isbad return:%d (expected 1)\n",*/
/*                                ret1);*/
/*                printk(PRINT_WARNING*/
/*                                " -block_markbad return:%d (expected 0)\n",*/
/*                                ret2);*/
/*                err = -1;*/
/*        }*/
/*out:*/
/*        return err;*/
/*}*/


static const struct bbm_test_case bbm_test_cases[] = {
	{
		.name = "Basic read write (with data verification)",
		.check = write_verify_all_mtd,
	},
	{
		.name = "Mark bad the first block (with data verification)",
		.run = test2,
		.check = write_verify_all_mtd,
	},
	{
		.name = "Double mark bad on block (with data verification)",
		.run = test3,
		.check = write_verify_all_mtd,
	},
	{
		.name = "Mark bad the last block (with data verification)",
		.run = test4,
		.check = write_verify_all_mtd,
	},
	{
		.name = "Mark bad a random block (with data verification)",
		.run = test5,
		.check = write_verify_all_mtd,
	},
	{
		.name = "Multi bad blocks adjacent (with data verification)",
		.run = test6,
		.check = write_verify_all_mtd,
	},
	{
		.name = "Multi bad blocks random (with data verification)",
		.run = test7,
		.check = write_verify_all_mtd,
	},
/*
 * FIXME
 * find a good stop state
 * actualy this test is failed, PR LMSQBXXXX
 * when this test result is fail, the stop state is not good.
 */
/*	{
 *		.name = "Mark the same blk until there aren't free block",
 *		.run = test8,
 *		.check = write_verify_all_mtd,
 *	},
 */
};

/*******************************************************************/
/*  Tests Engine                                                   */
/*******************************************************************/

static int bbm_test_run(void)
{
	int err = 0, i;
	uint64_t tmp;


	if (mtd->writesize == 1) {
		printk(PRINT_WARNING "not NAND flash,"
				"assume page size is 512 bytes.\n");
		pgsize = 2048;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

	printk(PRINT_INFO "MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	/* allocate a write buffer*/
	err = -ENOMEM;
	writebuf = vmalloc(mtd->erasesize);
	readbuf  = vmalloc(mtd->erasesize);
	if (!writebuf | !readbuf) {
		printk(PRINT_WARNING "error: cannot allocate memory\n");
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(bbm_test_cases); i++) {
		if (test && ((i + 1) != test))
			continue;
		printk(PRINT_NOTICE "Test case %d. %s...\n",
				i + 1, bbm_test_cases[i].name);
		/* prepare */
		if (bbm_test_cases[i].prepare) {
			err = bbm_test_cases[i].prepare();
			if (err) {
				printk(PRINT_WARNING "Test case %d: "
						"prepare failed\n", i + 1);
				goto out;
			}
		}
		/* run */
		if (bbm_test_cases[i].run) {
			err = bbm_test_cases[i].run();
			if (err < 0) {
				printk(PRINT_WARNING "Test case %d: "
						"run failed\n", i + 1);
				print_markbad_err();
				goto out;
			}
		}
		/* check */
		if (bbm_test_cases[i].check) {
			err = bbm_test_cases[i].check(i+1);
			if (err) {
				printk(PRINT_WARNING "Test case %d: "
						"check failed\n", i + 1);
				goto out;
			}
		}
		printk(PRINT_NOTICE "Test case %d: Result: OK\n", i + 1);
	}

out:
	vfree(writebuf);
	vfree(readbuf);
	put_mtd_device(mtd);
	if (err)
		printk(PRINT_WARNING "error %d occurred\n", err);

	printk(PRINT_INFO
			"===============================================\n");
	printk(PRINT_INFO
			"if you reuse this partition with classic\n"
			"filesystem. You must erase the nand completly\n"
			"to remove the wrong bad block\n");
	printk(PRINT_INFO
			"===============================================\n");
	return err;
}

ssize_t bbm_test_show(struct class *cls, char *buf)
{
	return sprintf(buf, "help:\n"
			"  Usage: echo <test_number> > test\n\n"
			"  test_number: 0 => run all test\n"
			"               X => run test number X\n");
}
ssize_t bbm_test_store(struct class *cls, const char *buf, size_t count)
{
	long res;
	strict_strtol(buf, 10, &res);
	if ((res < 0) && (res > ARRAY_SIZE(bbm_test_cases)))
		return -EINVAL;
	test = res;
	bbm_test_run();

	return count;
}
static CLASS_ATTR(test, S_IWUSR | S_IRUGO, bbm_test_show, bbm_test_store);

static int __init mtd_nand_bbmtest_init(void)
{
	int err = -EINVAL;

	if (mtdblk < 0) {
		printk(PRINT_WARNING "Device number failed\n");
		goto out;
	}

	printk(PRINT_INFO "\n");
	printk(PRINT_INFO "================================================\n");
	printk(PRINT_INFO "MTD device: %d\n", mtdblk);

	mtd = get_mtd_device(NULL, mtdblk);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(PRINT_WARNING "error: cannot get MTD device\n");
		goto out;
	}

	bbm_test_class = class_create(THIS_MODULE, DEVICE_BBMTEST_NAME);
	if (IS_ERR(bbm_test_class)) {
		printk(PRINT_WARNING "init %s failed\n", __func__);
		goto out;
	}

	err = class_create_file(bbm_test_class, &class_attr_test);
	if (err) {
		printk(PRINT_WARNING "error: cannot create sys file\n");
		goto out;
	}
out:
	return err;
}
module_init(mtd_nand_bbmtest_init);

static void __exit mtd_nand_bbmtest_exit(void)
{
	class_remove_file(bbm_test_class, &class_attr_test);
	class_destroy(bbm_test_class);
	return;
}
module_exit(mtd_nand_bbmtest_exit);

MODULE_DESCRIPTION("BBM test module");
MODULE_AUTHOR("Ludovic Barre");
MODULE_LICENSE("GPL");

/*
 * Report the detailed information about how the read EB differs from what was
 * written.
 */
static void report_corrupt(unsigned char *read, unsigned char *written)
{
	int i;
	int bytes, bits, pages, first;
	int offset, len;
	size_t check_len = mtd->erasesize;

	bytes = bits = pages = 0;
	for (i = 0; i < check_len; i += pgsize)
		if (countdiffs(written, read, i, pgsize, &bytes,
			       &bits) >= 0)
			pages++;

	printk(PRINT_INFO "verify fails on %d pages, %d bytes/%d bits\n",
	       pages, bytes, bits);
	printk(PRINT_INFO "The following is a list of all differences between"
	       " what was read from flash and what was expected\n");

	for (i = 0; i < check_len; i += pgsize) {
		cond_resched();
		bytes = bits = 0;
		first = countdiffs(written, read, i, pgsize, &bytes,
				   &bits);
		if (first < 0)
			continue;

		printk(PRINT_INFO "------------------------------------"
				"--------------------------------------"
				"---------------\n");

		printk(PRINT_INFO "Page %d has %d bytes/%d bits failing verify,"
		       " starting at offset 0x%x\n",
		       (mtd->erasesize - check_len + i) / pgsize,
		       bytes, bits, first);

		offset = first & ~0x7;
		len = ((first + bytes) | 0x7) + 1 - offset;

		print_bufs(read, written, offset, pgsize);
	}
}

static void print_bufs(unsigned char *read, unsigned char *written, int start,
		       int len)
{
	int i = 0, j1, j2;
	char *diff;

	printk(PRINT_INFO "Offset       Read                        Written\n");
	while (i < len) {
		printk(PRINT_INFO "0x%08x: ", start + i);
		diff = "   ";
		for (j1 = 0; j1 < 8 && i + j1 < len; j1++) {
			printk(" %02x", read[start + i + j1]);
			if (read[start + i + j1] != written[start + i + j1])
				diff = "***";
		}

		while (j1 < 8) {
			printk(" ");
			j1 += 1;
		}

		printk("  %s ", diff);

		for (j2 = 0; j2 < 8 && i + j2 < len; j2++)
			printk(" %02x", written[start + i + j2]);
		printk("\n");
		i += 8;
	}
}

/*
 * Count the number of differing bytes and bits and return the first differing
 * offset.
 */
static int countdiffs(unsigned char *buf, unsigned char *check_buf,
		      unsigned offset, unsigned len, unsigned *bytesp,
		      unsigned *bitsp)
{
	unsigned i, bit;
	int first = -1;

	for (i = offset; i < offset + len; i++)
		if (buf[i] != check_buf[i]) {
			first = i;
			break;
		}

	while (i < offset + len) {
		if (buf[i] != check_buf[i]) {
			(*bytesp)++;
			bit = 1;
			while (bit < 256) {
				if ((buf[i] & bit) != (check_buf[i] & bit))
					(*bitsp)++;
				bit <<= 1;
			}
		}
		i++;
	}

	return first;
}
