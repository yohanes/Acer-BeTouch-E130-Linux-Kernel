/* linux/drivers/misc/pmem.c
 *
 * Modified by ST-Ericsson
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/android_pmem.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <linux/mempolicy.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>

/***** +debug *****/
/* enable icebird debug/trace stuff */
#define CONFIG_DEBUG
#include <mach/debug.h>

#define DEBUG_LAYER  1
#define DEBUG_VAR    config_pmem_debug

#if defined(CONFIG_DEBUG)
static int config_pmem_debug = 0;
module_param_named(debug, config_pmem_debug, int, S_IRUGO | S_IWUSR);
#endif

/* enable debug instrumentation */
#define PMEM_DEBUG 1

/***** -debug *****/


#define PMEM_MAX_DEVICES 10
#define PMEM_MAX_ORDER 128
#define PMEM_MIN_ALLOC PAGE_SIZE


/* indicates that a refernce to this file has been taken via get_pmem_file,
 * the file should not be released until put_pmem_file is called */
#define PMEM_FLAGS_BUSY 0x1
/* indicates that this is a suballocation of a larger master range */
#define PMEM_FLAGS_CONNECTED 0x1 << 1
/* indicates this is a master and not a sub allocation and that it is mmaped */
#define PMEM_FLAGS_MASTERMAP 0x1 << 2
/* submap and unsubmap flags indicate:
 * 00: subregion has never been mmaped
 * 10: subregion has been mmaped, reference to the mm was taken
 * 11: subretion has ben released, refernece to the mm still held
 * 01: subretion has been released, reference to the mm has been released
 */
#define PMEM_FLAGS_SUBMAP 0x1 << 3
#define PMEM_FLAGS_UNSUBMAP 0x1 << 4


struct pmem_data {
	/* in alloc mode: an index into the bitmap
	 * in no_alloc mode: the size of the allocation */
	int index;
	/* see flags above for descriptions */
	unsigned int flags;
	/* protects this data field, if the mm_mmap sem will be held at the
	 * same time as this sem, the mm sem must be taken first (as this is
	 * the order for vma_open and vma_close ops */
	struct rw_semaphore sem;
	/* info about the mmaping process */
	struct vm_area_struct *vma;
	/* task struct of the mapping process */
	struct task_struct *task;
	/* process id of teh mapping process */
	pid_t pid;
	/* file descriptor of the master */
	int master_fd;
	/* file struct of the master */
	struct file *master_file;
	/* a list of currently available regions if this is a suballocation */
	struct list_head region_list;
	/* a linked list of data so we can access them for debugging */
	struct list_head list;
#if PMEM_DEBUG
	int ref;
#endif
	/* file struct (for debug) */
	struct file *this_file;

	/* pmem_alloc/pmem_free */
	struct files_struct *files;
	int fd;
};

struct pmem_bits {
	unsigned allocated:1;		/* 1 if allocated, 0 if free */
	unsigned order:7;		/* size of the region in pmem space */
};

struct pmem_region_node {
	struct pmem_region region;
	struct list_head list;
};



struct pmem_info {
	struct miscdevice dev;
	/* physical start address of the remaped pmem space */
	unsigned long base;
	/* vitual start address of the remaped pmem space */
	unsigned char __iomem *vbase;
	/* total size of the pmem space */
	unsigned long size;
	/* number of entries in the pmem space */
	unsigned long num_entries;
	/* pfn of the garbage page in memory */
	unsigned long garbage_pfn;
	/* index of the garbage page in the pmem space */
	int garbage_index;
	/* the bitmap for the region indicating which entries are allocated
	 * and which are free */
	struct pmem_bits *bitmap;
	/* indicates the region should not be managed with an allocator */
	unsigned no_allocator;
	/* indicates maps of this region should be cached, if a mix of
	 * cached and uncached is desired, set this and open the device with
	 * O_SYNC to get an uncached region */
	unsigned cached;
	unsigned buffered;
	/* in no_allocator mode the first mapper gets the whole space and sets
	 * this flag */
	unsigned allocated;
	/* for debugging, creates a list of pmem file structs, the
	 * data_list_sem should be taken before pmem_data->sem if both are
	 * needed */
	struct semaphore data_list_sem;
	struct list_head data_list;
	/* pmem_sem protects the bitmap array
	 * a write lock should be held when modifying entries in bitmap
	 * a read lock should be held when reading data from bits or
	 * dereferencing a pointer into bitmap
	 *
	 * pmem_data->sem protects the pmem data of a particular file
	 * Many of the function that require the pmem_data->sem have a non-
	 * locking version for when the caller is already holding that sem.
	 *
	 * IF YOU TAKE BOTH LOCKS TAKE THEM IN THIS ORDER:
	 * down(pmem_data->sem) => down(bitmap_sem)
	 */
	struct rw_semaphore bitmap_sem;

	long (*ioctl)(struct file *, unsigned int, unsigned long);
	int (*release)(struct inode *, struct file *);

	unsigned long stat_orders[32];
	unsigned long stat_max_orders[32];
	unsigned long stat_curr;
	unsigned long stat_max;
};

static struct pmem_info pmem[PMEM_MAX_DEVICES];
static int id_count = 0;

#if !defined(MAX)
#define MAX(a,b) ((a>b)?a:b)
#endif

#define PMEM_IS_FREE(id, index) !(pmem[id].bitmap[index].allocated)
#define PMEM_ORDER(id, index) pmem[id].bitmap[index].order
#define PMEM_BUDDY_INDEX(id, index) (index ^ (1 << PMEM_ORDER(id, index)))
#define PMEM_NEXT_INDEX(id, index) (index + (1 << PMEM_ORDER(id, index)))
#define PMEM_OFFSET(index) (index * PMEM_MIN_ALLOC)
#define PMEM_START_ADDR(id, index) (PMEM_OFFSET(index) + pmem[id].base)
#define PMEM_LEN(id, index) ((1 << PMEM_ORDER(id, index)) * PMEM_MIN_ALLOC)
#define PMEM_END_ADDR(id, index) (PMEM_START_ADDR(id, index) + \
	PMEM_LEN(id, index))
#define PMEM_START_VADDR(id, index) (PMEM_OFFSET(id, index) + pmem[id].vbase)
#define PMEM_END_VADDR(id, index) (PMEM_START_VADDR(id, index) + \
	PMEM_LEN(id, index))
#define PMEM_REVOKED(data) (data->flags & PMEM_FLAGS_REVOKED)
#define PMEM_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))
#define PMEM_IS_SUBMAP(data) ((data->flags & PMEM_FLAGS_SUBMAP) && \
	(!(data->flags & PMEM_FLAGS_UNSUBMAP)))

static int pmem_release(struct inode *, struct file *);
static int pmem_mmap(struct file *, struct vm_area_struct *);
static int pmem_open(struct inode *, struct file *);
static long pmem_ioctl(struct file *, unsigned int, unsigned long);

struct file_operations pmem_fops = {
	.release = pmem_release,
	.mmap = pmem_mmap,
	.open = pmem_open,
	.unlocked_ioctl = pmem_ioctl,
};

static int get_id(struct file *file)
{
	return MINOR(file->f_dentry->d_inode->i_rdev);
}

