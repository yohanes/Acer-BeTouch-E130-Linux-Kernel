/*
 * ============================================================================
 *
 *       Filename:  vreg.h
 *
 *    Description:  This file implements functions dedicated to ip hardware
 *                  sharing between 2 users.
 *                  A memory bank (containing hardware register) is shared between 
 *                  a driver and a user.
 *                  The driver has dedicated attributes for register access.
 *                  The user has other attibutes for register access.
 *                  The driver is responsible 
 *                  - for configuring the access attributes of user and driver,
 *                  - providing service to the user for reading/writing register
 *                  - for taking care of interruption relative to this share IP
 *                  - for dispatching to the user the interruption source placed on the user
 *                  control.
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

#define VR_NOACCESS 0 /* default value,no acess */
#define VR_READ 1 
#define VR_WRITE  2
/* the 2 following mode applies to both user */
/* VR_WRITE is not possible in these 2 modes */
#define VR_CLEAR 4  /* the bits is cleared after read */
#define VR_RESET 8/* the bit is reset after read */
#define VR_MASK 16 /* VR_WRITE is possible in this mode */
                  /* the mask split the bits of a register between 2 users */
                  /* the bit that are not mask by the 2 user are shared between the 2 user */
                  /* this bits are provided in read as they are */
                  
#define VR_POLARITY 32 
/* if polarity is defined , 
						for a share register only polarity mask is used to compute the value
					   	 val0 =~(~val_0 & pol) val1 idem */

#define VR_EXCLU 64

typedef int (* t_nread)(void *ctx, unsigned long reg, unsigned char *data, unsigned long len);
typedef int (* t_nwrite)(void *ctx,unsigned long reg, unsigned char *data, unsigned long len);

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
extern void *vreg_alloc(void *ctx, 
		                char *name,
						unsigned long address, 
						int nb,  
						t_nwrite nwrite, 
						t_nread nread);

/** 
 * @brief release the descriptor of a shared register bank  
 * 
 * @param desc descriptor of bank register
 * 
 * @return none
 * */
extern void vreg_free(void *desc);



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
extern int vreg_driver_desc(void *desc,
					unsigned long offset, 
					unsigned long size, 
					unsigned char type, 
					unsigned char *mask, 
					unsigned char *polarity);

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
 

extern int vreg_user_desc(void *desc,
		                  unsigned long offset, 
						  unsigned long size, 
						  unsigned char type,
						  unsigned char *mask, 
						  unsigned char *polarity);



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
extern int vreg_driver_write(void *desc, unsigned long reg, void *data, int size);


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
extern int vreg_driver_read(void *desc,unsigned long reg, void *data, int size);


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
extern int vreg_user_write(void *desc, unsigned long reg, void *data, int size);
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
extern int vreg_user_read(void *desc, unsigned long reg, void *data, int size);

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
extern int vreg_user_pending(void *desc, unsigned long reg, int size);

