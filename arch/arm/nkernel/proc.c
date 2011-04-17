/*
 ****************************************************************
 *
 * Component = Nano-Kernel control file /proc/nkrestart
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)proc.c 1.13     05/12/02 Jaluna"
 *
 * Contributor(s):
 *   Vladimir Grouzdev (grouzdev@jaluna.com) Jaluna SA
 *
 ****************************************************************
 */

/*
 * This module provides the /proc/nkrestart, /proc/nkstop and
 * /proc/nkresume files allowing to stop/resume and restart a
 * secondary kernel.
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <nk/nkern.h>
#include <asm/uaccess.h>
#include <asm/nkern.h>

#ifdef CONFIG_NKERNEL_PROC_RESTART

    static int
_nk_proc_open (struct inode* inode,
	       struct file*  file)
{
    return 0;
}

    static int
_nk_proc_release (struct inode* inode,
	          struct file*  file)
{
    return 0;
}

    static loff_t
_nk_proc_lseek (struct file* file,
	        loff_t       off,
	        int          whence)
{
    loff_t new;

    switch (whence) {
	case 0:	 new = off; break;
	case 1:	 new = file->f_pos + off; break;
	case 2:	 new = 1 + off; break;
	default: return -EINVAL;
    }

    if (new) {
	return -EINVAL;
    }

    return (file->f_pos = new);
}

    static ssize_t
_nk_proc_read (struct file* file,
	       char*        buf,
	       size_t       count,
	       loff_t*      ppos)
{
    return 0;
}

    static int
_nk_proc_getid (const char* buf, size_t size)
{
    NkOsId id = 0;
    while (size--) {
        char digit;
        if (copy_from_user(&digit, buf++, 1)) {
	    return -EFAULT;
	}
	if ((digit < '0') || ('9' < digit)) {
	    break;
	}
	id = (id * 10) + (digit - '0');
    }
    return id;
}

    static ssize_t
_nkrestart_proc_write (struct file* file,
	               const char*  buf,
	               size_t       size,
	               loff_t*      ppos)
{
    int id;

    if (*ppos || !size) {
	return 0;
    }

    id = _nk_proc_getid(buf, size);
    if (id < 0) {
	return id;
    }
    os_ctx->restart (os_ctx, id);

    return size;
}

    static ssize_t
_nkstop_proc_write (struct file* file,
	            const char*  buf,
	            size_t       size,
	            loff_t*      ppos)
{
    int id;

    if (*ppos || !size) {
	return 0;
    }

    id = _nk_proc_getid(buf, size);
    if (id < 0) {
	return id;
    }
    os_ctx->stop (os_ctx, id);

    return size;
}

    static ssize_t
_nkresume_proc_write (struct file* file,
	              const char*  buf,
	              size_t       size,
                      loff_t*      ppos)
{
    int id;

    if (*ppos || !size) {
	return 0;
    }

    id = _nk_proc_getid(buf, size);
    if (id < 0) {
	return id;
    }
    os_ctx->resume (os_ctx, id);

    return size;
}

static struct file_operations _nkrestart_proc_fops = {
    open:    _nk_proc_open,
    release: _nk_proc_release,
    llseek:  _nk_proc_lseek,
    read:    _nk_proc_read,
    write:   _nkrestart_proc_write,
};

static struct file_operations _nkstop_proc_fops = {
    open:    _nk_proc_open,
    release: _nk_proc_release,
    llseek:  _nk_proc_lseek,
    read:    _nk_proc_read,
    write:   _nkstop_proc_write,
};

static struct file_operations _nkresume_proc_fops = {
    open:    _nk_proc_open,
    release: _nk_proc_release,
    llseek:  _nk_proc_lseek,
    read:    _nk_proc_read,
    write:   _nkresume_proc_write,
};

    static void
_nk_proc_create (struct proc_dir_entry*  parent,
		 const char*             name,
	         struct file_operations* fops)
{
    struct proc_dir_entry* file;
    file = create_proc_entry(name, (S_IFREG|S_IRUGO|S_IWUSR), parent);
    if (!file) {
	printk("NK: error -- create_proc_entry(%s) failed\n", name);
	return;
    }
    file->data      = 0;
    file->size      = 0;
    file->proc_fops = fops;
}

#endif

    static int
_nkcontrol_module_init (void)
{
	/* PLA modif for 2.6.27 To Verify */
	struct proc_dir_entry* nk = proc_mkdir("nk", NULL);
	if (!nk) {
	    printk("NK: error -- proc_mkdir(/proc/nk) failed\n");
	    return 0;
    }
#ifdef CONFIG_NKERNEL_PROC_RESTART
    _nk_proc_create(nk, "restart", &_nkrestart_proc_fops);
    _nk_proc_create(nk, "stop",    &_nkstop_proc_fops);
    _nk_proc_create(nk, "resume",  &_nkresume_proc_fops);
#endif
    return 0;
}

    static void
_nkcontrol_module_exit (void)
{
#ifdef CONFIG_NKERNEL_PROC_RESTART
    struct proc_dir_entry* nk = _nk_proc_lookup("nk");
    if (nk) {
	remove_proc_entry("restart", nk);
	remove_proc_entry("stop",    nk);
	remove_proc_entry("resume",  nk);
    }
#endif
}

module_init(_nkcontrol_module_init);
module_exit(_nkcontrol_module_exit);