int is_pmem_file(struct file *file)
{
	int id;

	if (unlikely(!file || !file->f_dentry || !file->f_dentry->d_inode))
		return 0;
	id = get_id(file);
	if (unlikely(id >= PMEM_MAX_DEVICES))
		return 0;
	if (unlikely(file->f_dentry->d_inode->i_rdev !=
	     MKDEV(MISC_MAJOR, pmem[id].dev.minor)))
		return 0;
	return 1;
}

static int has_allocation(struct file *file)
{
	struct pmem_data *data;
	/* check is_pmem_file first if not accessed via pmem_file_ops */

	if (unlikely(!file->private_data))
		return 0;
	data = (struct pmem_data *)file->private_data;
	if (unlikely(data->index < 0))
		return 0;
	return 1;
}

static int is_master_owner(struct file *file)
{
	struct file *master_file;
	struct pmem_data *data;
	int put_needed, ret = 0;

	if (!is_pmem_file(file) || !has_allocation(file))
		return 0;
	data = (struct pmem_data *)file->private_data;
	if (PMEM_FLAGS_MASTERMAP & data->flags)
		return 1;
	master_file = fget_light(data->master_fd, &put_needed);
	if (master_file && data->master_file == master_file)
		ret = 1;
	fput_light(master_file, put_needed);
	return ret;
}

static int pmem_free_id(int id, int index)
{
	/* caller should hold the write lock on pmem_sem! */
	int buddy, curr = index;
	//printk("index %d\n", index);

	if (pmem[id].no_allocator) {
		pmem[id].allocated = 0;
		return 0;
	}
        
	/* stat */
	pmem[id].stat_curr -= PMEM_LEN(id, index);
	pmem[id].stat_orders[PMEM_ORDER(id, index)]--;

	/* clean up the bitmap, merging any buddies */
	pmem[id].bitmap[curr].allocated = 0;
	/* find a slots buddy Buddy# = Slot# ^ (1 << order)
	 * if the buddy is also free merge them
	 * repeat until the buddy is not free or end of the bitmap is reached
	 */
	do {
		buddy = PMEM_BUDDY_INDEX(id, curr);
		if (PMEM_IS_FREE(id, buddy) &&
				PMEM_ORDER(id, buddy) == PMEM_ORDER(id, curr)) {
			PMEM_ORDER(id, buddy)++;
			PMEM_ORDER(id, curr)++;
			curr = min(buddy, curr);
		} else {
			break;
		}
	} while (curr < pmem[id].num_entries);

	return 0;
}

static void pmem_revoke(struct file *file, struct pmem_data *data);
#if PMEM_DEBUG	
static ssize_t format_debug_string(int id, char *buffer, size_t bufsize);
#endif

static unsigned long pmem_order(unsigned long len)
{
	int i;

	len = (len + PMEM_MIN_ALLOC - 1)/PMEM_MIN_ALLOC;
	len--;
	for (i = 0; i < sizeof(len)*8; i++)
		if (len >> i == 0)
			break;
	return i;
}

static int pmem_allocate(int id, unsigned long len)
{
	/* caller should hold the write lock on pmem_sem! */
	/* return the corresponding pdata[] entry */
	int curr = 0;
	int end = pmem[id].num_entries;
	int best_fit = -1;
	unsigned long order = pmem_order(len);
#if PMEM_DEBUG	
	const int debug_bufmax = 4096;
	static char buffer[4096];
#endif

	PROLOG("");

	if (pmem[id].no_allocator) {
		PDEBUG("no allocator");
		if ((len > pmem[id].size) || pmem[id].allocated) {
			CRITICAL("cannot allocate: too large or already allocated");
			EPILOG("");
			return -1;
		}
		pmem[id].allocated = 1;

		EPILOG("");
		return len;
	}

	if (order > PMEM_MAX_ORDER) {
		CRITICAL("cannot allocate: max order exceeded");

		EPILOG("");
		return -1;
	}
	PDEBUG("order %lx", order);

	/* look through the bitmap:
	 * 	if you find a free slot of the correct order use it
	 * 	otherwise, use the best fit (smallest with size > order) slot
	 */
	while (curr < end) {
		if (PMEM_IS_FREE(id, curr)) {
			if (PMEM_ORDER(id, curr) == (unsigned char)order) {
				/* set the not free bit and clear others */
				best_fit = curr;
				break;
			}
			if (PMEM_ORDER(id, curr) > (unsigned char)order &&
			    (best_fit < 0 ||
			     PMEM_ORDER(id, curr) < PMEM_ORDER(id, best_fit)))
				best_fit = curr;
		}
		curr = PMEM_NEXT_INDEX(id, curr);
	}

	/* if best_fit < 0, there are no suitable slots,
	 * return an error
	 */
	if (best_fit < 0) {
		CRITICAL("pmem: no space left to allocate!");
#if PMEM_DEBUG
		down(&pmem[id].data_list_sem);
		format_debug_string( id, buffer, debug_bufmax );
		up(&pmem[id].data_list_sem);
		
		CRITICAL("%s",buffer);
#endif
		EPILOG("Error");
		return -1;
	}

	/* now partition the best fit:
	 * 	split the slot into 2 buddies of order - 1
	 * 	repeat until the slot is of the correct order
	 */
	while (PMEM_ORDER(id, best_fit) > (unsigned char)order) {
		int buddy;
		PMEM_ORDER(id, best_fit) -= 1;
		buddy = PMEM_BUDDY_INDEX(id, best_fit);
		PMEM_ORDER(id, buddy) = PMEM_ORDER(id, best_fit);
	}
	pmem[id].bitmap[best_fit].allocated = 1;

	/* stats */
	pmem[id].stat_curr += PMEM_LEN(id, best_fit);
	pmem[id].stat_max = MAX(pmem[id].stat_max, 
                                pmem[id].stat_curr);
	pmem[id].stat_orders[PMEM_ORDER(id, best_fit)]++;

	if (pmem[id].stat_curr == pmem[id].stat_max) {
		/* keep a snapshot of max consumption */
 		memcpy(pmem[id].stat_max_orders, 
 			pmem[id].stat_orders, 
			sizeof(pmem[id].stat_orders));
	}

	EPILOG("");
	return best_fit;
}

static pgprot_t phys_mem_access_prot(struct file *file, pgprot_t vma_prot)
{
	int id = get_id(file);
	int ret = 0;

	PROLOG("");

#ifdef pgprot_noncached
	if (pmem[id].cached == 0 || file->f_flags & O_SYNC) {
		ret = pgprot_noncached(vma_prot);
		EPILOG("");
		return ret;
	}
#endif
#ifdef pgprot_ext_buffered
	else if (pmem[id].buffered) {
		ret = pgprot_ext_buffered(vma_prot);
		EPILOG("");
		return ret;
	}
#endif

	EPILOG("");
	return vma_prot;
}

static unsigned long pmem_start_addr(int id, struct pmem_data *data)
{
	if (pmem[id].no_allocator)
		return PMEM_START_ADDR(id, 0);
	else
		return PMEM_START_ADDR(id, data->index);

}

static void *pmem_start_vaddr(int id, struct pmem_data *data)
{
	return pmem_start_addr(id, data) - pmem[id].base + pmem[id].vbase;
}

static unsigned long pmem_len(int id, struct pmem_data *data)
{
	if (pmem[id].no_allocator)
		return data->index;
	else
		return PMEM_LEN(id, data->index);
}

