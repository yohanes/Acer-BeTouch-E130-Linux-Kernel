/**
 *
 * hwmem_alloc.c: contiguous memory buffers pool allocator
 * and its user space interface
 *
 * Modified by ST-Ericsson
 * Copyright (c) Purple Labs SA 2006  <freesoftware@purplelabs.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#define _hwmem_alloc_c_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>

#include <mach/hwmem_api.h>

/** device & module file name */
#define MODULE_NAME  "hwmem"

/** device major number */
#define HWMEM_MAJOR         37  /* major borrowed to IDE tape */
#define HWMEM_MINOR          0

/* Module parameters:
 * @pools description of the form "size1xnb1, size2xnb2, ... "*/

/****************************************************************************
 *
 *          QVGA Configuration
 *
 ****************************************************************************/
#ifdef CONFIG_HWMEM_QVGA
static char * pools =
    "4096x20," /* MPEG4 enc lib = "MEM_ID_ALGO"          1x2848 +
                  MPEG4 dec lib = "sharedHWSW"          19x3776 */
    "8192x61," /* MPEG4 dec lib = "sharedHWSW"          59x4992 +
                  MPEG4 enc lib = "MEM_ID_TABLEPCB_MV"   2x6336  */
   "12288x10," /* IPP binary shape on QVGA 16bit =      10x9664 */
   "20480x03," /* IPP "big" (jpeg) binary shape =        3x17344 */
   "32768x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_PACK" 1x32768 */
   "49152x10," /* IPP object alloc on QVGA 16bit =      10x46280 */
   "81920x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_PACK" 1x81920 */
   "90112x03," /* IPP "big" (jpeg) object alloc  =       3x87370 */
  "152064x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BM"   1x152064 */
  "262144x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BSG"  1x262144 */
#ifdef CONFIG_FB_LCDBUS_TVOUT
  "462848x02," /* 2x FB (240x320) RGB888_24 double buffering (LCD & TVOUT) */
#else
  "307200x01," /* 1x FB (240x320) RGB565_16 double buffering (LCD) */
#endif
  "524288x01" /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BSG"  1x524288 */
    ;

#else /* CONFIG_HWMEM_QVGA */

/****************************************************************************
 *
 *          WQVGA Configuration (TO BE UPDATED)
 *
 ****************************************************************************/
#ifdef CONFIG_HWMEM_WQVGA
static char * pools =
    "4096x20," /* MPEG4 enc lib = "MEM_ID_ALGO"          1x2848 +
                  MPEG4 dec lib = "sharedHWSW"          19x3776 */
    "8192x61," /* MPEG4 dec lib = "sharedHWSW"          59x4992 +
                  MPEG4 enc lib = "MEM_ID_TABLEPCB_MV"   2x6336  */
   "12288x10," /* IPP binary shape on QVGA 16bit =      10x9664 */
   "20480x03," /* IPP "big" (jpeg) binary shape =        3x17344 */
   "32768x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_PACK" 1x32768 */
   "49152x10," /* IPP object alloc on QVGA 16bit =      10x46280 */
   "81920x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_PACK" 1x81920 */
   "90112x03," /* IPP "big" (jpeg) object alloc  =       3x87370 */
  "152064x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BM"   1x152064 */
  "262144x01," /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BSG"  1x262144 */
#ifdef CONFIG_FB_LCDBUS_TVOUT
  "462848x02," /* 2x FB (240x320) RGB888_24 double buffering (LCD & TVOUT) */
#else
  "307200x01," /* 1x FB (240x320) RGB565_16 double buffering (LCD) */
#endif
  "524288x01" /* MPEG4 enc lib = "MEM_ID_TABLEPCB_BSG"  1x524288 */
    ;
#else  /* CONFIG_HWMEM_WQVGA */

/****************************************************************************
 *
 *          Other Configurations to be added here
 *
 ****************************************************************************/



/****************************************************************************
 *
 *          No configuration defined ==> Compilation error
 *
 ****************************************************************************/	
#error "No HWMEM allocator configuration defined !"

