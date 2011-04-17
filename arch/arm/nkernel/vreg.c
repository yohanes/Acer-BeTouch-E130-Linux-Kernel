/*
 * ============================================================================
 *
 *       Filename:  vreg.c
 *
 *    Description:  This file implements function dedicated to ip hardware
 *                  sharing between 2 drivers.
 *                  one driver has : the access control, the second is consider as user.
 *                  the second user provides access through IOCTL function.
 *                  over load functions for :
 *                  register description are provided
 *                  register write 
 *                  register read
 *                  interruption handling 
 *
 *        Version:  1.0
 *        Created:  09/23/2008 09:23:51 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  M JAOUEN
 *        Company:  ST NXP Le Mans 
 *
 * ============================================================================
 */
#ifdef TSTU
#include "vreg.h"
#include <stdlib.h>
#define ALLOC(A) calloc(A,1)
#define PERROR(format, arg...) printf( format , ## arg)
int g_err=0;
#define TEST_ERROR(A) {printf(A); g_err++;}
#define FREE(A) free(A)
#else
#include <nk/vreg.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#define ALLOC(A) kzalloc(A,GFP_KERNEL)
#ifdef DEBUG
#define PERROR(format, arg...) printk(KERN_ERR format , ## arg)
#else
#define PERROR(format, arg...) 
#endif
#define FREE(A) kfree(A)
#endif

typedef struct t_vreg {
	unsigned char *name;
	unsigned long address;
	unsigned long size;
	/* VR_UNDEFINED :*/ 
	/* VR_READ : hardware reg read allowed */
	/* VR_WRITE :hardware write allowed */
	/* VR_CLEAR_READ : after the user has read the register, the copy is cleared */
    /*                 This category of register is physically read by instance driver (number 0) */
    /*                 All the other instance uses only copy content */	
	/* VR_MASK_READ  : value of mask must be used to provide read value */
	/*                 for register that are not EXCLU and not CLEAR_READ and not VR_MASK_READ */
	/*                 physical read is perform if read allowed*/
	/* VR_POLARITY   : the register is shared , certain bits in the register have to remain to 0 even if they are set to 1 by 
	 *                 one of the 2 users , if no mask exists exits all bit set in polarity are really beahving as above,
	 *                 if mask exist only the bits which are set in mask and in polarity are behaving as above.
	 *                 */
	/* VR_EXCLU : only one user can write, all read are physical access attribute CLEAR READ/MASK_READ is forbidden   */
	
	unsigned char *type[2];
	/* give number in bytes of contiguous attribute */
	/* the target is to support , read/write of variable length */
	unsigned long *contiguous[2];
	unsigned char *data[2];
    /* tempory item for the aera which are shared */
	unsigned char *tmp;
	/* mask is usefull for share area only */
	unsigned char *mask[2];
	/* polarity is isefull for share area only */
	unsigned char *polarity;
	/* all register with CLEAR READ are in this category */
	/* while driver read register it computes pending counter */
	/* this counter is cleared */
	void *ctx;
	t_nwrite nwrite;
	t_nread nread;
#ifndef TSTU
   /* we required a lock on the access to data in write */
   /* especially for the data that are CLEAR_READ */
   /* and for the shared data  while the value is read, for building and writing we must ensure , that data copy is unchanged */
   	struct mutex lock;
#endif 
}t_vreg;



/** 
 * @brief Allocate a descriptor for a bank of share register 
 * 
 * @param ctx context for read / write call function 
 * @param name string containing the name of the register
 * @param address start address of register bank 
 * @param nb size of the bank in bytes
 * @param nwrite pointer to a function performing physical write to register bank
 * @param nread pointer to a function performing physical read to register bank
 * 
 * @return pointer to bank register descriptor
 */
extern void *vreg_alloc(void *ctx, char *name,unsigned long address, int nb,  t_nwrite nwrite, t_nread nread)
{
	int i;
	t_vreg *ret=ALLOC( sizeof(t_vreg));
	if (ret==0) goto end;
	ret->name=ALLOC(strlen(name));
	if (ret->name==0) 
	{
		ret=0;
		goto end;
	}
	strcpy(ret->name,name);
	ret->size=nb;
	ret->address=address;
	ret->nwrite=nwrite;
	ret->nread=nread;
	ret->ctx=ctx;
	for(i=0;i<2;i++)
	{
		ret->type[i]=ALLOC(sizeof(unsigned char)*nb);
		ret->data[i]=ALLOC(sizeof(unsigned char)*nb);
		ret->contiguous[i]=ALLOC(sizeof(unsigned long)*nb);
		ret->mask[i]=ALLOC(sizeof(unsigned char*)*nb);
		if (!(ret->type[i])||(!(ret->data[i]))||(!ret->mask[i])||(!ret->contiguous[i]))
		{
			ret=0;
			goto end;
		}
	}
	ret->polarity=ALLOC(sizeof(unsigned char*)*nb);
	ret->tmp=ALLOC(sizeof(unsigned char*)*nb);
#ifndef TSTU
	mutex_init(&ret->lock);
#endif
	if (!ret->tmp) ret=0;
end:
	if (ret==0)
	{
#ifdef DEBUG
		PERROR("VREG %s: alloc pb\n",name);
#endif
	}
	return (void*)ret;

}

/** 
 * @brief release the descriptor of a shared register bank  
 * 
 * @param desc descriptor of bank register
 * 
 * @return none
 * */
void vreg_free(void *desc)
{
	t_vreg *preg=(t_vreg*) desc;
	int i;
	for(i=0;i<2;i++)
	{	
		FREE(preg->type[i]);
		FREE(preg->data[i]);
		FREE(preg->contiguous[i]);
		FREE(preg->mask[i]);
	}
	FREE(preg->name);
    FREE(preg->polarity);
    FREE(preg->tmp);	
	FREE(preg);
}


/** 
 * @brief fix the attribute of a contiguous register set in a shared register bank
 * 
 * @param u 0 attribute given to driver user 
 *          1 attribute given to user driver 
 * @param desc pointer to descriptor of share bank register 
 * @param offset start address of register  set in the register bank
 * @param size number of bytes   
 * @param type attribute of this register set
 * @param mask pointer to a table giving the mask for each register
 * @param polarity pointer to a table giving the polarity for each register
 * 
 * @return 0 sucessfull else error 
 */