static int pmem_map_garbage(int id, struct vm_area_struct *vma,
			    struct pmem_data *data, unsigned long offset,
			    unsigned long len)
{
	int i, garbage_pages = len >> PAGE_SHIFT;

	PROLOG("");

	vma->vm_flags |= VM_IO | VM_RESERVED | VM_PFNMAP | VM_SHARED | VM_WRITE;
	for (i = 0; i < garbage_pages; i++) {
		if (vm_insert_pfn(vma, vma->vm_start + offset + (i * PAGE_SIZE),
				  pmem[id].garbage_pfn)) {
			EPILOG("error");
			return -EAGAIN;
		}
	}

	EPILOG("");
	return 0;
}

static int pmem_unmap_pfn_range(int id, struct vm_area_struct *vma,
				struct pmem_data *data, unsigned long offset,
				unsigned long len)
{
	int garbage_pages;
	
	PROLOG("");

	PDEBUG("unmap offset %lx len %lx", offset, len);

	BUG_ON(!PMEM_IS_PAGE_ALIGNED(len));

	garbage_pages = len >> PAGE_SHIFT;
	zap_page_range(vma, vma->vm_start + offset, len, NULL);
	pmem_map_garbage(id, vma, data, offset, len);

	EPILOG("");
	return 0;
}

static int pmem_map_pfn_range(int id, struct vm_area_struct *vma,
			      struct pmem_data *data, unsigned long offset,
			      unsigned long len)
{
	PROLOG("");

	PDEBUG("map offset %lx len %lx", offset, len);
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(vma->vm_start));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(vma->vm_end));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(len));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(offset));

	if (io_remap_pfn_range(vma, vma->vm_start + offset,
		(pmem_start_addr(id, data) + offset) >> PAGE_SHIFT,
		len, vma->vm_page_prot)) {
		EPILOG("error when mapping");
		return -EAGAIN;
	}

	EPILOG("");
	return 0;
}

static int pmem_remap_pfn_range(int id, struct vm_area_struct *vma,
			      struct pmem_data *data, unsigned long offset,
			      unsigned long len)
{
	/* hold the mm semp for the vma you are modifying when you call this */
	BUG_ON(!vma);
	zap_page_range(vma, vma->vm_start + offset, len, NULL);
	return pmem_map_pfn_range(id, vma, data, offset, len);
}



/* the following are the api for accessing pmem regions by other drivers
 * from inside the kernel */
int get_pmem_user_addr(struct file *file, unsigned long *start,
		   unsigned long *len)
{
	struct pmem_data *data;

	PROLOG("file=%p", file);

	if (!is_pmem_file(file) || !has_allocation(file)) {
#if PMEM_DEBUG
		PTRACE("pmem: requested pmem data from invalid"
		       "file.");
#endif
		EPILOG("error, invalid file");
		return -1;
	}
	data = (struct pmem_data *)file->private_data;
	down_read(&data->sem);
	if (data->vma) {
		*start = data->vma->vm_start;
		*len = data->vma->vm_end - data->vma->vm_start;
	} else {
		*start = 0;
		*len = 0;
	}
	up_read(&data->sem);

	EPILOG("");
	return 0;
}

int g_put_needed = 0;
struct file *g_file = NULL;

static int pmem_connect(unsigned long connect, struct file *file)
{
	struct pmem_data *data = (struct pmem_data *)file->private_data;
	struct pmem_data *src_data;
	struct file *src_file;
	int ret = 0, put_needed;

	PROLOG("file=%p connect on fd=%ld", file, connect);

	down_write(&data->sem);
	/* retrieve the src file and check it is a pmem file with an alloc */
	src_file = fget_light(connect, &put_needed);
	PTRACE("connect %p to %p", file, src_file);
	if (!src_file) {
		CRITICAL("pmem: src file not found!");
		ret = -EINVAL;
		goto err_no_file;
	}
	if (unlikely(!is_pmem_file(src_file) || !has_allocation(src_file))) {
		PTRACE("pmem: src file is not a pmem file or has no "
		       "alloc!");
		ret = -EINVAL;
		goto err_bad_file;
	}
	src_data = (struct pmem_data *)src_file->private_data;

	if (has_allocation(file) && (data->index != src_data->index)) {
		CRITICAL("pmem: file is already mapped but doesn't match this"
		       " src_file!");
		ret = -EINVAL;
		goto err_bad_file;
	}
	data->index = src_data->index;
	data->flags |= PMEM_FLAGS_CONNECTED;
	data->master_fd = connect;
	data->master_file = src_file;

err_bad_file:
	fput_light(src_file, put_needed);
err_no_file:
	up_write(&data->sem);

	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	return ret;
}

static void pmem_unlock_data_and_mm(struct pmem_data *data,
				    struct mm_struct *mm)
{
	PROLOG("");

	up_write(&data->sem);
	if (mm != NULL) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}

	EPILOG("");
}

static int pmem_lock_data_and_mm(struct file *file, struct pmem_data *data,
				 struct mm_struct **locked_mm)
{
	int ret = 0;
	struct mm_struct *mm = NULL;
	*locked_mm = NULL;

	PROLOG("");

lock_mm:
	down_read(&data->sem);
	if (PMEM_IS_SUBMAP(data)) {
		mm = get_task_mm(data->task);
		if (!mm) {
#if PMEM_DEBUG
			CRITICAL("pmem: can't remap task is gone!");
#endif
			up_read(&data->sem);
			EPILOG("error, can't remap task is gone");
			return -1;
		}
	}
	up_read(&data->sem);

	if (mm)
		down_write(&mm->mmap_sem);

	down_write(&data->sem);
	/* check that the file didn't get mmaped before we could take the
	 * data sem, this should be safe b/c you can only submap each file
	 * once */
	if (PMEM_IS_SUBMAP(data) && !mm) {
		pmem_unlock_data_and_mm(data, mm);
		up_write(&data->sem);
		goto lock_mm;
	}
	/* now check that vma.mm is still there, it could have been
	 * deleted by vma_close before we could get the data->sem */
	if ((data->flags & PMEM_FLAGS_UNSUBMAP) && (mm != NULL)) {
		/* might as well release this */
		if (data->flags & PMEM_FLAGS_SUBMAP) {
			put_task_struct(data->task);
			data->task = NULL;
			/* lower the submap flag to show the mm is gone */
			data->flags &= ~(PMEM_FLAGS_SUBMAP);
		}
		pmem_unlock_data_and_mm(data, mm);
		EPILOG("unsubmapped");
		return -1;
	}
	*locked_mm = mm;

	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	return ret;
}


static void pmem_revoke(struct file *file, struct pmem_data *data)
{
	struct pmem_region_node *region_node;
	struct list_head *elt, *elt2;
	struct mm_struct *mm = NULL;
	int id = get_id(file);
	int ret = 0;

	PROLOG("");
	
	data->master_file = NULL;
	ret = pmem_lock_data_and_mm(file, data, &mm);
	/* if lock_data_and_mm fails either the task that mapped the fd, or
	 * the vma that mapped it have already gone away, nothing more
	 * needs to be done */
	if (ret) {
		EPILOG("cannot lock, no action");		
		return;
	}
	/* unmap everything */
	/* delete the regions and region list nothing is mapped any more */
	if (data->vma)
		list_for_each_safe(elt, elt2, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
						 list);
			pmem_unmap_pfn_range(id, data->vma, data,
					     region_node->region.offset,
					     region_node->region.len);
			list_del(elt);
			kfree(region_node);
	}
	/* delete the master file */
	pmem_unlock_data_and_mm(data, mm);

	EPILOG("");
}