#endif /* CONFIG_HWMEM_WQVGA */
#endif /* CONFIG_HWMEM_QVGA */


module_param(pools, charp, 0444);
MODULE_PARM_DESC(pools, "pools description form \"size1x#1,size2x#2, ...\"");

/** buffers pool type */
typedef struct
{
	struct list_head list;
	u32 size;
	u16 count;
	u16 useCount;
	u16 maxUse;
	spinlock_t lock;
	t_hwmem_Buffer *pBuffers;
} t_hwmem_Pool;

/* Preallocated contiguous memory pools sizes sorted list by increasing size */
static LIST_HEAD(sPoolsList);

#ifdef HWMEM_DEBUG
#define	dbg(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __func__, ## args)
#else
#define	dbg(fmt, args...) do{}while(0)
#endif

/** List of requested buffers indexed by their userland handle */
t_hwmem_Buffer *gHwmem_ReqUserBuffer[HWMEM_MAX_USER_HANDLE];
EXPORT_SYMBOL(gHwmem_ReqUserBuffer);

/** index of "probably" first unused handle in ReqUserBuffer table */
static int gHwmem_ReqBufferIndex = 1;

static int hwmem_proc_show(struct seq_file *m, void *v);

/** Utilities functions ---------------------------------------------------- */

static int allocPoolAndBuf(int size, int nb)
{
	int i;
	t_hwmem_Pool * pool;
	static size_t boff = 0;  /* Buffer offset for mmap */

	pool = kzalloc(sizeof(t_hwmem_Pool), GFP_KERNEL);
	if (!pool) return -ENOMEM;
	spin_lock_init(&pool->lock);
	pool->size = PAGE_ALIGN(size); /* Align on page for mmap */
	pool->count = nb;
	pool->pBuffers = kcalloc(nb, sizeof(t_hwmem_Buffer), GFP_KERNEL);
	if (!pool->pBuffers)
	{
		kfree(pool);
		return -ENOMEM;
	}

	printk(KERN_INFO "%d buffers of size %d (aligned size: %dk)\n", nb, size,
			pool->size>>10);

	for (i=0 ; i<nb ; i++)
	{
		/* Alloc DMA enabled contiguous memory */
		pool->pBuffers[i].pVirtualAddr = dma_alloc_coherent(NULL,
				pool->size, &(pool->pBuffers[i].physAddr),
				GFP_KERNEL | GFP_DMA);
		if (pool->pBuffers[i].pVirtualAddr)
		{
			pool->pBuffers[i].magic = HWMEM_BUFF_MAGIC;
			pool->pBuffers[i].lock = 0; /* default, buffer isn't locked */
			pool->pBuffers[i].boff = boff; /* Init buffer offset  for mmap */
			boff += pool->size;  /* Next buffer offset */
		}
		else
		{
			printk(KERN_ERR "HWMEM ERROR: failed to allocate %d bytes contiguous buffer !\nIncrease CONSISTENT_DMA_SIZE in platform memory.h file\n" ,pool->size);
			BUG(); /* Force quit with an exception */
		}
	}

	if (list_empty(&sPoolsList))
	{
		list_add(&(pool->list), &sPoolsList);
	}
	else
	{
		struct list_head *elt;

		/* Add this pool in sPoolsList sorted in ascended buffer size */
		list_for_each(elt, &sPoolsList)
		{
			t_hwmem_Pool * d;
			d = list_entry(elt, t_hwmem_Pool, list);
			if (pool->size < d->size)
				break;
		}
		list_add(&(pool->list), elt->prev);
	}

	return 0;
}

static t_hwmem_Buffer * findHwmemBufFromVirtAddr(void *virtAddr)
{
	int i;
	t_hwmem_Pool * pool;
	struct list_head *elt;

	list_for_each(elt, &sPoolsList)
	{
		pool = list_entry(elt, t_hwmem_Pool, list);
		spin_lock(&pool->lock);
		for (i=0 ; i<pool->count ; i++)
		{
			if (virtAddr == pool->pBuffers[i].pVirtualAddr)
			{
				spin_unlock(&pool->lock);
				return &(pool->pBuffers[i]);
			}
		}
		spin_unlock(&pool->lock);
	}
	/* Failed to find suitable block */
	return NULL;
}