static int vreg_desc(int u,t_vreg *desc,unsigned long offset,
	        	unsigned long size, 
				unsigned char type, 
				unsigned char *mask, 
				unsigned char *polarity)
{
	int ret=0;
	int o=0;
	int i;
	/* test register is in correct area */
	int index = offset;
	/* control u consistency */
	if (u==0) o=1;
	else
		if (u==1) o=0;
		else
		{
			ret=1;
			goto end;
		}
	/* control access config within range */
	if ((offset+size) > (desc->size))
	{
		PERROR("VREG %s: error def out of range %x %x %x %x\n",desc->name,
				(unsigned int)u,
				(unsigned int)index,
				(unsigned int)size,
				(unsigned int)type);
		ret=2;
		goto end;
	}
	/* control type is consistent */
	if ((!(type & VR_READ)) &&((type & VR_MASK) || (type & (VR_CLEAR | VR_RESET))))
	{
		/* access in read forbid but behaviour in read defined */
		PERROR("VREG %s: error read def %x %x %x %x\n",desc->name,
				(unsigned int)u,
				(unsigned int)index,
				(unsigned int)size,
				(unsigned int)type);
		ret=3;
		goto end;	   
	}
	if ((type & VR_EXCLU))
	{
#if 0 /* this constaints is removed some hardware register can be read only */
		if ((!(type & VR_WRITE)))
		{ /* access is exclusif and write access is not given */
			PERROR("VREG %s: error read def %x %x %x %x\n",desc->name,u,index,size,type);
			ret=4;
			goto end;
		}
#endif 
		if ((type & VR_MASK)||(type & (VR_CLEAR| VR_RESET))||(type & VR_POLARITY))
		{
			/* attribute relative to read share function not avalaible for register exclu */
			PERROR("VREG %s: error read def %x %x %x %x\n",desc->name,
					(unsigned int)u,
					(unsigned int)index,
					(unsigned int)size,
					(unsigned int)type);
			ret=5;
			goto end;	   
		}	
	}
	else
	{
		/* check that RESET and CLEAR are not set */
        if ((type & (VR_CLEAR | VR_RESET)) == (VR_RESET | VR_CLEAR))
		{
         /* attribute relative to read share function not avalaible for register exclu */
			PERROR("VREG %s: error read def %x %x %x %x\n",desc->name,
					(unsigned int)u,
					(unsigned int)index,
					(unsigned int)size,
					(unsigned int)type);
			ret=6;
			goto end;	   
		}

	}
	for (i=0;i<size;i++)
	{ desc->type[u][index+i]=type;	
		desc->contiguous[u][index+i]=size-i;
		if (type&VR_MASK) desc->mask[u][index+i]=mask[i];
		/* check compatibility with the device */
		if (type & VR_CLEAR) desc->data[u][index+i]= 0x0;
		if (type & VR_RESET) desc->data[u][index+i]= 0x0ff;
		if (type&VR_POLARITY) 
		{
			unsigned char vmask=0xff;
			if (type&VR_MASK) vmask=mask[i];
			/* as for polarity the value which must be set is zero if one zero is present 
			 * initialization at 1 is done */
			desc->data[u][index+i]= polarity[i]&vmask;
			desc->polarity[index+i]=desc->polarity[index+i] | (polarity[i]&vmask);
		}
		if (type & VR_EXCLU )  
		{
			if (desc->type[o][index+i]&VR_WRITE)
			{
				PERROR("VREG %s: error write def %x %x %x %x\n",desc->name,
						(unsigned int)u,
						(unsigned int)index,
						(unsigned int)size,
						(unsigned int)type);
				ret=6;
				goto end;
			}
		}
	}
end:
	return ret;
}

/** 
 * @brief fix the driver attribute of a contiguous register set in a shared register bank 
 * 
 * @param desc pointer to descriptor of share bank register 
 * @param offset start address of register  set in the register bank
 * @param size number of bytes   
 * @param type attribute of this register set
 * @param mask pointer to a table giving the mask for each register
 * @param polarity pointer to a table giving the polarity for each register
 * 
 * @return 0 sucessfull else error
 */
int vreg_driver_desc(void *desc,
					unsigned long offset, 
					unsigned long size, 
					unsigned char type, 
					unsigned char *mask, 
					unsigned char *polarity)
{
return vreg_desc(0,(t_vreg*)desc,offset,size,type,mask,polarity);
}


/** 
 * @brief fix the user attribute of a contiguous register set in a shared register bank 
 * 
 * @param desc pointer to descriptor of shared bank register 
 * @param offset start address of register  set in the register bank
 * @param size number of bytes   
 * @param type attribute of this register set
 * @param mask pointer to a table giving the mask for each register
 * @param polarity pointer to a table giving the polarity for each register
 * 
 * @return 0 sucessfull else error 
 */
 

int vreg_user_desc(void *desc,unsigned long offset, unsigned long size, unsigned char type,unsigned char *mask, unsigned char *polarity)
{
return vreg_desc(1,(t_vreg*)desc,offset,size,type,mask,polarity);
}


/** 
 * @brief write registers in user or driver attribute in a shared register bank 
 * 
 * @param u 0 driver access  
 *          1 user access
 * @param desc pointer to descriptor of shared bank register
 * @param reg physical address of 1st register 
 * @param data pointer to data to be written 
 * @param size number of bytes 
 * 
 * @return number of bytes written
 */
static int vreg_write(int u,t_vreg *desc, unsigned long reg, void *data, int size)
{
	int ret=0;
	int index=0;
	int o,i;
	/* control u consistency */
	if (u==0) o=1;
	else
		if (u==1) o=0;
		else
		{
			PERROR("VREG %s: instance error %x %x %x\n",desc->name,u,index,size);	
			goto end;
		}
	/* check access range */
	if ((reg<desc->address) || ((reg+size) > (desc->address+desc->size))|| (!(desc->type[u][reg-desc->address]& VR_WRITE)))
	{
		PERROR("VREG %s: write range problem %x %x %x\n",desc->name,u,reg,size);
		goto end;
	}
	index=reg-desc->address;	
	/* check access range is possible  */
	if ((desc->contiguous[u][index]>=size))
	{
		/* determine the value to be written in register */
		if (desc->type[u][index] & VR_EXCLU)
		{
			/* store value in memory copy , it is only for debug purpose in this case */
			/* memcpy(&desc->data[u][index],data,size);*/
			ret=desc->nwrite(desc->ctx,reg,data,size);
		}
		else
		{
			
#ifndef TSTU
			mutex_lock(&desc->lock);
#endif
			/* store value in memory copy */
			memcpy(&desc->data[u][index],data,size);
			/* compute the value to be written according to other space */
			if (desc->type[u][index]& VR_MASK)
			{
				/* we attempt to detect a error access */
				/* user or driver sets a bit outside of the area */
				/* we apply the mask to the value */
				for(i=0;i<size;i++)
				{
                desc->data[u][index+i]=desc->data[u][index+i]&desc->mask[u][index+i];
				}

			}
			if (desc->type[u][index]& VR_POLARITY)
			{
				for (i=0;i<size;i++)
				{
					int j;
					unsigned char mask[2]={0,0};
					for (j=0;j<2;j++)
					{
						mask[j]=  (~(desc->data[j][index+i])& (desc->polarity[index+i]));
						/*in mask value only the area mask are kept */
						if (desc->type[j][index+i]& VR_MASK) mask[j]= mask[j] &desc->mask[u][index+i];
					}
					mask[0]=mask[0] | mask[1];
					desc->tmp[index+i]=(desc->data[u][index+i]|desc->data[o][index+i])&~mask[0];

				}
			}
			else
			{
				for (i=0;i<size;i++)
				{
					/* fixe me or can be optimize in u32 */
					desc->tmp[index+i]=desc->data[u][index+i]|desc->data[o][index+i];
				}
			}
			ret=desc->nwrite(desc->ctx,reg,&(desc->tmp[index]),size);
#ifndef TSTU
			mutex_unlock(&desc->lock);
#endif
		}
	}
	else
	/* this loop split the access in contiguous attribute */
	{int len;
		i=0;
		do 
		{	len=((i+desc->contiguous[u][index+i])>size)?(size-i): (desc->contiguous[u][index+i]);
			if (len==0) { ret=0; goto end;}
			ret=vreg_write(u,desc,reg+i,data,len);
			if (ret!=len) goto end;
			data=data+len;
			i=i+len;
		}
		while(i<size);
		ret=size;
	}
end:
	return ret;
}


