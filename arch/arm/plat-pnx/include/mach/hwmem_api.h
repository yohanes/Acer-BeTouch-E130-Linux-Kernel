/**
 * @file hwmem_api.h
 * @brief Contiguous memory buffers pool allocator
 * @author (C) Purple Labs SA 2006 & ST-Ericsson 2010
 * @date 2008-04-10
 * @par Contributor:
 * Philippe Langlais, Purple Labs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _hwmem_api_h_
#define _hwmem_api_h_

/**
 * @brief device IOCTLs for userptr
 */
enum HWMEM_IOCTL
{
	HWMEM_REQUEST_BUF = 1, /* Request a userptr buffer */
	HWMEM_RELEASE_BUF = 2, /* Release a userptr buffer */
	HWMEM_QUERY_BUF = 3    /* Query a userptr buffer */
};

/**
 * @brief ioctl user parameter for userptr
 */
typedef struct
{
	void *pUserHandle; /* userptr handle */
	size_t size;       /* buffer size */
	size_t boff;       /* buffer offset for mmap */
} t_hwmem_UserBuffer;

#ifdef __KERNEL__

#undef HWMEM_DEBUG /* define it for versbose debug */

/* magic number to identify hwmem buffer structure */
#define HWMEM_BUFF_MAGIC 0xABCD4321

/* maximum number of user handles , */
/*NOTE :  this value must be keeped above the number of preallocated buffers */
#define HWMEM_MAX_USER_HANDLE 32

/* hwmem buffer handler type */
typedef struct
{
	u32 magic;
	struct file *pFd;
	void *pVirtualAddr;
	u32 physAddr;
	u32 usedSize : 26;
	u32 lock : 5;
	u32 inUse : 1;
	size_t boff;
	void *pool;
} t_hwmem_Buffer;

extern t_hwmem_Buffer *gHwmem_ReqUserBuffer[];

/**
 * @brief Allocate a memory block from an existing pool
 * @param size size of the memory block required
 * @return the virtual kernel address of the buffer
 *         or NULL if there is not enough remaining buffer in memory pools
 */
void* hwmem_alloc(u32 size);

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
void* hwmem_realloc(void *buf, u32 size);

/**
 * @brief Release a memory block allocated by hwmem_alloc
 * @param buf hwmem buffer to free
 */
void hwmem_free(void *buf);

/**
 * @brief Return the physical address of the given hwmem buffer
 * @param buf target hwmem buffer
 * @return the buffer's physical address or 0 if error
 */
u32 hwmem_pa(void *buf);

/**
 * @brief Map a hwmem buffer previously allocated by hwmem_alloc
 * into user space.  The hwmem buffer must not be freed by the
 * driver until the user space mapping has been released.
 * @param buf hwmem buffer to map
 * @param vma vm_area_struct describing requested user mapping
 * @param size size of memory originally requested in hwmem_alloc
 * @return 0 on success
 */
int hwmem_mmap(void *buf, struct vm_area_struct * vma, size_t size);

/** USERPTR relative functions -------------------------------------------- */

/**
 * hwmem_isUserPtrValid:
 * check if a user pointer leads to a hwmem buffer
 */
static inline int hwmem_isUserPtrValid(void *handle)
{
	return ( ((u32)(handle) < HWMEM_MAX_USER_HANDLE) &&
      (gHwmem_ReqUserBuffer[(u32)(handle)] != NULL) &&
      (gHwmem_ReqUserBuffer[(u32)(handle)]->magic == HWMEM_BUFF_MAGIC) );
}

/**
 * hwmem_getBufFromUserPtr:
 * returns hwmem virtual kernel address buffer relative to a user pointer
 */
static inline void* hwmem_getBufFromUserPtr(void *handle)
{
	return gHwmem_ReqUserBuffer[(u32)handle]->pVirtualAddr;
}

/**
 *  hwmem_getPhysAddrFromUserPtr:
 *  returns physical address of the buffer linked to the user pointer
 */
static inline u32 hwmem_getPhysAddrFromUserPtr(void *handle)
{
	return gHwmem_ReqUserBuffer[(u32)handle]->physAddr;
}

/**
 *  Lock , unlock and check lock of the hwmem buffer linked to the user pointer
 */
static inline void hwmem_lockBufFromUserPtr(void *handle)
{
	gHwmem_ReqUserBuffer[(u32)(handle)]->lock ++;
}

static inline void hwmem_unlockBufFromUserPtr(void *handle)
{
	if(gHwmem_ReqUserBuffer[(u32)(handle)]->lock > 0)
		gHwmem_ReqUserBuffer[(u32)(handle)]->lock--;
	else
		gHwmem_ReqUserBuffer[(u32)(handle)]->lock = 0;
}

static inline int hwmem_isBufLockedFromUserPtr(void *handle)
{
	return (gHwmem_ReqUserBuffer[(u32)(handle)]->lock > 0);
}

static inline int hwmem_getLockNumberFromUserPtr(void *handle)
{
	return (gHwmem_ReqUserBuffer[(u32)(handle)]->lock);
}

#endif /*kernel include */

#endif /* _hwmem_api_h_ */