/** API functions ---------------------------------------------------------- */

/**
 * @brief Allocate memory from existing pool.
 * @param size size of the memory block required
 * @return the virtual kernel address of the buffer
 *         or NULL if there is not enough remaining memory in pools
 *
 * We use simple iterate algorithm to find suitable memory block,
 * we could use RB tree here but for few blocks, the gain will be thick.
 */
void* hwmem_alloc(u32 size)
{
	int i;
	t_hwmem_Pool * pool;
	struct list_head *elt;

	list_for_each(elt, &sPoolsList)
	{
		pool = list_entry(elt, t_hwmem_Pool, list);
		if (pool->size<size)
			continue;
		spin_lock(&pool->lock);
		for (i=0 ; i<pool->count ; i++)
		{
			if (!pool->pBuffers[i].inUse)
			{
				pool->pBuffers[i].inUse = 1;
				pool->pBuffers[i].usedSize = size;
				pool->pBuffers[i].pool = pool;
				if (++pool->useCount > pool->maxUse) /* For statistic */
					pool->maxUse = pool->useCount;
				spin_unlock(&pool->lock);
				return pool->pBuffers[i].pVirtualAddr;
			}
		}
		spin_unlock(&pool->lock);
	}

	/* Failed to find suitable block (Dump memory status) */
	hwmem_proc_show(NULL, NULL);

	return NULL;
}

EXPORT_SYMBOL(hwmem_alloc);

/**
 * @brief The size of the memory block pointed to by the buf parameter is
 *        changed to size parameter, expanding or reducing the amount of 
 *        memory available in the block
 * @param buf  pointer to a memory block previously allocated with hwmem_alloc
 *			   if this is NULL a new block is allocated and a pointer to it 
 *			   is returned by the function
 * @param size the new size of the memory block
 *			   if it is 0 the memory block pointed by buf is deallocated
 *			   and a NULL pointer is returned
 * @return - the SAME virtual kernel address of the buffer
 *         - NULL if there is not enough remaining memory
 *         - NULL if the memory block is deallocated 
 */
void* hwmem_realloc(void *buf, u32 size)
{
	void *res = NULL;

	if (size == 0) {
		hwmem_free(buf);
	}
	else {
		if (buf == NULL) {
			res = hwmem_alloc(size);
		}
		else {
			t_hwmem_Buffer *pBuf = findHwmemBufFromVirtAddr(buf);
			t_hwmem_Pool* pool = (t_hwmem_Pool*)(pBuf->pool);
			
			spin_lock(&pool->lock);
			if (size <= pool->size) {
				res = pBuf->pVirtualAddr;
				pBuf->usedSize = size;
				if (++pool->useCount > pool->maxUse) /* For statistic */
					pool->maxUse = pool->useCount;
			}
			spin_unlock(&pool->lock);

			/* Failed to find suitable block (Dump memory status) */			
			if (res == NULL) {
				hwmem_proc_show(NULL, NULL);
			}
		}
	}

	return res;
}

EXPORT_SYMBOL(hwmem_realloc);

/**
 * @brief Release block allocated by hwmem_alloc
 */
void hwmem_free(void *buf)
{
	t_hwmem_Buffer *pBuf = findHwmemBufFromVirtAddr(buf);
	if (pBuf == NULL)
	{
		printk(KERN_WARNING "%p is not a hwmem buffer\n", buf);
		return;
	}

	{
		t_hwmem_Pool* pool = (t_hwmem_Pool*)(pBuf->pool);

		spin_lock(&pool->lock);
		if (pool->useCount > 0)
			pool->useCount--;
		else
			printk(KERN_WARNING "Too many free for %p\n", buf);
		pBuf->inUse = 0;
		pBuf->usedSize = 0;
		pBuf->pFd = NULL;
		pBuf->lock = 0;
		spin_unlock(&pool->lock);
	}
}