/** 
 * @brief write registers with driver attributes in a shared register bank 
 * 
 * @param desc pointer to descriptor of shared bank register 
 * @param reg  physical address of 1st register  
 * @param data pointer to data to be written 
 * @param size  number of bytes 
 * 
 * @return number of bytes written
 */
int vreg_driver_write(void *desc, unsigned long reg, void *data, int size)
{
return (vreg_write(0,(t_vreg*)desc,reg,data,size));
}


/** 
 * @brief read registers in user or driver attribute in a shared register bank 
 * 
 * @param u 0 driver access  
 *          1 user access
 * @param desc pointer to descriptor of shared bank register
 * @param reg  physical address of 1st register 
 * @param data pointer to data to be read
 * @param size number of bytes 
 * 
 * @return number of bytes written
 */
static int vreg_read(int u,t_vreg *desc, unsigned long reg, unsigned char *data, int size)
{
	int ret=0;
	int index=0;
	int i,o;
	/* control u consistency */
	if (u==0) o=1;
	else
		if (u==1) o=0;
		else
		{
			PERROR("VREG %s: instance error %x %x %x\n",desc->name,u,index,size);	
			goto end;
		}
	/* check access range */
	if 	((reg<desc->address) || ((reg+size) > (desc->address+desc->size))|| (!(desc->type[u][reg-desc->address]& VR_READ)))
	{
		PERROR("VREG %s: read  range | right problem %x %x %x\n",desc->name,u,reg,size);
		goto end;
	}
	index=reg-desc->address;
	/* check access range is possible  */
	if (desc->contiguous[u][index]>=size)
	{
		/* determine the value to be read in register */
		if (desc->type[u][index] & VR_EXCLU)
		{
			ret=desc->nread(desc->ctx,reg,data,size);
		}
		else
		{
			if ((desc->type[u][index] & (VR_CLEAR | VR_RESET))||(desc->type[o][index] & (VR_CLEAR | VR_RESET)))
			{
			    /* polarity is ignored */
				/* we have got to take the lock */
#ifndef TSTU
				mutex_lock(&desc->lock);
#endif
				if (u==0) 
				{
					/* we store the result in data */
					ret=desc->nread(desc->ctx,reg,data,size);
					/* we compute 1st the status available for the user only */
					if (desc->type[1][index] & VR_MASK)
					{
						/* we compute 1st the status available for the user only */
						for(i=0;i<size;i++)
						{
							desc->data[1][index+i]=(desc->data[1][index+i]) | (data[i]&(desc->mask[1][index+i]));
						}	
					}
					else 
					{
						for(i=0;i<size;i++)
						{
							/* according to clear or reset */
                            if (desc->type[1][index] & (VR_CLEAR))
							/* we continue to set the bits */
							desc->data[1][index+i]=desc->data[1][index+i]|(data[i]);
							else
							/* we continue to keep the bit cleared */
	                        desc->data[1][index+i]=desc->data[1][index+i]&(data[i]);

						}
					}
					/* we compute the return value to the driver */
					if (desc->type[0][index] & VR_MASK)
					{
						/* we compute 1st the status available for the user only */
						for(i=0;i<size;i++)
						data[i]=data[i]& desc->mask[0][index+i];
					}
				}
				else 
				{
					/* a memcpy is done */
					memcpy(data,&desc->data[u][index],size);
					/* a clear or a reset is done */
                    if (desc->type[u][index] & (VR_CLEAR))
					memset(&desc->data[u][index],0,size);
					else
                	memset(&desc->data[u][index],0xff,size);
					ret=size;
				}
#ifndef TSTU
				mutex_unlock(&desc->lock);
#endif
			}
			else
			{
				ret=desc->nread(desc->ctx,reg,data,size);
				if (desc->type[u][index]&VR_MASK)
				{
					/* we read the register anyway */
						for(i=0;i<size;i++)
						data[i]=data[i]& desc->mask[u][index+i];
				}
				if (desc->type[u][index]&VR_POLARITY)
				{ /* for polarity */
				  /* we take the value present in memory */
			            for(i=0;i<size;i++)
						data[i]=data[i] |(desc->polarity[index+i]& desc->data[u][index+i]);
				}
			}
		}
	}
	else
	/* this loop split the access in contiguous attribute */
	{
		int len;
		i=0;
		do 
		{	len=((i+desc->contiguous[u][index+i])>size)?(size-i): (desc->contiguous[u][index+i]);
			if (len==0) { ret=0; goto end;}
			ret=vreg_read(u,desc,reg+i,data,len);
			if (ret!=len) goto end;
			data=data+len;
			i=i+len;
		}
		while(i<size);
		ret=size;
	}
end:
	return ret;
}
/** 
 * @brief read registers with driver attributes in a shared register bank 
 * 
 * @param desc pointer to descriptor of shared bank register 
 * @param reg  physical address of 1st register  
 * @param data pointer to data to be written 
 * @param size number of bytes 
 * 
 * @return number of bytes read
 **/
int vreg_driver_read(void *desc,unsigned long reg, void *data, int size)
{
return (vreg_read(0,(t_vreg*)desc,reg,data,size));
}


/** 
 * @brief write registers with user attributes in a shared register bank 
 * 
 * @param desc pointer to descriptor of shared bank register 
 * @param reg  physical address of 1st register  
 * @param data pointer to data to be written 
 * @param size  number of bytes 
 * 
 * @return number of bytes written
 */
int vreg_user_write(void *desc, unsigned long reg, void *data, int size)
{
#if defined(DEBUG)
	int ret;
	{ int i;
		printk("w %lx %x \n",reg,size);
		for (i=0;i<size;i++)
		{
			printk("%x ",((char*)data)[i]);
			if (((i%10)==0)&&(i!=0)) printk("\n");
		}
		printk("\n");
	}
	ret = vreg_write(1,(t_vreg*)desc,reg,data,size);
	if (ret!=size)
		printk("bbw %lx %xerror\n",reg,size);
	return ret;
#else
	return (vreg_write(1,(t_vreg*)desc,reg,data,size));
#endif
}

/** 
 * @brief read registers with user attributes in a shared register bank 
 * 
 * @param desc pointer to descriptor of shared bank register 
 * @param reg  physical address of 1st register  
 * @param data pointer to data to be written 
 * @param size number of bytes 
 * 
 * @return number of bytes read
 **/
int vreg_user_read(void *desc, unsigned long reg, void *data, int size)
{
#if defined(DEBUG)
	int ret=vreg_read(1,(t_vreg*)desc,reg,data,size);
	if (ret!=size)
		printk("bbr %lx %x error\n",reg,size);
	return ret;
#else
	return (vreg_read(1,(t_vreg*)desc,reg,data,size));
#endif
}