static void pmem_get_size(struct pmem_region *region, struct file *file)
{
	struct pmem_data *data = (struct pmem_data *)file->private_data;
	int id = get_id(file);

	PROLOG("file=%p", file);

	if (!has_allocation(file)) {
		region->offset = 0;
		region->len = 0;
		
		EPILOG("no allocation, returns 0");
		return;
	} else {
		region->offset = pmem_start_addr(id, data);
		region->len = pmem_len(id, data);
	}
	PDEBUG("offset %lx len %lx", region->offset, region->len);

	EPILOG("offset %lx len %lx", region->offset, region->len);
}


static int pmem_allocate_by_fd(int fd, unsigned long len)
{
	struct file *file;
	int id = 0;
	int end = 0;
	int ret = 0;
	struct pmem_data *data;
	int index;

	file = fget(fd);
	id = get_id(file);
	end = pmem[id].num_entries;
	data = (struct pmem_data *)file->private_data;

	PDEBUG("fd=%d, size=%lu", fd, len);

	down_write(&data->sem);

	/* if file->private_data == unalloced, alloc*/
	if (data && data->index == -1) {
		down_write(&pmem[id].bitmap_sem);
		index = pmem_allocate(id, len);
		up_write(&pmem[id].bitmap_sem);
		data->index = index;

		/* either no space was available or an error occured */
		if (!has_allocation(file)) {
			ret = -EINVAL;
			PDEBUG("pmem: no space or error occured.(fd=%d, size=%lu)", fd, len);
			goto error;
		}

		if (pmem_len(id, data) < len) {
			PDEBUG("pmem: allocated less than requested"
					"%lu over %lu requested", pmem_len(id, data),len);
			ret = -EINVAL;
			goto error;
		}

	} else {
		PDEBUG("already allocated...");
	}

	PDEBUG("fd=%d, allocated size=%lu", fd, pmem_len(id, data));

error:
	up_write(&data->sem);
	fput(file);
	return(ret);
}



/*******************************************************/
/*************** external interface ********************/
/*******************************************************/
#undef  DEBUG_LAYER
#define DEBUG_LAYER 0

/****************************/
/*   user-land interface    */
/****************************/
static int pmem_open(struct inode *inode, struct file *file)
{
	struct pmem_data *data;
	int id = get_id(file);
	int ret = 0;

	PROLOG("file=%p", file);

	PDEBUG("current %u file %p(%ld)", current->pid, file, file_count(file));
	/* setup file->private_data to indicate its unmapped */
	/*  you can only open a pmem device one time */
	if (file->private_data != NULL) {
		EPILOG("error, no private data");
		return -1;
	}
	data = kmalloc(sizeof(struct pmem_data), GFP_KERNEL);
	if (!data) {
		CRITICAL("pmem: unable to allocate memory for pmem metadata.");

		EPILOG("error, no more memory");
		return -1;
	}
	data->flags = 0;
	data->index = -1;
	data->task = NULL;
	data->vma = NULL;
	data->pid = 0;
	data->master_file = NULL;
	data->this_file = file;
	data->files = NULL;
	data->fd = 0;
#if PMEM_DEBUG
	data->ref = 0;
#endif
	INIT_LIST_HEAD(&data->region_list);
	init_rwsem(&data->sem);

	file->private_data = data;
	INIT_LIST_HEAD(&data->list);

	down(&pmem[id].data_list_sem);
	list_add(&data->list, &pmem[id].data_list);
	up(&pmem[id].data_list_sem);
	PDEBUG("file=%p, data=%p, id=%d", file, data, id);

	EPILOG("");
	return ret;
}


static int pmem_release(struct inode *inode, struct file *file)
{
	struct pmem_data *data = (struct pmem_data *)file->private_data;
	struct pmem_region_node *region_node;
	struct list_head *elt, *elt2;
	int id = get_id(file), ret = 0;

	PROLOG("file=%p", file);

	PDEBUG("close file=%p, data=%p, id=%d", file, data, id);

	down(&pmem[id].data_list_sem);
	/* if this file is a master, revoke all the memory in the connected
	 *  files */
	if (PMEM_FLAGS_MASTERMAP & data->flags) {
		struct pmem_data *sub_data;
		list_for_each(elt, &pmem[id].data_list) {
			sub_data = list_entry(elt, struct pmem_data, list);
			down_read(&sub_data->sem);
			if (PMEM_IS_SUBMAP(sub_data) &&
			    file == sub_data->master_file) {
				up_read(&sub_data->sem);
				pmem_revoke(file, sub_data);
			}  else
				up_read(&sub_data->sem);
		}
	}
	list_del(&data->list);
	up(&pmem[id].data_list_sem);


	down_write(&data->sem);

	/* if its not a conencted file and it has an allocation, free it */
	if (!(PMEM_FLAGS_CONNECTED & data->flags) && has_allocation(file)) {
		down_write(&pmem[id].bitmap_sem);
		ret = pmem_free_id(id, data->index);
		up_write(&pmem[id].bitmap_sem);
	} else {
		PDEBUG("connected or no allocation, skip free...");
	}

	/* if this file is a submap (mapped, connected file), downref the
	 * task struct */
	if (PMEM_FLAGS_SUBMAP & data->flags)
		if (data->task) {
			put_task_struct(data->task);
			data->task = NULL;
		}

	file->private_data = NULL;

	list_for_each_safe(elt, elt2, &data->region_list) {
		region_node = list_entry(elt, struct pmem_region_node, list);
		list_del(elt);
		kfree(region_node);
	}
	BUG_ON(!list_empty(&data->region_list));

	up_write(&data->sem);
	kfree(data);
	if (pmem[id].release)
		ret = pmem[id].release(inode, file);
	return ret;
}

static void pmem_vma_open(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct pmem_data *data = file->private_data;
	int id = get_id(file);
	/* this should never be called as we don't support copying pmem
	 * ranges via fork */
	BUG_ON(!has_allocation(file));
	down_write(&data->sem);
	/* remap the garbage pages, forkers don't get access to the data */
	pmem_unmap_pfn_range(id, vma, data, 0, vma->vm_start - vma->vm_end);
	up_write(&data->sem);
}

static void pmem_vma_close(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct pmem_data *data = file->private_data;

	//printk("current %u ppid %u file %p count %ld\n", current->pid,
	//     current->parent->pid, file, file_count(file));
	if (unlikely(!is_pmem_file(file) || !has_allocation(file))) {
		WARNING("pmem: something is very wrong, you are "
		       "closing a vm backing an allocation that doesn't "
		       "exist!\n");
		return;
	}
	down_write(&data->sem);
	if (data->vma == vma) {
		data->vma = NULL;
		if ((data->flags & PMEM_FLAGS_CONNECTED) &&
		    (data->flags & PMEM_FLAGS_SUBMAP))
			data->flags |= PMEM_FLAGS_UNSUBMAP;
	}
	/* the kernel is going to free this vma now anyway */
	up_write(&data->sem);
}

static struct vm_operations_struct vm_ops = {
	.open = pmem_vma_open,
	.close = pmem_vma_close,
};