EXPORT_SYMBOL(hwmem_free);

int hwmem_mmap(void *buf, struct vm_area_struct * vma, size_t size)
{
	t_hwmem_Buffer *pBuf = findHwmemBufFromVirtAddr(buf);
	if (pBuf == NULL)
	{
		printk(KERN_WARNING "%p is not a hwmem buffer\n", buf);
		return -ENXIO;
	}

	if (size > PAGE_ALIGN((int)pBuf->usedSize))
	{
		printk(KERN_WARNING "try to map a memory area bigger than given hwmem buffer\n");
		return -EINVAL;
	}
	return dma_mmap_coherent(NULL, vma, buf, pBuf->physAddr, size);
}

EXPORT_SYMBOL(hwmem_mmap);

/**
 * @brief Return the physical address of the given hwmem buffer or 0 if error
 */
u32 hwmem_pa(void *buf)
{
	t_hwmem_Buffer *pBuf = findHwmemBufFromVirtAddr(buf);
	if (pBuf == NULL)
	{
		printk(KERN_WARNING "%p is not a hwmem buffer\n", buf);
		return 0;
	}
	return pBuf->physAddr;
}
EXPORT_SYMBOL(hwmem_pa);

/** Device driver functions ------------------------------------------------ */

static int hwmem_dev_open (struct inode *pInode, struct file *pFile)
{
	/* Init state : No buffer hold by handle */
	pFile->private_data=NULL ;
	return 0;
}

static int hwmem_dev_release (struct inode *pInode, struct file *pFile)
{
	u16 i;

	/* check for not released buffer ( segfault user or just forgot to call release buffer  ..) */
	if(pFile->private_data != NULL )
	{
		printk(KERN_WARNING "Hwmem : found %u orphan buffers [owner :%d]\n",(u32)pFile->private_data,current->tgid);
		for ( i = 0; i < HWMEM_MAX_USER_HANDLE ; i++)
		{
			if ((gHwmem_ReqUserBuffer[i] != NULL) && (gHwmem_ReqUserBuffer[i]->pFd == pFile))
			{
				hwmem_free(gHwmem_ReqUserBuffer[i]->pVirtualAddr);
				gHwmem_ReqUserBuffer[i] = NULL;
			}
		}
	}
	pFile->private_data = NULL ;
	return 0;
}

static int hwmem_dev_ioctl(struct inode *pInode, struct file *pFile, unsigned int cmd, unsigned long arg)
{
	u16 baseIndex;
	t_hwmem_UserBuffer userBuf;
	t_hwmem_Buffer *pHwmemBuf;
	void *buf;

	if (copy_from_user(&userBuf,(void*)arg,sizeof(t_hwmem_UserBuffer)))
		return -EFAULT;

	switch (cmd)
	{
	case  HWMEM_REQUEST_BUF :
		/* find free handle */
		baseIndex = gHwmem_ReqBufferIndex;
		while (gHwmem_ReqUserBuffer[gHwmem_ReqBufferIndex]!=NULL)
		{
			gHwmem_ReqBufferIndex++;
			if (gHwmem_ReqBufferIndex >= HWMEM_MAX_USER_HANDLE)
				gHwmem_ReqBufferIndex-= (HWMEM_MAX_USER_HANDLE-1);
			if (gHwmem_ReqBufferIndex==baseIndex)
			{
				printk(KERN_WARNING "hwmem: no more free user handle\n");
				return -ENOMEM;
			}
		}
		/* record hwmem struct pointer */
		buf = hwmem_alloc(userBuf.size);
		if(buf != NULL)
		{
			pHwmemBuf = findHwmemBufFromVirtAddr(buf);
			gHwmem_ReqUserBuffer[gHwmem_ReqBufferIndex] = pHwmemBuf;
			userBuf.pUserHandle = (void*)gHwmem_ReqBufferIndex;
			userBuf.boff = pHwmemBuf->boff;
			pHwmemBuf->pFd = pFile;
			pFile->private_data++;
			dbg("HWMEM_REQUEST_BUF : size %u handle %u Buffer @%p ( Phys %x) \n"
					,userBuf.size,
					(u32)userBuf.pUserHandle,
					pHwmemBuf->pVirtualAddr,
					pHwmemBuf->physAddr);
		}
		else
		{
			printk(KERN_WARNING "hwmem: no more free preallocated buf\n");
			return -ENOMEM;
		}
		break;

	case  HWMEM_RELEASE_BUF:
		if((pFile->private_data != NULL)
			&& (gHwmem_ReqUserBuffer[(u32)userBuf.pUserHandle] != NULL))
		{
			hwmem_free(gHwmem_ReqUserBuffer[(u32)userBuf.pUserHandle]->pVirtualAddr);
			gHwmem_ReqUserBuffer[(u32)userBuf.pUserHandle] = NULL ;  /* free again */
			userBuf.pUserHandle = NULL;
			pFile->private_data--;
		}
		break;

	case  HWMEM_QUERY_BUF:  /* Return Size and offset of this userptr */
		{
			userBuf.size = gHwmem_ReqUserBuffer[(u32)userBuf.pUserHandle]->usedSize;
			userBuf.boff = gHwmem_ReqUserBuffer[(u32)userBuf.pUserHandle]->boff;
		}
		break;

	default:
		return -ENOIOCTLCMD; /* unrecognized ioctl */
	}

	if (copy_to_user((void*)arg,&userBuf,sizeof(t_hwmem_UserBuffer)) != 0)
		return -ENOMEM;
	else
		return 0;
}