/** 
 * @brief Check wether some bits are set in a some user register with CLEAR READ attributes
 * 
 * @param desc pointer to descriptor of shared bank register  
 * @param reg physical address of 1st register 
 * @param size 
 * 
 * @return 0 no bits set 
 *         1 some bits are set
 *         -1 error
 */
int vreg_user_pending(void *desc, unsigned long reg, int size)
{
t_vreg *d=(t_vreg *)desc;
int index=0;
int i;
	if 	((reg<d->address) || ((reg+size) > (d->address+d->size))|| (!(d->type[1][reg-d->address]& (VR_READ |VR_CLEAR | VR_RESET))))
	{
		PERROR("VREG %s: pending  range | right problem %x %x %x\n",d->name,1,index,size);
		goto end;
	}
	index=reg-d->address;
    /* check access range is possible  */
	if (d->contiguous[1][index]>=size)
	{

		/* as soon as a register is different from 0  return 1 */
		for(i=0;i<size;i++)
		{
			if (d->type[1][index+i]&VR_CLEAR )
			{ 
				if (d->data[1][index+i]) return 1;
			}
			else
			{ 
				if (d->data[1][index+i]!=0xff) return 1;
			}
		}	
		return 0;
	}
end:
	/* acess problem */
    return -1;
}

#ifdef TSTU
/* define register taken as example for tstu purpose */
enum pfc50626_regs {
	PCF50626_REG_ID		= 0x00,
	PCF50626_REG_INT1	= 0x01,	/* Interrupt Status */
	PCF50626_REG_INT2	= 0x02,	/* Interrupt Status */
	PCF50626_REG_INT3	= 0x03,	/* Interrupt Status */
	PCF50626_REG_INT4	= 0x04,	/* Interrupt Status */
	PCF50626_REG_INT5	= 0x05,	/* Interrupt Status */
	PCF50626_REG_INT6	= 0x06,	/* Interrupt Status */
	/* Reserved */
	/* Reserved */
	PCF50626_REG_INT7	= 0x09,	/* Interrupt Status */
	PCF50626_REG_INT1M	= 0x0a,	/* Interrupt Mask */
	PCF50626_REG_INT2M	= 0x0b,	/* Interrupt Mask */
	PCF50626_REG_INT3M	= 0x0c,	/* Interrupt Mask */
	PCF50626_REG_INT4M	= 0x0d,	/* Interrupt Mask */
	PCF50626_REG_INT5M	= 0x0e,	/* Interrupt Mask */
	PCF50626_REG_INT6M	= 0x0f,	/* Interrupt Mask */
	/* Reserved */
	/* Reserved */
	PCF50626_REG_INT7M	= 0x12,	/* Interrupt Mask */
	PCF50626_REG_ERROR	= 0x13,
	PCF50626_REG_OOC1	= 0x14,
	PCF50626_REG_OOC2	= 0x15,
	PCF50626_REG_OOCPH	= 0x16,
	PCF50626_REG_OOCS	= 0x17,
	PCF50626_REG_BVMC	= 0x18,
	PCF50626_REG_RECC1	= 0x19,
	PCF50626_REG_RECC2	= 0x1a,
	PCF50626_REG_RECS	= 0x1b,
	PCF50626_REG_RTC1	= 0x1c,
	PCF50626_REG_RTC2	= 0x1d,
	PCF50626_REG_RTC3	= 0x1e,
	PCF50626_REG_RTC4	= 0x1f,
	PCF50626_REG_RTC1A	= 0x20,
	PCF50626_REG_RTC2A	= 0x21,
	PCF50626_REG_RTC3A	= 0x22,
	PCF50626_REG_RTC4A	= 0x23,
	PCF50626_REG_CBCC1	= 0x24,
	PCF50626_REG_CBCC2	= 0x25,
	PCF50626_REG_CBCC3	= 0x26,
	PCF50626_REG_CBCC4	= 0x27,
	PCF50626_REG_CBCC5	= 0x28,
	PCF50626_REG_CBCC6	= 0x29,
	PCF50626_REG_CBSC1	= 0x2a,
	PCF50626_REG_CBCS2	= 0x2b,
	PCF50626_REG_BBCC1	= 0x2c,
	PCF50626_REG_PWM1S	= 0x2d,
	PCF50626_REG_PWM1D	= 0x2e,
	PCF50626_REG_PWM2S	= 0x2f,
	PCF50626_REG_PWM2D	= 0x30,
	PCF50626_REG_LED1C	= 0x31,
	PCF50626_REG_LED2C	= 0x32,
	PCF50626_REG_LEDCC	= 0x33,
	PCF50626_REG_ADCC2	= 0x34,
	PCF50626_REG_ADCC3	= 0x35,
	PCF50626_REG_ADCC4	= 0x36,
	PCF50626_REG_ADCC1	= 0x37,
	PCF50626_REG_ADCS1	= 0x38,
	PCF50626_REG_ADCS2	= 0x39,
	PCF50626_REG_ADCS3	= 0x3a,
	PCF50626_REG_TSIC2	= 0x3b,
	PCF50626_REG_TSIC1	= 0x3c,
	PCF50626_REG_TSIDAT1	= 0x3d,
	PCF50626_REG_TSIDAT2	= 0x3e,
	PCF50626_REG_TSIDAT3	= 0x3f,
	PCF50626_REG_GPIO1C1	= 0x40,
	PCF50626_REG_E1REGC2	= 0x41,
	PCF50626_REG_E1REGC3	= 0x42,
	PCF50626_REG_GPIO2C1	= 0x43,
	PCF50626_REG_E2REGC2	= 0x44,
	PCF50626_REG_E2REGC3	= 0x45,
	PCF50626_REG_GPIO3C1	= 0x46,
	PCF50626_REG_E3REGC2	= 0x47,
	PCF50626_REG_E3REGC3	= 0x48,
	PCF50626_REG_GPIO4C1	= 0x49,
	PCF50626_REG_E4REGC2	= 0x4a,
	PCF50626_REG_E4REGC3	= 0x4b,
	PCF50626_REG_GPIO5C1	= 0x4c,
	PCF50626_REG_E5REGC2	= 0x4d,
	PCF50626_REG_E5REGC3	= 0x4e,
	PCF50626_REG_GPIO6C1	= 0x4f,
	PCF50626_REG_E6REGC2	= 0x50,
	PCF50626_REG_E6REGC3	= 0x51,
	PCF50626_REG_GPO1C1	= 0x52,
	PCF50626_REG_EO1REGC2	= 0x53,
	PCF50626_REG_EO1REGC3	= 0x54,
	PCF50626_REG_GPO2C1	= 0x55,
	PCF50626_REG_EO2REGC2	= 0x56,
	PCF50626_REG_EO2REGC3	= 0x57,
	PCF50626_REG_GPO3C1	= 0x58,
	PCF50626_REG_EO3REGC2	= 0x59,
	PCF50626_REG_EO3REGC3	= 0x5a,
	PCF50626_REG_GPO4C1	= 0x5b,
	PCF50626_REG_EO4REGC2	= 0x5c,
	PCF50626_REG_EO4REGC3	= 0x5d,
	PCF50626_REG_D1REGC1	= 0x5e,
	PCF50626_REG_D1REGC2	= 0x5f,
	PCF50626_REG_D1REGC3	= 0x60,
	PCF50626_REG_D2REGC1	= 0x61,
	PCF50626_REG_D2REGC2	= 0x62,
	PCF50626_REG_D2REGC3	= 0x63,
	PCF50626_REG_D3REGC1	= 0x64,
	PCF50626_REG_D3REGC2	= 0x65,
	PCF50626_REG_D3REGC3	= 0x66,
	PCF50626_REG_D4REGC1	= 0x67,
	PCF50626_REG_D4REGC2	= 0x68,
	PCF50626_REG_D4REGC3	= 0x69,
	PCF50626_REG_D5REGC1	= 0x6a,
	PCF50626_REG_D5REGC2	= 0x6b,
	PCF50626_REG_D5REGC3	= 0x6c,
	PCF50626_REG_D6REGC1	= 0x6d,
	PCF50626_REG_D6REGC2	= 0x6e,
	PCF50626_REG_D6REGC3	= 0x6f,
	PCF50626_REG_D7REGC1	= 0x70,
	PCF50626_REG_D7REGC2	= 0x71,
	PCF50626_REG_D7REGC3	= 0x72,
	PCF50626_REG_D8REGC1	= 0x73,
	PCF50626_REG_D8REGC2	= 0x74,
	PCF50626_REG_D8REGC3	= 0x75,
	PCF50626_REG_RF1REGC1	= 0x76,
	PCF50626_REG_RF1REGC2	= 0x77,
	PCF50626_REG_RF1REGC3	= 0x78,
	PCF50626_REG_RF2REGC1	= 0x79,
	PCF50626_REG_RF2REGC2	= 0x7a,
	PCF50626_REG_RF2REGC3	= 0x7b,
	PCF50626_REG_RF3REGC1	= 0x7c,
	PCF50626_REG_RF3REGC2	= 0x7d,
	PCF50626_REG_RF3REGC3	= 0x7e,
	PCF50626_REG_RF4REGC1	= 0x7f,
	PCF50626_REG_RF4REGC2	= 0x80,
	PCF50626_REG_RF4REGC3	= 0x81,
	PCF50626_REG_IOREGC1	= 0x82,
	PCF50626_REG_IOREGC2	= 0x83,
	PCF50626_REG_IOREGC3	= 0x84,
	PCF50626_REG_USBREGC1	= 0x85,
	PCF50626_REG_USBREGC2	= 0x86,
	PCF50626_REG_USBREGC3	= 0x87,
	PCF50626_REG_USIMREGC1	= 0x88,
	PCF50626_REG_USIMREGC2	= 0x89,
	PCF50626_REG_USIMREGC3	= 0x8a,
	PCF50626_REG_LCREGC1	= 0x8b,
	PCF50626_REG_LCREGC2	= 0x8c,
	PCF50626_REG_LCREGC3	= 0x8d,
	PCF50626_REG_HCREGC1	= 0x8e,
	PCF50626_REG_HCREGC2	= 0x8f,
	PCF50626_REG_HCREGC3	= 0x90,
	PCF50626_REG_DCD1C1	= 0x91,
	PCF50626_REG_DCD1C2	= 0x92,
	PCF50626_REG_DCD1C3	= 0x93,
	PCF50626_REG_DCD1C4	= 0x94,
	PCF50626_REG_DCD1DVM1	= 0x95,
	PCF50626_REG_DCD1DVM2	= 0x96,
	PCF50626_REG_DCD1DVM3	= 0x97,
	PCF50626_REG_DCD1DVMTIM	= 0x98,
	PCF50626_REG_DCD2C1	= 0x99,
	PCF50626_REG_DCD2C2	= 0x9a,
	PCF50626_REG_DCD2C3	= 0x9b,
	PCF50626_REG_DCD2C4	= 0x9c,
	PCF50626_REG_DCD2DVM1	= 0x9d,
	PCF50626_REG_DCD2DVM2	= 0x9e,
	PCF50626_REG_DCD2DVM3	= 0x9f,
	PCF50626_REG_DCD2DVMTIM	= 0xa0,
	PCF50626_REG_DCUDC1	= 0xa1,
	PCF50626_REG_DCUDC2	= 0xa2,
	PCF50626_REG_DCUDC3	= 0xa3,
	PCF50626_REG_DCUDC4	= 0xa4,
	PCF50626_REG_DCUDDVMTIM	= 0xa5,
	PCF50626_REG_DCULEDC1	= 0xa6,
	PCF50626_REG_DCULEDC2	= 0xa7,
	PCF50626_REG_DCULEDC3	= 0xa8,
	PCF50626_REG_DIMMAN	= 0xa9,
	PCF50626_REG_ALMCAL	= 0xaa,
	PCF50626_REG_ALMCALMEA	= 0xab,
	PCF50626_REG_ALMCRV1	= 0xac,
	PCF50626_REG_ALMCRV2	= 0xad,
	PCF50626_REG_ALMCRV3	= 0xae,
	PCF50626_REG_ALMCRV4	= 0xaf,
	PCF50626_REG_GPIOS	= 0xb0,
	PCF50626_REG_DREGS1	= 0xb1,
	PCF50626_REG_DREGS2	= 0xb2,
	PCF50626_REG_RFREGS	= 0xb3,
	PCF50626_REG_GREGS	= 0xb4,
	PCF50626_REG_GPIO7C1	= 0xb5,
	PCF50626_REG_GPIO8C1	= 0xb6,
	PCF50626_REG_USIMDETC	= 0xb7,
	PCF50626_REG_TSINOI	= 0xfe,
	PCF50626_REG_TSIDAT4	= 0xff,
	__NUM_PCF50626_REGS
};