static int pmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct pmem_data *data = NULL;
	int index;
	unsigned long vma_size =  vma->vm_end - vma->vm_start;
	int ret = 0, id = get_id(file);

	if (vma->vm_pgoff || !PMEM_IS_PAGE_ALIGNED(vma_size)) {
#if PMEM_DEBUG
		CRITICAL( "pmem: mmaps must be at offset zero, aligned"
				" and a multiple of pages_size.");
#endif
		ret = -EINVAL;
		goto error;
	}

	data = (struct pmem_data *)file->private_data;
	down_write(&data->sem);
	/* check this file isn't already mmaped, for submaps check this file
	 * has never been mmaped */
	if ((data->flags & PMEM_FLAGS_MASTERMAP) ||
	    (data->flags & PMEM_FLAGS_SUBMAP) ||
	    (data->flags & PMEM_FLAGS_UNSUBMAP)) {
#if PMEM_DEBUG
		CRITICAL( "pmem: you can only mmap a pmem file once, "
		       "this file is already mmaped. %x", data->flags);
#endif
		ret = -EINVAL;
		goto error;
	}
	/* if file->private_data == unalloced, alloc*/
	if (data && data->index == -1) {
		down_write(&pmem[id].bitmap_sem);
		index = pmem_allocate(id, vma->vm_end - vma->vm_start);
		up_write(&pmem[id].bitmap_sem);
		data->index = index;
	}
	/* either no space was available or an error occured */
	if (!has_allocation(file)) {
		ret = -EINVAL;
		CRITICAL("pmem: could not find allocation for map.");
		goto error;
	}

	if (pmem_len(id, data) < vma_size) {
#if PMEM_DEBUG
		WARNING("pmem: mmap size [%lu] does not match"
		       "size of backing region [%lu].", vma_size,
		       pmem_len(id, data));
#endif
		ret = -EINVAL;
		goto error;
	}

	vma->vm_pgoff = pmem_start_addr(id, data) >> PAGE_SHIFT;
	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_page_prot);

	if (data->flags & PMEM_FLAGS_CONNECTED) {
		struct pmem_region_node *region_node;
		struct list_head *elt;
		if (pmem_map_garbage(id, vma, data, 0, vma_size)) {
			CRITICAL("pmem: mmap failed in kernel!");
			ret = -EAGAIN;
			goto error;
		}
		list_for_each(elt, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
						 list);
			PDEBUG("remapping file: %p %lx %lx", file,
				region_node->region.offset,
				region_node->region.len);
			if (pmem_remap_pfn_range(id, vma, data,
						 region_node->region.offset,
						 region_node->region.len)) {
				ret = -EAGAIN;
				goto error;
			}
		}
		data->flags |= PMEM_FLAGS_SUBMAP;
		get_task_struct(current->group_leader);
		data->task = current->group_leader;
		data->vma = vma;
#if PMEM_DEBUG
		data->pid = current->pid;
#endif
		PDEBUG("submmapped file %p vma %p pid %u", file, vma,
		     current->pid);
	} else {
		if (pmem_map_pfn_range(id, vma, data, 0, vma_size)) {
			WARNING("pmem: mmap failed in kernel!");
			ret = -EAGAIN;
			goto error;
		}
		data->flags |= PMEM_FLAGS_MASTERMAP;
		data->pid = current->pid;
	}
	vma->vm_ops = &vm_ops;

error:
	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	up_write(&data->sem);
	return ret;
}

#define CMD_TO_STR(c)						\
            ((c == PMEM_GET_PHYS)? "GET_PHYS":			\
             (c == PMEM_MAP)?"MAP":				\
             (c == PMEM_UNMAP)?"UNMAP":				\
             (c == PMEM_GET_SIZE)?"GET SIZE":			\
             (c == PMEM_GET_TOTAL_SIZE)?"GET TOTAL SIZE":	\
             (c == PMEM_ALLOCATE)?"ALLOCATE":			\
             (c == PMEM_CONNECT)?"CONNECT":			\
             "!Unknown command!")

static long pmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pmem_data *data;
	int id = get_id(file);
	int ret = 0;

	PROLOG("cmd=%s", CMD_TO_STR(cmd));

	switch (cmd) {
	case PMEM_GET_PHYS:
		{
			struct pmem_region region;
			PDEBUG("get_phys");
			if (!has_allocation(file)) {
				region.offset = 0;
				region.len = 0;
			} else {
				data = (struct pmem_data *)file->private_data;
				region.offset = pmem_start_addr(id, data);
				region.len = pmem_len(id, data);
			}
			PTRACE("pmem: request for physical address of pmem region "
					"from process %d.", current->pid);
			if (copy_to_user((void __user *)arg, &region,
					 sizeof(struct pmem_region))) {
				CRITICAL("copy_to_user failed");
				ret = -EFAULT;
				goto error;
			}
			break;
		}
	case PMEM_MAP:
		{
			struct pmem_region region;
			if (copy_from_user(&region, (void __user *)arg,
						sizeof(struct pmem_region)))
				ret = -EFAULT;
			data = (struct pmem_data *)file->private_data;
			return pmem_remap(&region, file, PMEM_MAP);
		}
		break;
	case PMEM_UNMAP:
		{
			struct pmem_region region;
			if (copy_from_user(&region, (void __user *)arg,
						sizeof(struct pmem_region)))
				ret = -EFAULT;
			data = (struct pmem_data *)file->private_data;
			return pmem_remap(&region, file, PMEM_UNMAP);
			break;
		}
	case PMEM_GET_SIZE:
		{
			struct pmem_region region;
			PDEBUG("get_size");
			pmem_get_size(&region, file);
			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct pmem_region)))
				ret = -EFAULT;
			break;
		}
	case PMEM_GET_TOTAL_SIZE:
		{
			struct pmem_region region;
			PDEBUG("get total size");
			region.offset = 0;
			get_id(file);
			region.len = pmem[id].size;
			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct pmem_region)))
				ret = -EFAULT;
			break;
		}
	case PMEM_ALLOCATE:
		{
			if (has_allocation(file))
				ret = -EINVAL;goto error;;
			data = (struct pmem_data *)file->private_data;
			data->index = pmem_allocate(id, arg);
			break;
		}
	case PMEM_CONNECT:
		PDEBUG("connect");
		return pmem_connect(arg, file);
		break;
	default:
		if (pmem[id].ioctl)
			return pmem[id].ioctl(file, cmd, arg);
		ret = -EINVAL;goto error;;
	}
	return 0;

error:
	EPILOG("error");
	return(ret);
}