static int hwmem_dev_mmap(struct file *pFile, struct vm_area_struct * vma)
{
	if (pFile->private_data != 0)
	{
		int i;
		size_t boff = (vma->vm_pgoff << PAGE_SHIFT);
		for (i = 0 ; i < HWMEM_MAX_USER_HANDLE ; i++)
		{
			if (gHwmem_ReqUserBuffer[i] != NULL
				&& gHwmem_ReqUserBuffer[i]->pFd == pFile
				&& gHwmem_ReqUserBuffer[i]->boff == boff)
			{
				/* mmap always at the buffer head */
				vma->vm_pgoff = 0;
				return hwmem_mmap(
					gHwmem_ReqUserBuffer[i]->pVirtualAddr,
					vma, vma->vm_end - vma->vm_start);
			}
		}
	}
	return -EINVAL ;
}

/* File operations for hwmem , user space interface */
struct file_operations s_hwmem_Fops = {
	.open    = hwmem_dev_open,
	.release = hwmem_dev_release,
	.mmap    = hwmem_dev_mmap,
	.ioctl   = hwmem_dev_ioctl
};


/** proc functions --------------------------------------------------------- */
static int hwmem_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 alloc, memSize, memAlloc;
	struct list_head *elt;
	t_hwmem_Pool *pool;	

#ifdef CONFIG_HWMEM_QVGA
	if (m)
	seq_printf(m, "QVGA Configuration:\n");
	else
		printk("QVGA Configuration:\n");

#elif CONFIG_HWMEM_WQVGA
	if (m)
	seq_printf(m, "WQVGA Configuration:\n");
	else
		printk("WQVGA Configuration:\n");

#else
	if (m)
	seq_printf(m, "UNKNOWN Configuration:\n");
	else
		printk("UNKNOWN Configuration:\n");