typedef struct {
	unsigned char tmp[255];
	unsigned char name[10];
}t_test;

int drive_writen(void *ctx, unsigned long reg, unsigned char *data, unsigned long len)
{
	t_test *tmp=(t_test*)ctx;
	int i;
	printf("%s write %x %x:\n",tmp->name,reg,len);
	for (i=0;i<len;i++) 
	{
		printf("%x ",data[i]);
	}
	printf("\n");
	memcpy(&tmp->tmp[reg],data,len);
	return len;
}

	
int drive_readn(void *ctx, unsigned long reg, unsigned char *data, unsigned long len)
{
	t_test *tmp=(t_test*)ctx;
	int i;
	printf("%s read %x %x\n",tmp->name,reg,len);
	printf("\n");
	memcpy(data, &tmp->tmp[reg],len);
    return len;
}


int main (int argc, char *argv[])
{
	t_test test;
	t_vreg *desc;
	int ret=0,i;
	unsigned char tmp[100];
	/* initialize test.tmp to 0*/
	memset(test.tmp,0,255);
	strcpy(test.name,"pmu");
	desc = vreg_alloc(&test,"PMU",0,0xff,drive_writen,drive_readn);
	/* id is share */
	ret+=vreg_user_desc(desc,PCF50626_REG_ID,1,VR_READ,0,0);	
	/* config gpio */
	ret+=vreg_user_desc(desc,PCF50626_REG_GPIO3C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_GPIO4C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_GPIO5C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);

	/* config for SIM register */
	ret+=vreg_user_desc(desc,PCF50626_REG_USIMREGC1,3, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_USIMREGC1,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	/* config for regu modem */
	ret+=vreg_user_desc(desc,PCF50626_REG_DCD1C1,16,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);
	ret+=vreg_user_desc(desc,PCF50626_REG_D1REGC1,12,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);

	ret+=vreg_user_desc(desc,PCF50626_REG_RF1REGC1,12,VR_READ | VR_WRITE | VR_EXCLU, 0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_RFREGS,1, VR_READ | VR_WRITE | VR_EXCLU,0,0); 

	/* interruption */
	ret+=vreg_user_desc(desc,PCF50626_REG_INT1,6, VR_READ | VR_CLEAR ,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_INT7,1, VR_READ | VR_CLEAR,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_INT1M,6, VR_WRITE ,0,0);
	ret+=vreg_user_desc(desc,PCF50626_REG_INT7M,1, VR_WRITE ,0,0);
	ret+=vreg_driver_desc(desc,PCF50626_REG_INT1,6, VR_READ | VR_CLEAR ,0,0);
	ret+=vreg_driver_desc(desc,PCF50626_REG_INT7,1, VR_READ | VR_CLEAR,0,0);
	ret+=vreg_driver_desc(desc,PCF50626_REG_INT1M,6, VR_WRITE ,0,0);
	ret+=vreg_driver_desc(desc,PCF50626_REG_INT7M,1, VR_WRITE ,0,0);

	if (ret!=0) TEST_ERROR("error 0\n");
	/* attempt to read area without right */
	if (vreg_driver_read(desc,PCF50626_REG_RTC1,tmp,1)!=0) TEST_ERROR("error\n");
	vreg_driver_desc(desc,	PCF50626_REG_RTC1, 1+PCF50626_REG_TSIDAT3-PCF50626_REG_RTC1, VR_WRITE | VR_READ | VR_EXCLU, 0,0);
	/* read with right */
	if (vreg_driver_read(desc,PCF50626_REG_RTC1,tmp,1)!=1) TEST_ERROR("error 1\n");
	if (vreg_driver_read(desc,PCF50626_REG_TSIDAT3,tmp,1)!=1) TEST_ERROR("error 2\n");
	/* go outside of area */
	if (vreg_driver_read(desc,PCF50626_REG_TSIDAT3+1,tmp,1)!=0) TEST_ERROR("error 3\n");

	/* attempt to define an area not share with another */
	if(vreg_driver_desc(desc,PCF50626_REG_RF1REGC1+10,4,VR_READ | VR_WRITE | VR_EXCLU, 0,0)!=6) TEST_ERROR("error 6\n");

	/* test the write of INT register */
	tmp[0]=0xff;
	test.tmp[PCF50626_REG_INT1]=0x0;
	if (vreg_driver_write(desc,PCF50626_REG_INT1,tmp,1)!=0) TEST_ERROR("error 7\n");
	if (test.tmp[PCF50626_REG_INT1]!=0) TEST_ERROR("error 8\n");
	/* test the read of interrupt register */
	test.tmp[PCF50626_REG_INT1]=0xff;
	tmp[0]=0x0;
	if (vreg_driver_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 8\n");
	if ( tmp[0]!=0xff) TEST_ERROR("error 9\n");
	tmp[0]=0x0;
	/* 1st read value */
	if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 9\n");
	if (tmp[0]!=0xff) TEST_ERROR("error 10\n");
	/* 2nd read value 0 */
	if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 11\n");
	if (tmp[0]!=0x0) TEST_ERROR("error 12\n");
	tmp[0]=0x0;
	/* a read from driver again */
	test.tmp[PCF50626_REG_INT1]=0x0f;
	if (vreg_driver_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 13\n");
	if ( tmp[0]!=0x0f) TEST_ERROR("error 14\n");
	/* change the stub content */
	test.tmp[PCF50626_REG_INT1]=0xf0;
	if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 15\n");
	if ( tmp[0]!=0x0f) TEST_ERROR("error 16\n");

	if (vreg_driver_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 17\n");
	if ( tmp[0]!=0xf0) TEST_ERROR("error 18\n");
	/* change the stub content */
	test.tmp[PCF50626_REG_INT1]=0x0f;
	if (vreg_driver_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 19\n");
	if ( tmp[0]!=0x0f) TEST_ERROR("error 20\n");
	if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 21\n");
	if (tmp[0]!=0xff) TEST_ERROR("error 22\n");
	/* 2nd read value 0 */
	if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 23\n");
	if (tmp[0]!=0x0) TEST_ERROR("error 24\n");
	tmp[0]=0x0;
	/* define an area with mask */


	/* define an area with polarity and mask */
	/* bit 6 or RECC1 has a polarity 0 
	 * user can only clear and set this bit there is a mask */
	{
		unsigned char pol[1]={1<<6};
		unsigned char mask[1]={1<<6};
		unsigned char mask1[1]={0xff};
		if (vreg_user_desc(desc, PCF50626_REG_RECC1,1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ , mask,pol)) TEST_ERROR("error 24\n");
		if (vreg_driver_desc(desc, PCF50626_REG_RECC1,1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ , mask1,pol)) TEST_ERROR("error 25\n");
	}
	/* read value at startup */
	/* start up test */
	if (vreg_user_read(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 25.1\n");
	if (tmp[0]!=(1<<6)) TEST_ERROR("error 25.2\n");
	if (vreg_driver_read(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 25.3\n");
	if (tmp[0]!=(1<<6)) TEST_ERROR("error 25.4\n");
	tmp[0]=0xff;
	if (vreg_driver_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 25.5\n");
	if (test.tmp[PCF50626_REG_RECC1]!=0xff)TEST_ERROR("error 25.6\n");
	tmp[0]=1<<6;
	if (vreg_driver_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 25.7\n");
	if (test.tmp[PCF50626_REG_RECC1]!=(1<<6))TEST_ERROR("error 25.8\n");
	tmp[0]=0x81;
	test.tmp[PCF50626_REG_RECC1]=0xff;
	if (vreg_user_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 26\n");
	if (test.tmp[PCF50626_REG_RECC1]!=0x00)TEST_ERROR("error 27\n");
	tmp[0]=0xff;
	if (vreg_driver_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 28\n");
	if (test.tmp[PCF50626_REG_RECC1]!=0xbf)TEST_ERROR("error 29\n");
	tmp[0]=0x40;
	if (vreg_user_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 30\n");
	if (test.tmp[PCF50626_REG_RECC1]!=0xff)TEST_ERROR("error 30\n");
	tmp[0]=0xbf;
	if (vreg_driver_write(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 31\n");
	if (test.tmp[PCF50626_REG_RECC1]!=0xbf)TEST_ERROR("error 32\n");
	if (vreg_user_read(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 33\n");
	if (tmp[0]!=0x40) TEST_ERROR("error 34\n");
	if (vreg_driver_read(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 35\n");
	if (tmp[0]!=0xbf) TEST_ERROR("error 36\n");
	/* simulate a status moving */
	test.tmp[PCF50626_REG_RECC1]=0x80;
	if (vreg_driver_read(desc,PCF50626_REG_RECC1,tmp,1)!=1) TEST_ERROR("error 37\n");
	if (tmp[0]!=0x80) TEST_ERROR("error 38\n");

	/*  specific attribute for RECC2, REC2S */ 
	if (vreg_driver_desc(desc,PCF50626_REG_RECC2, 1, VR_READ | VR_WRITE | VR_EXCLU, 0, 0)) TEST_ERROR("error 39\n");
	if (vreg_driver_desc(desc,PCF50626_REG_RECS, 1, VR_READ | VR_EXCLU, 0, 0)) TEST_ERROR("error 40\n");

	/* perform contiguous write */
	tmp[0]=0x18; tmp[1]=0x19; 
	if (vreg_driver_write(desc,PCF50626_REG_RECC1,tmp,2)!=2) TEST_ERROR("error 41\n");
	/* look content is ok */
	if ((test.tmp[PCF50626_REG_RECC1]!=0x18)||(test.tmp[PCF50626_REG_RECC2]!=0x19)) TEST_ERROR("error 42\n");
	/* perform contiguous read */
	/* set RECS */
	tmp[0]=0;tmp[1]=0;tmp[2]=0;
	test.tmp[PCF50626_REG_RECS]=0x55;
	if (vreg_driver_read(desc,PCF50626_REG_RECC1,tmp,3)!=3) TEST_ERROR("error 43\n");
	/* check value returned */
	if ((tmp[0]!=0x18)||(tmp[1]!=0x19)||(tmp[2]!=0x55 )) TEST_ERROR("error 44\n");

	/* check contiguous write and read */
	/* normal border */
	for (i=0;i<16;i++) tmp[i]=i;
	if (vreg_user_write(desc,PCF50626_REG_DCD1C1,tmp,16)!=16) TEST_ERROR("error 45\n");
	for(i=0;i<16;i++)
	{
		if (test.tmp[PCF50626_REG_DCD1C1+i]!=i) { TEST_ERROR("error 46\n");break;}
	}
	for (i=0;i<17;i++) tmp[i]=0;
	/* attempt to read outside , check that access is detected */
	if (vreg_user_read(desc,PCF50626_REG_DCD1C1,tmp,17)!=0) TEST_ERROR("error 47\n");
	/* check that the 1st 16 value have been read */
	for(i=0;i<16;i++) {
		if (tmp[i]!=i) { TEST_ERROR("error 48\n");break;}
	}
	/* attempt to write outside , check that access is detected */
	for(i=0;i<3;i++)tmp[i]=i;
	for(i=0;i<255;i++) test.tmp[i]=0x55;
	if (vreg_user_write(desc,PCF50626_REG_DCD1C1+15,tmp,3)!=0) TEST_ERROR("error 49\n");
	if ((test.tmp[PCF50626_REG_DCD1C1+15]!=0) || (test.tmp[PCF50626_REG_DCD1C1+14]!=0x55) || 
			(test.tmp[PCF50626_REG_DCD1C1+16]!=0x55) || (test.tmp[PCF50626_REG_DCD1C1+17]!=0x55)) 

		TEST_ERROR("error 50\n");
	/* read and write multiple on read clear area */

	/* read and write multiple on maskable area */
	/* this config is not realistic , it is only for test purpose */
	/* config */
	ret=0;
	for(i=0;i<10;i++) tmp[i]=0xf0;
	ret+=vreg_user_desc(desc, PCF50626_REG_ADCC2,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1, VR_READ | VR_WRITE | VR_MASK ,tmp,0);
	for(i=0;i<10;i++) tmp[i]=0x0f;
	ret+=vreg_driver_desc(desc, PCF50626_REG_ADCC2,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1, VR_READ | VR_WRITE | VR_MASK ,tmp,0);
	if (ret!=0) TEST_ERROR("error 51\n");
	/* write contiguous */
	/* set a default value */
	for(i=0;i<10;i++) test.tmp[ PCF50626_REG_ADCC2+i]=0x55;
	/* perform an access out the mask */
	if (vreg_user_write(desc, PCF50626_REG_ADCC2,tmp,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1)!=(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1))TEST_ERROR("error 52\n");
	/* check register is  zero */
	for(i=0;i<(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1);i++) 
		if (test.tmp[PCF50626_REG_ADCC2+i]!=0x0){ TEST_ERROR("error 53\n"); break;}
	for(i=0;i<10;i++) test.tmp[ PCF50626_REG_ADCC2+i]=0x55;
	for(i=0;i<10;i++) tmp[i]=0xf0;
	if (vreg_driver_write(desc, PCF50626_REG_ADCC2,tmp,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1)!=(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1))TEST_ERROR("error 54\n");
	/* check register is  zero */
	for(i=0;i<(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1);i++) 
		if (test.tmp[PCF50626_REG_ADCC2+i]!=0x0){ TEST_ERROR("error 55\n"); break;}
	/* read contiguous */
	for(i=0;i<(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1);i++) test.tmp[ PCF50626_REG_ADCC2+i]=0x55;
	if (vreg_user_read(desc, PCF50626_REG_ADCC2,tmp,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1)!=(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1))TEST_ERROR("error 56\n");
	/* check register is  zero */
	for(i=0;i<(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1);i++) 
		if (tmp[i]!=0x50){ TEST_ERROR("error 57\n"); break;}
	if (vreg_driver_read(desc, PCF50626_REG_ADCC2,tmp,PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1)!=(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1))TEST_ERROR("error 58\n");
	/* check register is  zero */
	for(i=0;i<(PCF50626_REG_ADCS3-PCF50626_REG_ADCC2+1);i++) 
		if (tmp[i]!=0x5){ TEST_ERROR("error 59\n"); break;}
	/*  polarity maskeable area limit test  */
	/* we use a different mask and polarity */
	{

		unsigned char pol[3]={0x10,0x20,0x11};
		unsigned char masku[3]={0xf0,0x20,0x11};
		unsigned char maskd[3]={0x1f,0x2f,0x11};
		unsigned char tmpu[3]={0x40,0x30,0xf0};
		unsigned char tmpd[3]={0x5,0x13,0xee};
		unsigned char resu[3]={0x40,0x20,0x10};
		unsigned char resd[3]={0x5,0x3,0x0};
		unsigned char resreg[3]={0x45,0x03,0x0};
		for (i=0;i<3;i++)tmp[i]=0;
		if (vreg_user_write(desc, PCF50626_REG_ADCC2,tmp,3)!=3) TEST_ERROR("error 60\n");
		if (vreg_driver_write(desc, PCF50626_REG_ADCC2,tmp,3)!=3) TEST_ERROR("error 61\n");

		/* reconfigure 3 registers */
		if (vreg_user_desc(desc,PCF50626_REG_ADCC2,3, VR_READ | VR_WRITE | VR_POLARITY | VR_MASK,masku,pol))TEST_ERROR("error 62\n"); 
		if (vreg_driver_desc(desc,PCF50626_REG_ADCC2,3, VR_READ | VR_WRITE | VR_POLARITY | VR_MASK,maskd,pol))TEST_ERROR("error 63\n");
		/* read and write multiple on share area not maskable */
		if (vreg_driver_write(desc,PCF50626_REG_ADCC2,tmpd,3)!=3) TEST_ERROR("error 64\n");
		if (vreg_user_write(desc,PCF50626_REG_ADCC2,tmpu,3)!=3) TEST_ERROR("error 65\n");
		/* check register value */
		for(i=0;i<3;i++)
		{
			if (test.tmp[ PCF50626_REG_ADCC2+i]!=resreg[i]) {
				TEST_ERROR("error 66\n");
				break;
			}
		}
		/* read value */
		if (vreg_user_read(desc, PCF50626_REG_ADCC2, tmp,3)!=3)TEST_ERROR("error 67\n");
		for(i=0;i<3;i++)
		{
			if (tmp[i]!=resu[i]) {TEST_ERROR("error 68\n");break;}
		}
		if (vreg_driver_read(desc, PCF50626_REG_ADCC2, tmp,3)!=3)TEST_ERROR("error 69\n");
		for(i=0;i<3;i++)
		{
			if (tmp[i]!=resd[i]) {TEST_ERROR("error 70\n");break;}
		}

	}

	/* test function relatif to pending with MASK*/
	{
		unsigned char masku[3]={0x10,0x20,0x11};
		unsigned char maskd[3]={0x1f,0x2f,0x11};
		unsigned char val1[3]={0x0f,0xf,0x00};
		unsigned char res1[3]={0xf,0xf,0x0};
		unsigned char val2[3]={0xe,0xff,0x30};
		unsigned char res2[3]={0x0e,0x2f,0x10};
		unsigned char resu2[3]={0x0,0x20,0x10};

		/* configure 3 registers */
		if (vreg_user_desc(desc,PCF50626_REG_TSIC2 ,3, VR_READ | VR_MASK | VR_CLEAR,masku,0))TEST_ERROR("error 71\n"); 
		if (vreg_driver_desc(desc,PCF50626_REG_TSIC2 ,3, VR_READ | VR_CLEAR | VR_MASK,maskd,0))TEST_ERROR("error 72\n");
		/* fix the value in stub */
		for (i=0;i<3;i++)
			test.tmp[PCF50626_REG_TSIC2+i]=val1[i];
		/* driver 1st read this register */
		if (vreg_driver_read(desc,PCF50626_REG_TSIC2,tmp,3)!=3) TEST_ERROR("error 73\n");
		/* test result */
		for (i=0;i<3;i++) 
		{
			if (tmp[i]!=res1[i])
			{
				TEST_ERROR("error 74\n");
				break;
			}	
		}
		/* check pending range */
		if (vreg_user_pending(desc,PCF50626_REG_TSIC2+1,3)!=-1) TEST_ERROR("error 76\n");
		/* check pending nul */
		if (vreg_user_pending(desc,PCF50626_REG_TSIC2,3)!=0) TEST_ERROR("error 75\n");
		/* fix the value in stub */
		for (i=0;i<3;i++)
			test.tmp[PCF50626_REG_TSIC2+i]=val2[i];

		if (vreg_driver_read(desc,PCF50626_REG_TSIC2,tmp,3)!=3) TEST_ERROR("error 76\n");
		/* test result */
		for (i=0;i<3;i++) 
		{
			if (tmp[i]!=res2[i])
			{
				TEST_ERROR("error 77\n");
				break;
			}	
		}
		/* check pending not nul */
		if (vreg_user_pending(desc,PCF50626_REG_TSIC2,3)!=1) TEST_ERROR("error 78\n");
		/* read value */
		if (vreg_user_read(desc,PCF50626_REG_TSIC2,tmp,3)!=3) TEST_ERROR("error 79\n");
		/* test result */
		for (i=0;i<3;i++) 
		{
			if (tmp[i]!=resu2[i])
			{
				TEST_ERROR("error 80\n");
				break;
			}	
		}
		/* check pending  nul */
		if (vreg_user_pending(desc,PCF50626_REG_TSIC2,3)!=0) TEST_ERROR("error 81\n");
	}
	/* test the function with VR_RESET */ 
	{   unsigned char int1=0xfe;
		unsigned char int2=0xff;
		unsigned char int3=0x0f;
		unsigned char int22=0xfa;
		unsigned char int23=0x0f;
		unsigned char int24=0xff;
		ret=0;
		ret+=vreg_user_desc(desc,PCF50626_REG_INT1,6, VR_READ | VR_RESET ,0,0);
		ret+=vreg_user_desc(desc,PCF50626_REG_INT7,1, VR_READ | VR_RESET,0,0);
		ret+=vreg_driver_desc(desc,PCF50626_REG_INT1,6, VR_READ | VR_RESET ,0,0);
		ret+=vreg_driver_desc(desc,PCF50626_REG_INT7,1, VR_READ | VR_RESET,0,0);
		if (ret!=0) TEST_ERROR("error 82\n");
		/* set the content of the register in read */
		test.tmp[PCF50626_REG_INT1]=int1;
		test.tmp[PCF50626_REG_INT2]=int2;
		test.tmp[PCF50626_REG_INT3]=int3;
		if (vreg_driver_read(desc,PCF50626_REG_INT1,tmp,3)!=3) TEST_ERROR("error 83\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT2,1)!=0) TEST_ERROR("error 84\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT1,1)!=1) TEST_ERROR("error 85\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT2,2)!=1) TEST_ERROR("error 86\n");
		if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 87\n");
		if (vreg_user_read(desc,PCF50626_REG_INT1,tmp,1)!=1) TEST_ERROR("error 88\n");
		if (tmp[0]!=0xff) TEST_ERROR("error 89\n");
		test.tmp[PCF50626_REG_INT2]=int22;
		if (vreg_driver_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 90\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT2,1)!=1) TEST_ERROR("error 84\n");
		test.tmp[PCF50626_REG_INT2]=int23;
		if (vreg_driver_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 90\n");
		if (tmp[0]!=int23) TEST_ERROR("error 90.1\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT2,1)!=1) TEST_ERROR("error 91\n");
		test.tmp[PCF50626_REG_INT2]=int24;
		if (vreg_driver_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 92\n");
		if (tmp[0]!=int24) TEST_ERROR("error 92.1\n");
		if (vreg_user_pending(desc,PCF50626_REG_INT2,1)!=1) TEST_ERROR("error 93\n");
		if (vreg_user_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 94\n");
		if (tmp[0]!=0x0a) TEST_ERROR("error 95\n");
		test.tmp[PCF50626_REG_INT2]=int23;
		if (vreg_user_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 96\n");
		if (tmp[0]!=0x0ff) TEST_ERROR("error 97\n");
		if (vreg_driver_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 98\n");
		if (tmp[0]!=int23) TEST_ERROR("error 97\n");
		if (vreg_user_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 98\n");
		if (tmp[0]!=int23) TEST_ERROR("error 99\n");
		if (vreg_user_read(desc,PCF50626_REG_INT2,tmp,1)!=1) TEST_ERROR("error 100\n");
		if (tmp[0]!=0x0ff) TEST_ERROR("error 101\n");
	}

	if (g_err==0) printf("All test OK\n");
	else printf("%d test KO\n",g_err);

}	
#endif