#if PMEM_DEBUG
static ssize_t debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t format_debug_string(int id, char *buffer, size_t bufsize)
{
	struct list_head *elt, *elt2;
	struct pmem_data *data;
	struct pmem_region_node *region_node;
	int n = 0;
	int i = 0;
	int prev_order = -1;
	int order = 0;

	PROLOG("");

	/* general */
	n  = scnprintf(buffer, bufsize, "[general]\n"
			" cache = %s\n"
			" size  = %08lu/%08lu/%08lu (current/max/total)\n",
			((pmem[id].cached)?"yes":"no"),
			pmem[id].stat_curr,
			pmem[id].stat_max,
			pmem[id].size);

	if (pmem[id].stat_max > 0) {
		/* max blocks summary */
		n += scnprintf(buffer + n, bufsize - n, 
			       "[max blocks], %luK/%luK divided in\n ",
				(pmem[id].stat_max/1024),
				(pmem[id].size)/1024);
		for (i=0; i<32; i++) {
			if (pmem[id].stat_max_orders[i] > 0) {
				n += scnprintf(buffer + n, bufsize - n, 
					"%ldx%ldK ",
					pmem[id].stat_max_orders[i],
					(((1 << i) * PMEM_MIN_ALLOC))/1024);
			}
		}
		n += scnprintf(buffer + n, bufsize - n, "\n");
	}

	if (pmem[id].stat_curr > 0) {
		/* blocks summary */
		n += scnprintf(buffer + n, bufsize - n, 
			       "[current blocks], %luK/%luK divided in\n ",
	  (pmem[id].stat_curr)/1024,
	   (pmem[id].size)/1024);
		for (i=0; i<32; i++) {
			if (pmem[id].stat_orders[i] > 0) {
				n += scnprintf(buffer + n, bufsize - n, 
					       "%ldx%ldK ",
	    pmem[id].stat_orders[i],
     (((1 << i) * PMEM_MIN_ALLOC))/1024);
			}
		}
		n += scnprintf(buffer + n, bufsize - n, 
			       "\n");

		/* fragmentation */
		n += scnprintf(buffer + n, bufsize - n, 
			       "[fragmentation]");
		for (i=0; i<pmem[id].num_entries; i++) {
			if (pmem[id].bitmap[i].allocated) {
				order = pmem[id].bitmap[i].order;
				if (order != prev_order) {
					n += scnprintf(buffer + n, bufsize - n, 
							"\n %luK(o=%d): ",
       (PMEM_LEN(id, i)/1024),
	order);
					prev_order = order;

				}
				n += scnprintf(buffer + n, bufsize - n, 
					       "%d ", i);
			}
		}
		n += scnprintf(buffer + n, bufsize - n, 
			       "\n");

		/* details */
		n += scnprintf(buffer + n, bufsize - n, 
			       "[details]\n pid file#: mapped regions (offset, len) (offset,len)...\n");
		list_for_each(elt, &pmem[id].data_list) {
			data = list_entry(elt, struct pmem_data, list);
			if(down_read_trylock(&data->sem)) {
				n += scnprintf(buffer + n, bufsize - n, "pid=%u file=%p:",
					data->pid, data->this_file);
				n += scnprintf(buffer + n, bufsize - n,
					"(phy=%lx,log=%p,virt=%p,size=%lu) ",
					pmem_start_addr(id, data),
				__va(pmem_start_addr(id, data)),
					pmem_start_vaddr(id, data),
							pmem_len(id, data)
					);
				list_for_each(elt2, &data->region_list) {
					region_node = list_entry(elt2, struct pmem_region_node,
							list);
					n += scnprintf(buffer + n, bufsize - n,
						"(%lx,%lx) ",
						region_node->region.offset, region_node->region.len);
				}
				n += scnprintf(buffer + n, bufsize - n, "\n");
				up_read(&data->sem);
			}
			else {
				n += scnprintf(buffer + n, bufsize - n, "pid=%u file=%p:",
					       data->pid, data->this_file);
				n += scnprintf(buffer + n, bufsize - n,
						"(down_read_trylock failed)\n");
			}
		}
	}
	n++;
	buffer[n] = '\0';
	EPILOG("");
	return n;
}


static ssize_t debug_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int id = (int)file->private_data;
	const int debug_bufmax = 4096;
	static char buffer[4096];
	size_t n = 0;
	
	PDEBUG("debug open");

	down(&pmem[id].data_list_sem);

	n = format_debug_string( id, buffer, debug_bufmax );

	up(&pmem[id].data_list_sem);

	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static struct file_operations debug_fops = {
	.read = debug_read,
	.open = debug_open,
};
#endif

/****************************/
/*  kernel-land interface   */
/****************************/

int pmem_alloc_flags(unsigned long size, 
		     int* pfd,
		     struct files_struct** pfiles,
		     unsigned long* pallocated_size,
		     unsigned long* pphysical,
		     unsigned long* plogical,
		     int flags)
{
	int ret = 0;
	mm_segment_t restore_fs = 0;
	int fd = 0;
	int fs_forced = 0;
	struct file * file= NULL;
	unsigned long allocated_size = 0;
	unsigned long physical = 0;
	unsigned long logical = 0;
	unsigned long virtual = 0;
	struct pmem_data *data = NULL;
	int id = 0;

	PROLOG("size=%ld flags=%x", size, flags);

	/* init */
	(*pfd) = 0;
	(*pfiles) = NULL;
	(*pallocated_size) = 0;
	(*pphysical) = 0;
	(*plogical)  = 0;

	/* check */
	if (size == 0) {
		CRITICAL("pmem: 0 size");
		ret = -EINVAL;
		goto bail;
	}
	if (!pfd) {
		CRITICAL("pmem: NULL fd pointer");
		ret = -EINVAL;
		goto bail;
	}
	if (!pfiles) {
		CRITICAL("pmem: NULL files pointer");
		ret = -EINVAL;
		goto bail;
	}
	if (!pallocated_size) {
		CRITICAL("pmem: NULL allocated size pointer");
		ret = -EINVAL;
		goto bail;
	}
	if (!pphysical) {
		CRITICAL("pmem: NULL physical pointer");
		ret = -EINVAL;
		goto bail;
	}
	if (!plogical) {
		CRITICAL("pmem: NULL logical pointer");
		ret = -EINVAL;
		goto bail;
	}

retry:
	/* try to open a new pmem file */
	if (flags & PMEM_FLAGS_CACHE) {
		fd = sys_open("/dev/pmem", O_RDWR, 0);
	} else {
		/* 0_SYNC permission will lead to automatic cache
              on this area */
		fd = sys_open("/dev/pmem", O_RDWR | O_SYNC, 0);
	}
	if (fd < 0) {
		if (!fs_forced) {
			if (fd == -EFAULT) {
				PDEBUG("pmem file open failure, try forcing FS");
				restore_fs = get_fs();
				set_fs(KERNEL_DS);
				fs_forced = 1;
				goto retry;
			} else {
			}
		} else {
			CRITICAL("pmem: cannot open file even by forcing FS");
			ret = -EBADF;
			goto bail;
		}

		CRITICAL("pmem: cannot open file (err=%d)", fd);
		ret = -EBADF;
		goto bail;
	}

	/* restore FS if forced */
	if (fs_forced) {
		set_fs(restore_fs);
	}

	/* allocate */
	ret = pmem_allocate_by_fd(fd, size);
	if (ret) {
		CRITICAL("pmem: cannot allocate file size=%ld (err=%d)", size, ret);
		goto bail;
	}
	ret = get_pmem_file(fd, &physical, &virtual, &allocated_size, &file);
	if (ret) {
		CRITICAL("pmem: cannot get file (err=%d)", ret);        
		goto bail;
	}

	/* for pmem_free checks */
	data = (struct pmem_data *)file->private_data;
	data->files = current->files;
	data->fd = fd;

	/* check cache coherency */
	id = get_id(file);
	if ((!pmem[id].cached) &&
	    (flags & PMEM_FLAGS_CACHE)) {
		WARNING("pmem: try to allocate cacheable memory on a non-cacheable device");
	}
	

	put_pmem_file(file);

	/* phy => log */
	logical = (unsigned long)__va(physical);

	/* output */
	(*pfd) = fd;
	(*pfiles) = current->files;    /* restore it  @ free time if needed */
	(*pallocated_size) = allocated_size;
	(*pphysical) = physical;
	(*plogical)  = logical;

	PTRACE("alloc fd=%d, file=%p, physical=0x%lx, size=%ld, allocated size=%ld, files=%p",
	       (*pfd),
	       file,
	       (*pphysical),
	       size,
	       (*pallocated_size),
	       (*pfiles));

bail:
	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("fd=%d, file=%p, physical=0x%lx, size=%ld, allocated size=%ld",
		       (*pfd),
		       file,
		       (*pphysical),
		       size,
			(*pallocated_size));
	}
	return(ret);
}
EXPORT_SYMBOL(pmem_alloc_flags);