#endif

	list_for_each(elt, &sPoolsList) {
		pool = list_entry(elt, t_hwmem_Pool, list);

		/* Pool info (max size, in use size, ...) */
		for (i=0, alloc=0, memSize=0, memAlloc=0 ; i < pool->count ; i++) {
			alloc++;
			memAlloc += pool->size;
			if (pool->pBuffers[i].inUse) {
				memSize += pool->pBuffers[i].usedSize;
			}
		}

		/* Print pool infomation */
		if (m) {
		seq_printf(m," Pool %4dk(=%7d): Buffers=%2d/%3d/%3d Mem=%7d/%7d/%7d"
				" bytes (InUse/Max/Total)\n", 
				pool->size>>10, pool->size, pool->useCount, pool->maxUse,
				pool->count, memSize, pool->size*pool->maxUse,
				pool->size*pool->count);
	}
		else {
			printk(" Pool %4dk(=%7d): Buffers=%2d/%3d/%3d Mem=%7d/%7d/%7d"
					" bytes (InUse/Max/Total)\n",
					pool->size>>10, pool->size,pool->useCount, pool->maxUse,
					pool->count, memSize, pool->size*pool->maxUse,
					pool->size*pool->count);
		}
	}

	return 0;
}

static int hwmem_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &hwmem_proc_show, NULL);
}

static struct file_operations hwmem_ProcOperations = {
	.open     = hwmem_seq_open,
	.read     = seq_read,
	.llseek   = seq_lseek,
	.release  = seq_release
};

/* /proc support */
static struct proc_dir_entry *proc_dir;

/** Init/exit module functions --------------------------------------------- */

/**
 * @brief Init the pool allocator.
 * preallocate all needed contiguous buffers.
 * @return 0 if successful else -ENOMEM if it cannot allocate structures
 */
static int __init hwmem_init(void)
{
	int n, size, nb, ret;
	char *s, *p;
	struct proc_dir_entry *pEntry;

	printk("Hardware memory buffers allocator init\n");

	/* create /proc/hwmem */
	if (! (pEntry = create_proc_entry(MODULE_NAME, 0, NULL)))
	{
		printk(KERN_ERR "Could not create /proc/hwmem\n");
	}
	else
	{
		pEntry->proc_fops = &hwmem_ProcOperations;
	}

	/* Allocate pools & buffers vs module variable pools value */
	for(p = &pools[0]; ;)
	{
		s = strchr(p, ',');
		n = sscanf(p, "%dx%d", &size, &nb);
		p = s+1;
		if (n!=2)
		{
			printk(KERN_ERR "Bad pools list format: %s\n", pools);
			BUG();
		}
		ret = allocPoolAndBuf(size, nb);
		if (ret != 0) return ret;
		if (s == NULL) break;
	}

	/* Register device  */
	if (register_chrdev(HWMEM_MAJOR, MODULE_NAME, &s_hwmem_Fops))
	{
		printk(KERN_WARNING "unable to get major %d for hwmem driver\n",
				HWMEM_MAJOR);
		return -ENODEV;
	}

	return 0;
}

/**
 * @brief Shutdown pool allocator.
 * Free all resources.
 */
static void __exit hwmem_exit(void)
{
	int i;
	t_hwmem_Pool *pool;
	struct list_head *elt, *tmp;

	/* release remaining buffer */
	for ( i = 0; i < HWMEM_MAX_USER_HANDLE ; i++)
	{
		if (gHwmem_ReqUserBuffer[i] != NULL)
		{
			hwmem_free(gHwmem_ReqUserBuffer[i]->pVirtualAddr);
			gHwmem_ReqUserBuffer[i] = NULL;
		}
	}
	/* Unregister device */
	unregister_chrdev(HWMEM_MAJOR, MODULE_NAME);

	/* Free buffers */
	list_for_each_safe (elt, tmp, &sPoolsList)
	{
		pool = list_entry(elt, t_hwmem_Pool, list);
		for (i=0 ; i<pool->count ; i++)
		{
			dma_free_coherent(NULL, pool->size,
					pool->pBuffers[i].pVirtualAddr,
							(dma_addr_t)pool->pBuffers[i].physAddr);
		}
		list_del(elt);
		kfree(pool->pBuffers);
		kfree(pool);
	}

	/* remove file /proc/hwmem */
	if (proc_dir) 
	{
		remove_proc_entry(MODULE_NAME, NULL);
	}

	dbg("hwmem terminates OK\n");
}

module_init(hwmem_init);
module_exit(hwmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Purple Labs SA / ST-Ericsson");
MODULE_DESCRIPTION("contiguous physical memory buffers pool allocator");