int pmem_alloc(unsigned long size, 
               int* pfd,
               struct files_struct** pfiles,
               unsigned long* pallocated_size,
               unsigned long* pphysical,
               unsigned long* plogical)
{
  int ret = 0;

  PROLOG("size=%ld", size);
  ret = pmem_alloc_flags(size,
			 pfd,
			 pfiles,
			 pallocated_size,
			 pphysical,
			 plogical,
			 PMEM_FLAGS_NCACHE);
  return (ret);
  EPILOG("");
}
EXPORT_SYMBOL(pmem_alloc);

int pmem_free(int fd, struct files_struct* files)
{
	int ret = 0;
	struct files_struct* restore_files = NULL;
	struct list_head *elt = NULL;
	struct pmem_data *data = NULL;
	int found = 0;
	int id = 0;

	PROLOG("fd=%d", fd);

	if (!files) {
		CRITICAL("pmem: NULL files");
		ret = -EINVAL;
		goto bail;
	}

	/* check if already closed */
	for (id=0; id < id_count; id++) {
		list_for_each(elt, &pmem[id].data_list) {
			data = list_entry(elt, struct pmem_data, list);
			
			if ((data->files == files) &&
			    (data->fd == fd)) {
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		PDEBUG("already closed, exit");
		goto bail;
	}

	/* if try to close while not in same
	   context than open, force files */
	if (files != current->files) {
		PDEBUG("force files: curr=%p, restore=%p", current->files, files);
		restore_files = current->files;
		current->files = files;
	}

	/* close */
	ret = sys_close(fd);
	if (ret) {
		CRITICAL("pmem: cannot close file even by forcing FS");
		goto bail;
	}

	/* restore files if forced */
	if (restore_files) {
		current->files = restore_files;
	}

bail:
	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	return(ret);
}
EXPORT_SYMBOL(pmem_free);

int pmem_mmap_by_fd(int fd, struct files_struct* files, 
                    struct vm_area_struct* vma)
{
	int ret = 0;
	struct files_struct* restore_files = NULL;
	struct file *file = NULL;
	
	file = fget(fd);
	
	PROLOG("fd=%d, file=%p", fd, file);
	
	PDEBUG("mmap fd=%d, files=%p", fd, current->files);
	
	/* if try to mmap while not in same
	   context than open, force files */
	if (files != current->files) {
		restore_files = current->files;
		current->files = files;
	}
	
	/* mmap */
	ret = pmem_mmap(file, vma);
	if (ret) {
		CRITICAL("pmem: cannot mmap");
		goto bail;
	}
	
	/* restore files if forced */
	if (restore_files) {
		current->files = restore_files;
	}
	
bail:
	fput(file);
	
	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	return(ret);
}
EXPORT_SYMBOL(pmem_mmap_by_fd);


int get_pmem_addr(struct file *file, unsigned long *start,
		  unsigned long *vstart, unsigned long *len)
{
	struct pmem_data *data;
	int id;

	PROLOG("file=%p", file);

	if (!is_pmem_file(file) || !has_allocation(file)) {
		PTRACE("not pmem file or no allocation");
		EPILOG("error, no pmem file or no allocation");
		return -1;
	}

	data = (struct pmem_data *)file->private_data;
	if (data->index == -1) {
#if PMEM_DEBUG
		PTRACE("pmem: requested pmem data from file with no "
		       "allocation.");
		EPILOG("error, no allocation");
		return -1;
#endif
	}
	id = get_id(file);

	down_read(&data->sem);
	*start = pmem_start_addr(id, data);
	*len = pmem_len(id, data);
	*vstart = (unsigned long)pmem_start_vaddr(id, data);
	up_read(&data->sem);
#if PMEM_DEBUG
	down_write(&data->sem);
	data->ref++;
	up_write(&data->sem);
#endif

	EPILOG("");
	return 0;
}
EXPORT_SYMBOL(get_pmem_addr);

int get_pmem_file(int fd, unsigned long *start, unsigned long *vstart,
		  unsigned long *len, struct file **filp)
{
	struct file *file;

	file = fget(fd);

	PROLOG("fd=%d, file=%p", fd, file);

	if (unlikely(file == NULL)) {
		PTRACE("pmem: requested data from file descriptor "
		       "that doesn't exist.");
		EPILOG("error, file doesn't exist");
		return -1;
	}

	if (get_pmem_addr(file, start, vstart, len))
		goto end;

	if (filp)
		*filp = file;

	EPILOG("");
	return 0;
end:
	fput(file);

	EPILOG("error");
	return -1;
}
EXPORT_SYMBOL(get_pmem_file);

void put_pmem_file(struct file *file)
{
	struct pmem_data *data;
	int id;

	PROLOG("file=%p", file);

	if (!is_pmem_file(file)) {
		EPILOG("not a pmem file");
		return;
	}
	id = get_id(file);
	data = (struct pmem_data *)file->private_data;
#if PMEM_DEBUG
	down_write(&data->sem);
	if (data->ref == 0) {
		CRITICAL("pmem: pmem_put > pmem_get %s (pid %d)",
		       pmem[id].dev.name, data->pid);
		BUG();
	}
	data->ref--;
	up_write(&data->sem);
#endif
	fput(file);

	EPILOG("");
}
EXPORT_SYMBOL(put_pmem_file);

void flush_pmem_file(struct file *file, unsigned long offset, unsigned long len)
{
	struct pmem_data *data;
	int id;
	void *vaddr;
	void *flush_start, *flush_end;

	PROLOG("file=%p off=%ld, len=%ld", file, offset, len);

	if (!is_pmem_file(file) || !has_allocation(file)) {
		EPILOG("error: not a pmem file or no allocation");
		return;
	}

	id = get_id(file);
	data = (struct pmem_data *)file->private_data;
	if (!pmem[id].cached || file->f_flags & O_SYNC) {
		EPILOG("cache not activated, no action");
		return;
	}

	down_read(&data->sem);
	vaddr = pmem_start_vaddr(id, data);

	/* Flush only the requested offset and size */
	flush_start = vaddr + offset;
	flush_end = flush_start + len;
	dmac_flush_range(flush_start, flush_end);

	up_read(&data->sem);
	EPILOG("");
}
EXPORT_SYMBOL(flush_pmem_file);

int pmem_remap(struct pmem_region *region, struct file *file,
		      unsigned operation)
{
	int ret;
	struct pmem_region_node *region_node;
	struct mm_struct *mm = NULL;
	struct list_head *elt, *elt2;
	int id = get_id(file);
	struct pmem_data *data = (struct pmem_data *)file->private_data;

	PROLOG("file=%p, off=%ld, len=%ld", 
	       file,
	       region->offset,
	       region->len);

	/* pmem region must be aligned on a page boundry */
	if (unlikely(!PMEM_IS_PAGE_ALIGNED(region->offset) ||
		 !PMEM_IS_PAGE_ALIGNED(region->len))) {
#if PMEM_DEBUG
		CRITICAL("pmem: request for unaligned pmem suballocation "
		       "%lx %lx", region->offset, region->len);
#endif
		ret = -EINVAL;
		goto err;
	}

	/* if userspace requests a region of len 0, there's nothing to do */
	if (region->len == 0) {
		EPILOG("0 length, no action");
		return 0;
	}

	/* lock the mm and data */
	ret = pmem_lock_data_and_mm(file, data, &mm);
	if (ret) {
		EPILOG("cannot lock, no action");
		return 0;
	}

	/* only the owner of the master file can remap the client fds
	 * that back in it */
	if (!is_master_owner(file)) {
#if PMEM_DEBUG
		CRITICAL("pmem: remap requested from non-master process\n");
#endif
		ret = -EINVAL;
		goto err;
	}

	/* check that the requested range is within the src allocation */
	if (unlikely((region->offset > pmem_len(id, data)) ||
		     (region->len > pmem_len(id, data)) ||
		     (region->offset + region->len > pmem_len(id, data)))) {
#if PMEM_DEBUG
		WARNING("pmem: suballoc doesn't fit in src_file!");
#endif
		ret = -EINVAL;
		goto err;
	}

	if (operation == PMEM_MAP) {
		region_node = kmalloc(sizeof(struct pmem_region_node),
			      GFP_KERNEL);
		if (!region_node) {
			ret = -ENOMEM;
#if PMEM_DEBUG
			PTRACE("No space to allocate metadata!");
#endif
			goto err;
		}
		region_node->region = *region;
		list_add(&region_node->list, &data->region_list);
	} else if (operation == PMEM_UNMAP) {
		int found = 0;
		list_for_each_safe(elt, elt2, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
				      list);
			if (region->len == 0 ||
			    (region_node->region.offset == region->offset &&
			    region_node->region.len == region->len)) {
				list_del(elt);
				kfree(region_node);
				found = 1;
			}
		}
		if (!found) {
#if PMEM_DEBUG
			CRITICAL("pmem: Unmap region does not map any mapped "
				"region!");
#endif
			ret = -EINVAL;
			goto err;
		}
	}

	if (data->vma && PMEM_IS_SUBMAP(data)) {
		if (operation == PMEM_MAP)
			ret = pmem_remap_pfn_range(id, data->vma, data,
						   region->offset, region->len);
		else if (operation == PMEM_UNMAP)
			ret = pmem_unmap_pfn_range(id, data->vma, data,
						   region->offset, region->len);
	}

err:
	pmem_unlock_data_and_mm(data, mm);

	if (ret) {
		EPILOG("error %d", ret);
	} else {
		EPILOG("");
	}
	return ret;
}
EXPORT_SYMBOL(pmem_remap);


#if 0
static struct miscdevice pmem_dev = {
	.name = "pmem",
	.fops = &pmem_fops,
};
#endif

int pmem_setup(struct android_pmem_platform_data *pdata,
	       long (*ioctl)(struct file *, unsigned int, unsigned long),
	       int (*release)(struct inode *, struct file *))
{
	int err = 0;
	int i, index = 0;
	int id = id_count;
	id_count++;

	PROLOG("");

	pmem[id].no_allocator = pdata->no_allocator;
	pmem[id].cached = pdata->cached;
	pmem[id].buffered = pdata->buffered;
	pmem[id].base = pdata->start;
	pmem[id].size = pdata->size;
	pmem[id].ioctl = ioctl;
	pmem[id].release = release;
	init_rwsem(&pmem[id].bitmap_sem);
	init_MUTEX(&pmem[id].data_list_sem);
	INIT_LIST_HEAD(&pmem[id].data_list);
	pmem[id].dev.name = pdata->name;
	pmem[id].dev.minor = id;
	pmem[id].dev.fops = &pmem_fops;
	PTRACE("%s: %d init", pdata->name, pdata->cached);

	/* stats */
	pmem[id].stat_curr = 0;
	pmem[id].stat_max  = 0;

	err = misc_register(&pmem[id].dev);
	if (err) {
		CRITICAL("Unable to register pmem driver");
		goto err_cant_register_device;
	}
	pmem[id].num_entries = pmem[id].size / PMEM_MIN_ALLOC;

	pmem[id].bitmap = kmalloc(pmem[id].num_entries *
				  sizeof(struct pmem_bits), GFP_KERNEL);
	if (!pmem[id].bitmap) {
		CRITICAL("No more memory for metadata");
		goto err_no_mem_for_metadata;
	}

	memset(pmem[id].bitmap, 0, sizeof(struct pmem_bits) *
					  pmem[id].num_entries);

	for (i = sizeof(pmem[id].num_entries) * 8 - 1; i >= 0; i--) {
		if ((pmem[id].num_entries) &  1<<i) {
			PMEM_ORDER(id, index) = i;
			index = PMEM_NEXT_INDEX(id, index);
		}
	}

	if (pmem[id].cached)
		pmem[id].vbase = ioremap_cached(pmem[id].base,
						pmem[id].size);
#ifdef ioremap_ext_buffered
	else if (pmem[id].buffered)
		pmem[id].vbase = ioremap_ext_buffered(pmem[id].base,
						      pmem[id].size);
#endif
	else
		pmem[id].vbase = ioremap(pmem[id].base, pmem[id].size);

	if (pmem[id].vbase == 0) {
		CRITICAL("Cannot remap");
		goto error_cant_remap;
	}

	pmem[id].garbage_pfn = page_to_pfn(alloc_page(GFP_KERNEL));
	if (pmem[id].no_allocator)
		pmem[id].allocated = 0;

#if PMEM_DEBUG
	debugfs_create_file(pdata->name, S_IFREG | S_IRUGO, NULL, (void *)id,
			    &debug_fops);
#endif

	/* init stats */
	for (i=0; i<32; i++) {
		pmem[id].stat_orders[i]=0;
		pmem[id].stat_max_orders[i]=0;
	}

	return 0;
error_cant_remap:
	kfree(pmem[id].bitmap);
err_no_mem_for_metadata:
	misc_deregister(&pmem[id].dev);
err_cant_register_device:
	return -1;
}

static int pmem_probe(struct platform_device *pdev)
{
	struct android_pmem_platform_data *pdata;
	int ret = 0;


	if (!pdev || !pdev->dev.platform_data) {
		CRITICAL("Unable to probe pmem!");
		ret = -1;
		goto error;
	}
	pdata = pdev->dev.platform_data;
	ret = pmem_setup(pdata, NULL, NULL);
	if (ret)
		goto error;
	
	EPILOG("");
	return 0;

error:
	EPILOG("error");
	return ret;
}


static int pmem_remove(struct platform_device *pdev)
{
	int id = pdev->id;
	__free_page(pfn_to_page(pmem[id].garbage_pfn));
	misc_deregister(&pmem[id].dev);
	return 0;
}

static struct platform_driver pmem_driver = {
	.probe = pmem_probe,
	.remove = pmem_remove,
	.driver = { .name = "android_pmem" }
};


static int __init pmem_init(void)
{
	int ret = 0;

	PROLOG("");
	ret = platform_driver_register(&pmem_driver);
	if (ret) {
		CRITICAL("platform_driver_register err=%d", ret);
		goto error;
	}

	EPILOG("");
	return 0;

error:
	EPILOG("error");
	return ret;
}

static void __exit pmem_exit(void)
{
	platform_driver_unregister(&pmem_driver);
}

module_init(pmem_init);
module_exit(pmem_exit);

