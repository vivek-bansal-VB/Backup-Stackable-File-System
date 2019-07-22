/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009      Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "bkpfs.h"
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <../../CSE-506/struct.h>

#define MY_UDBG printk(KERN_DEFAULT "DBG: %s:%s:%d\n", __FILE__, __func__, __LINE__)

/* reference : https://stackoverflow.com/questions/49713035/how-to-add-a-custom-extended-attribute-from-linux-kernel-space-i-e-from-a-custo */
#define MIN_VERSION "user.custom_attrib1"
#define MAX_VERSION "user.custom_attrib2"
#define MAX_FILES 10 /* keeping only 10 max backup files */

int bkpfs_create_backup_file(struct dentry *dentry);
int bkpfs_isversion_valid(struct dentry *dentry, int version_no);

static ssize_t bkpfs_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
{
        int err;
        struct file *lower_file;
        struct dentry *dentry = file->f_path.dentry;

        lower_file = bkpfs_lower_file(file);
        err = vfs_read(lower_file, buf, count, ppos);

        /* update our inode atime upon a successful lower read */
        if (err >= 0)
                fsstack_copy_attr_atime(d_inode(dentry),
                                        file_inode(lower_file));
        printk("In bkpfs_read of file.c\n");
        return err;
}

static ssize_t bkpfs_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *ppos)
{
        int err = 0;

        struct file *lower_file;
        struct dentry *dentry = file->f_path.dentry;

        err =  bkpfs_create_backup_file(dentry);
        if(err)
                return err;

        lower_file = bkpfs_lower_file(file);
        err = vfs_write(lower_file, buf, count, ppos);

        /* update our inode times+sizes upon a successful lower write */
        if (err >= 0) {
                fsstack_copy_inode_size(d_inode(dentry),
                                        file_inode(lower_file));
                fsstack_copy_attr_times(d_inode(dentry),
                                        file_inode(lower_file));
        }
        printk("In bkpfs_write of file.c\n");
        return err;
}

/* deleting backup files from minversion to maxversion */
int bkpfs_delete_backup_files(int minversion, int maxversion, struct dentry *dentry)
{
        struct vfsmount *lower_dir_mnt;
        struct dentry *lower_dir_dentry = NULL;
        const char *name;
        struct path lower_path;
        int err = 0, len;
        struct dentry  *parent;
        struct path lower_parent_path;
        char* back_up_filename = NULL;
        char temp[10];
        int version;

        parent = dget_parent(dentry);
        bkpfs_get_lower_path(parent, &lower_parent_path);

        /* must initialize dentry operations */
        d_set_d_op(dentry, &bkpfs_dops);

        if (IS_ROOT(dentry))
                goto out;

        for(version = minversion; version <= maxversion; version++)
        {
                MY_UDBG;
                name = dentry->d_name.name;
                sprintf(temp, "%d", version);
                len = strlen(name) + strlen(temp) + strlen(".tmp");
                back_up_filename = kmalloc(len, GFP_KERNEL); /* getting the backup filename */
                strcpy(back_up_filename, name);
                strcat(back_up_filename, temp);
                strcat(back_up_filename, ".tmp");

                /* now start the actual lookup procedure */
                lower_dir_dentry = lower_parent_path.dentry;
                lower_dir_mnt = lower_parent_path.mnt;
                MY_UDBG;

                /* Use vfs_path_lookup to check if the dentry exists or not */
                err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, back_up_filename, 0,
                              &lower_path);
                if(err)
                        goto out;

                /* deleting the file which we got from above path lookup */
                err = vfs_unlink(d_inode(lower_dir_dentry), lower_path.dentry, NULL);
                if(err)
                        goto out;

                if(back_up_filename)
                        kfree(back_up_filename);
        }

out:
        bkpfs_put_lower_path(parent, &lower_parent_path);
        bkpfs_put_lower_path(dentry, &lower_path);
        dput(parent);
        dput(lower_dir_dentry);
        return err;
}

/* view the contents of the backup file of version : version_no
 * reading max 4K from the file from the given offset : offset */
int bkpfs_view_backup_file(struct dentry *dentry, int version_no, char __user *content, loff_t offset){
        struct vfsmount *lower_dir_mnt;
        struct dentry *lower_dir_dentry = NULL;
        const char *name;
        struct path lower_path;
        int err = 0, len;
        struct dentry  *parent;
        struct path lower_parent_path;
        char* back_up_filename = NULL;
        char temp[10];
        struct file* infile = NULL;
        char* buf1 = NULL;
        int read_bytes = 0;
        mm_segment_t old_fs;

        /* if version is invalid, lies outside min and max version range */
        if(!bkpfs_isversion_valid(dentry, version_no)){
                printk("View Version invalid. Please enter a valid view version \n");
                err = -EINVAL;
                goto out;
        }

        parent = dget_parent(dentry);
        bkpfs_get_lower_path(parent, &lower_parent_path);

        d_set_d_op(dentry, &bkpfs_dops);
        name = dentry->d_name.name;
        sprintf(temp, "%d", version_no);
        len = strlen(name) + strlen(temp) + strlen(".tmp");
        back_up_filename = kmalloc(len, GFP_KERNEL);
        strcpy(back_up_filename, name);
        strcat(back_up_filename, temp);
        strcat(back_up_filename, ".tmp"); /* getting the backup filename */

        /* now start the actual lookup procedure */
        lower_dir_dentry = lower_parent_path.dentry;
        lower_dir_mnt = lower_parent_path.mnt;

        /* Use vfs_path_lookup to check if the dentry exists or not */
        err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, back_up_filename, 0,
                              &lower_path);
        if(err){
                printk("View version functionality is getting failed: path look up is returning an error\n");
                goto out;
        }


        infile = dentry_open(&lower_path, O_RDONLY, current_cred()); /* to get file* infile : using dentry_open */
        if(infile == NULL || IS_ERR(infile)){
                err = PTR_ERR(infile);
                goto out;
        }

        old_fs = get_fs();
        set_fs(KERNEL_DS);
        buf1 = kmalloc(PAGE_SIZE, GFP_KERNEL);
        if(buf1 == NULL){
                err= -ENOMEM;
                goto out;
        }
        read_bytes = vfs_read(infile, buf1, PAGE_SIZE, &offset); /* read from the file max 4K from the offset given by the user */
        buf1[read_bytes] = '\0';
        if(copy_to_user(content, buf1, read_bytes + 1)){ /* copy from the kernel space to the user space */
                printk("Copy to user got failed in listing of the backup files\n");
                err = -EFAULT;
                goto out;
        }
        kfree(buf1);
        set_fs(old_fs);

out: /* free up kernel heap memory */
        bkpfs_put_lower_path(parent, &lower_parent_path);
        bkpfs_put_lower_path(dentry, &lower_path);
        dput(parent);
        dput(lower_dir_dentry);
        if(infile && !IS_ERR(infile))
                fput(infile);
        if(back_up_filename)
                kfree(back_up_filename);
        return err;
}

/* getting minimum and maximum version of the file from the written attributes */
int bkpfs_list_backup_files(struct dentry *dentry, int* min_version, int* max_version)
{
        int max_attr = 0, err = 0;
        int* maxval = NULL;
        int min_attr = 0;
        int* minval = NULL;

        maxval = kmalloc(sizeof(int), GFP_KERNEL);
        if(maxval == NULL){
                err = -ENOMEM;
                goto out;
        }
        max_attr = vfs_getxattr(dentry, MAX_VERSION, maxval, sizeof(int)); /* getting max attribute */

        minval = kmalloc(sizeof(int), GFP_KERNEL);
        if(minval == NULL){
                err = -ENOMEM;
                goto out;
        }
        min_attr = vfs_getxattr(dentry, MIN_VERSION, minval, sizeof(int)); /* getting min attribute */
        *min_version = *minval;
        *max_version = *maxval;

out:
        if(maxval)
                kfree(maxval);
        if(minval)
                kfree(minval);
        return err;
}
/* check if the version_no is valid: a version exists of this number */
int bkpfs_isversion_valid(struct dentry *dentry, int version_no)
{
        int max_attr = 0;
        int* maxval = NULL;
        int min_attr = 0;
        int* minval = NULL;
        int isvalid = 1;
        int tmpmin = 0, tmpmax = 0;
        int err = 0;
        maxval = kmalloc(sizeof(int), GFP_KERNEL);
        if(maxval == NULL){
                err = -ENOMEM;
                goto out;
        }
        max_attr = vfs_getxattr(dentry, MAX_VERSION, maxval, sizeof(int));

        minval = kmalloc(sizeof(int), GFP_KERNEL);
        if(minval == NULL){
                err = -ENOMEM;
                goto out;
        }
        min_attr = vfs_getxattr(dentry, MIN_VERSION, minval, sizeof(int));
        tmpmin = *minval;
        tmpmax = *maxval;

        /* checking if a version is valid or not by observing the range */
        if(version_no < tmpmin || version_no > tmpmax)
                isvalid = 0;

out:
        if(minval)
                kfree(minval);
        if(maxval)
                kfree(maxval);
        return isvalid;
}

/* restore file to the backup file of version: version_no */
int bkpfs_restore_file(struct dentry *dentry, int version_no)
{
        struct vfsmount *lower_dir_mnt;
        struct dentry *lower_dir_dentry = NULL;
        const char *name;
        struct path lower_path;
        int err = 0, len;
        struct dentry  *parent;
        struct path lower_parent_path;
        char* back_up_filename = NULL;
        char temp[10];
        struct file* infile = NULL;
        struct file* outfile = NULL;
        struct path inpath;

        /* check if version is valid or not */
        if(!bkpfs_isversion_valid(dentry, version_no)){
                printk("Restore version invalid. Please enter a valid restore version \n");
                err = -EINVAL;
                goto out;
        }
        MY_UDBG;
        parent = dget_parent(dentry);
        bkpfs_get_lower_path(parent, &lower_parent_path);

        /* must initialize dentry operations */
        d_set_d_op(dentry, &bkpfs_dops);
        MY_UDBG;
        name = dentry->d_name.name;
        sprintf(temp, "%d", version_no);
        len = strlen(name) + strlen(temp) + strlen(".tmp");
        back_up_filename = kmalloc(len, GFP_KERNEL);
        strcpy(back_up_filename, name);
        strcat(back_up_filename, temp);
        strcat(back_up_filename, ".tmp");
        printk("filename = %s\n", back_up_filename);
        MY_UDBG;
        /* now start the actual lookup procedure */
        lower_dir_dentry = lower_parent_path.dentry;
        lower_dir_mnt = lower_parent_path.mnt;
        MY_UDBG;

        /* Use vfs_path_lookup to check if the dentry exists or not */
        err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, back_up_filename, 0,
                              &lower_path);
        if(err){
                printk("Restore functionality is getting failed: path look up is returning an error\n");
                goto out;
        }

        MY_UDBG;
        bkpfs_get_lower_path(dentry, &inpath);
        MY_UDBG;
        outfile = dentry_open(&inpath, O_RDWR, current_cred()); /* using dentry_open to get FILE pointer */
        if(outfile == NULL || IS_ERR(outfile)){
                err = PTR_ERR(outfile);
                goto out;
        }

        MY_UDBG;
        infile = dentry_open(&lower_path, O_RDWR, current_cred());
        if(infile == NULL || IS_ERR(infile)){
                err = PTR_ERR(infile);
                goto out;
        }
        MY_UDBG;
        err = vfs_copy_file_range(infile, 0, outfile, 0, infile->f_inode->i_size, 0); /* copy the contents from outfile to infile */
        if(err < 0){
                printk("err = %d\n", err);
                printk("Copy file in restore got failed\n");
                goto out;
        }
        MY_UDBG;

out:
        bkpfs_put_lower_path(parent, &lower_parent_path);
        bkpfs_put_lower_path(dentry, &lower_path);
        bkpfs_put_lower_path(dentry, &inpath);
        dput(parent);
        dput(lower_dir_dentry);
        if(infile && !IS_ERR(infile))
                fput(infile);
        if(outfile && !IS_ERR(outfile))
                fput(outfile);
        if(back_up_filename)
                kfree(back_up_filename);
        return err;
}

/* create backup file whenever a write happens to the original file */
int bkpfs_create_backup_file(struct dentry *dentry)
{
        int err = 0, ret, len1;
        struct vfsmount *lower_dir_mnt;
        struct dentry *lower_dir_dentry = NULL;
        struct dentry *lower_dentry;
        struct dentry *lower_parent_dentry = NULL;
        const char *name;
        char* back_up_filename = NULL;
        struct path lower_path;
        struct qstr this;
        struct dentry *parent;
        struct path lower_parent_path;
        struct file* infile = NULL;
        struct file* outfile = NULL;
        struct path inpath;
        int ret_attr = 0;
        int* attrval = NULL;
        int ret_attr1 = 0;
        int* attrval1 = NULL;
        int min_v = 0, max_v = 0;
        int version_no = 1, version_no1 = 1;
        char temp[10];


        parent = dget_parent(dentry);
        bkpfs_get_lower_path(parent, &lower_parent_path);

        name = dentry->d_name.name;
        lower_dir_dentry = lower_parent_path.dentry;
        lower_dir_mnt = lower_parent_path.mnt;

        MY_UDBG;

        /* trying to read the max/min extended attributes from the file */
        attrval = kmalloc(sizeof(int), GFP_KERNEL);
        ret_attr = vfs_getxattr(dentry, MAX_VERSION, attrval, sizeof(int));
        attrval1 = kmalloc(sizeof(int), GFP_KERNEL);
        ret_attr1 = vfs_getxattr(dentry, MIN_VERSION, attrval1, sizeof(int));

        /* If the first time we are coming here: means we need to create the extended attributes here */
        if(ret_attr < 0){
                *attrval = 1;
                printk("Setting initial attribute value\n");
                printk("first time : %d\n", *attrval);
                err = vfs_setxattr(dentry, (const char*)MAX_VERSION, &version_no, sizeof(int), 0);
                if(err < 0){
                        printk("MAX attribute setting failed\n");
                        goto out;
                }
                err = vfs_setxattr(dentry, (const char*)MIN_VERSION, &version_no1, sizeof(int), 0);
                if(err < 0){
                        printk("MIN attribute setting failed\n");
                        goto out;
                }
        }
        else{
                /* deleting oldest back up file if it exceeds MAX_FILES */
                (*attrval)++;
                min_v = *attrval1;
                max_v = *attrval;
                printk("min_v = %d max_v = %d\n", min_v, max_v);
                if((max_v - min_v) >= MAX_FILES){ /* retention policy */
                        bkpfs_delete_backup_files(min_v, min_v, dentry);
                        min_v++;
                        err = vfs_setxattr(dentry, (const char*)MIN_VERSION, &min_v, sizeof(int), 0); /* updating minimum version of the backup files */
                        if(err < 0){
                                printk("MIN attribute setting failed\n");
                                goto out;
                        }
                }
        }

        sprintf(temp, "%d", *attrval);
        len1 = strlen(name) + strlen(temp) + strlen(".tmp");
        back_up_filename = kmalloc(len1, GFP_KERNEL);
        strcpy(back_up_filename, name);
        strcat(back_up_filename, temp);
        strcat(back_up_filename, ".tmp");
        printk("backup = %s\n", back_up_filename); /* creating the backup filename by incrementing the max version available till now */
        this.name = back_up_filename;
        this.len = len1;
        this.hash = full_name_hash(lower_dir_dentry, this.name, this.len);

        MY_UDBG;

        lower_dentry = d_alloc(lower_dir_dentry, &this); /* creating a new dentry of this backup file */
        if (!lower_dentry) {
                err = -ENOMEM;
                goto out;
        }
        d_add(lower_dentry, NULL); /* instantiate and hash */
        lower_path.dentry = lower_dentry;
        lower_path.mnt = mntget(lower_dir_mnt);

        lower_parent_dentry = lock_parent(lower_dentry);
        MY_UDBG;

        err = vfs_create(d_inode(lower_parent_dentry), lower_dentry, dentry->d_inode->i_mode, true);
        if(err){
                printk("back up file creation failed\n");
                goto out;
        }
        MY_UDBG;
        if(lower_parent_dentry)
                unlock_dir(lower_parent_dentry);

        MY_UDBG;
        bkpfs_get_lower_path(dentry, &inpath);
                infile = dentry_open(&inpath, O_RDONLY, current_cred());
        if(infile == NULL || IS_ERR(infile)){
                err = PTR_ERR(infile);
                return err;
        }

        MY_UDBG;
        outfile = dentry_open(&lower_path, O_WRONLY, current_cred());
        if(outfile == NULL || IS_ERR(outfile)){
                err = PTR_ERR(outfile);
                return err;
        }
        MY_UDBG;
        ret = vfs_copy_file_range(infile, 0, outfile, 0, infile->f_inode->i_size, 0); /* copy the contents of the original file before writing it to this backup file */
        MY_UDBG;
        if(ret_attr >= 0){
                err = vfs_setxattr(dentry, MAX_VERSION, &max_v, sizeof(int), 0); /* updating the max extended attribute */
                if(err < 0){
                        printk("max attribute setting failed\n");
                        goto out;
                }
        }

out:
        bkpfs_put_lower_path(parent, &lower_parent_path);
        bkpfs_put_lower_path(dentry, &lower_path);
        bkpfs_put_lower_path(dentry, &inpath);
        dput(parent);
        dput(lower_dentry);
        dput(lower_dir_dentry);
        fput(infile);
        fput(outfile);
        if(back_up_filename)
                kfree(back_up_filename);
        if(attrval)
                kfree(attrval);
        if(attrval1)
                kfree(attrval1);
        return err;
}

struct bkpfs_getdents_callback {
        struct dir_context ctx;
        struct dir_context *caller;
        struct super_block *sb;
        int filldir_called;
        int entries_written;
};

/* implementation of retention policy:
 * we are fill the directory if we got a bakcup file.
 * Assumption: backup files extension is of .tmp.
 * So it will hide all the files with extension .tmp */
static int
bkpfs_filldir(struct dir_context *ctx, const char *lower_name,
                 int lower_namelen, loff_t offset, u64 ino, unsigned int d_type)
{
        struct bkpfs_getdents_callback *buf =
                container_of(ctx, struct bkpfs_getdents_callback, ctx);
        int rc;

        /* Reference : https://stackoverflow.com/questions/5309471/getting-file-extension-in-c */
        const char* ext = strrchr(lower_name, '.');
        if(ext && ext != lower_name)
        {
                if(!strcmp(ext, ".tmp"))
                        return 0;
        }

        buf->filldir_called++;
        buf->caller->pos = buf->ctx.pos;
        rc = !dir_emit(buf->caller, lower_name, lower_namelen, ino, d_type);
        if (!rc)
                buf->entries_written++;

        return rc;
}

static int bkpfs_readdir(struct file *file, struct dir_context *ctx)
{
        int err;
        struct file *lower_file = NULL;
        struct dentry *dentry = file->f_path.dentry;
        struct inode *inode = d_inode(dentry);
        struct bkpfs_getdents_callback buf = {
                .ctx.actor = bkpfs_filldir,
                .caller = ctx,
                .sb = inode->i_sb,
        };
        lower_file = bkpfs_lower_file(file);
        err = iterate_dir(lower_file, &buf.ctx);
        file->f_pos = lower_file->f_pos;

        if (err < 0)
                goto out;
        if (buf.filldir_called && !buf.entries_written)
                goto out;
        if (err >= 0)           /* copy the atime */
                fsstack_copy_attr_atime(inode,
                                        file_inode(lower_file));
out:
        return err;
}
/* Reference: https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/ */
static long bkpfs_unlocked_ioctl(struct file *file, unsigned int cmd,
                                  unsigned long argp)
{
        long err = -ENOTTY;
        struct dentry *dentry = file->f_path.dentry;
        void* kaddr = NULL;
        version* kaddr_struct = NULL, *vaddr_struct = NULL;

        int max_attr = 0, min_version = 0, max_version = 0;
        int* maxval = NULL;
        int min_attr = 0, restore_version = 0, view_version = 0;
        int* minval = NULL, tmpmax = 0, tmpmin = 0;
        int __user *arg = (int __user *)argp;

        printk("In bkpfs_unlocked_ioctl\n");

        if(arg == NULL)
        {
                printk("Empty arguments send from the user space \n");
                err = -EINVAL;
                goto out;
        }

        /* checking whether access on user space is valid */
        if(!access_ok(VERIFY_READ, (void*)arg, sizeof(version)))
        {
                err = -EFAULT;
                goto out;
        }

        /* creating an area for kernel address space */
        kaddr = kmalloc(sizeof(version), GFP_KERNEL);
        if(kaddr == NULL){
                err = -ENOMEM;
                goto out;
        }

        /* copying data from user space to kernel space */
        if(copy_from_user(kaddr, (void*)arg, sizeof(struct mystruct))){
                err = -EFAULT;
                goto out;
        }

        /* creating a structure into kernel space from the kaddr created above */
        kaddr_struct = (version*)kaddr;
        vaddr_struct = (version*)arg;
        if(kaddr_struct == NULL || vaddr_struct == NULL){
                err = -EINVAL;
                goto out;
        }

        MY_UDBG;
        /* populating kernel memory from the user memory */
        kaddr_struct->newest = vaddr_struct->newest;
        kaddr_struct->oldest = vaddr_struct->oldest;
        kaddr_struct->all = vaddr_struct->all;
        kaddr_struct->version_no = vaddr_struct->version_no;
        kaddr_struct->min_version = vaddr_struct->min_version;
        kaddr_struct->max_version = vaddr_struct->max_version;
        kaddr_struct->offset = vaddr_struct->offset;

        maxval = kmalloc(sizeof(int), GFP_KERNEL);
        if(maxval == NULL){
                err = -ENOMEM;
                goto out;
        }
        max_attr = vfs_getxattr(dentry, MAX_VERSION, maxval, sizeof(int)); /* retrieving maximum attribute */
        minval = kmalloc(sizeof(int), GFP_KERNEL);
        if(minval == NULL){
                err = -ENOMEM;
                goto out;
        }
        min_attr = vfs_getxattr(dentry, MIN_VERSION, minval, sizeof(int)); /* retrieving minimum attribute */
        MY_UDBG;

        switch(cmd){
                case BKPFS_LIST: /* list all the backup files available of this file */
                        err = bkpfs_list_backup_files(dentry, &kaddr_struct->min_version, &kaddr_struct->max_version);
                        if(err){
                                printk("Error occured in listing of the files\n");
                                goto out;
                        }
                        /* copy to user space: minimum and maximum version */
                        if(copy_to_user((int*) &(vaddr_struct->min_version), &kaddr_struct->min_version, sizeof(int))){
                                printk("Copy to user got failed in listing of the backup files\n");
                                err = -EFAULT;
                                goto out;
                        }
                        if(copy_to_user((int*) &(vaddr_struct->max_version), &kaddr_struct->max_version, sizeof(int))){
                                printk("Copy to user got failed in listing of the backup files\n");
                                err = -EFAULT;
                                goto out;
                        }
                        break;
                                        case BKPFS_DEL: /* delete a backup file */
                        if(kaddr_struct->newest)
                                min_version = max_version = *maxval;
                        else if(kaddr_struct->oldest)
                                min_version = max_version = *minval;
                        else if(kaddr_struct->all){
                                min_version = *minval;
                                max_version = *maxval;
                        }
                        else{
                                err = -EINVAL;
                                goto out;
                        }
                        MY_UDBG;
                        err = bkpfs_delete_backup_files(min_version, max_version, dentry);
                        if(err){
                                printk("Delete operation on back up file gets failed\n");
                                goto out;
                        }

                        tmpmax = *maxval;
                        tmpmin = *minval;

                        /* update extended attributes after deleting the back up file */
                        if((tmpmax == tmpmin) || kaddr_struct->all) /* If only one version was there or if all versions have to be deleted, then remove attr */
                        {
                                err = vfs_removexattr(dentry, MAX_VERSION);
                                if(err && err != -ENODATA && err != -EOPNOTSUPP)
                                {
                                        printk("removing extended attribute: MAX_VERSION failed in deleting the file\n");
                                        goto out;
                                }
                                err = vfs_removexattr(dentry, MIN_VERSION);
                                if(err && err != -ENODATA && err != -EOPNOTSUPP)
                                {
                                        printk("removing extended attribute: MIN_VERSION failed in deleting the file\n");
                                        goto out;
                                }
                        }
                        else if(kaddr_struct->newest){
                                tmpmax--;
                                printk("max_version = %d\n", tmpmax);
                                err = vfs_setxattr(dentry, (const char*)MAX_VERSION, &tmpmax, sizeof(int), 0);
                                if(err){
                                        printk("updating extended attribute: MAX_VERSION failed in deleting the file\n");
                                        goto out;
                                }
                        }
                        else if(kaddr_struct->oldest){
                                tmpmin++;
                                printk("min_version = %d\n", tmpmin);
                                err = vfs_setxattr(dentry, (const char*)MIN_VERSION, &tmpmin, sizeof(int), 0);
                                if(err){
                                        printk("updating extended attribute: MIN_VERSION failed in deleting the file\n");
                                        goto out;
                                }
                        }

                        MY_UDBG;
                        break;

                case BKPFS_VIEW: /* view the version: view_version of the backup file */
                        if(kaddr_struct->newest)
                                view_version = *maxval;
                        else if(kaddr_struct->oldest)
                                view_version = *minval;
                        else if(kaddr_struct->version_no)
                                view_version = kaddr_struct->version_no;
                        else{
                                printk("Invalid input passes while viewing version file\n");
                                err = -EINVAL;
                                break;
                        }
                        err = bkpfs_view_backup_file(dentry, view_version, vaddr_struct->file_content, vaddr_struct->offset);
                        break;

                case BKPFS_RESTORE: /* restore the original file to the backup version: restore_version */
                        if(kaddr_struct->newest)
                                restore_version = *maxval;
                        else if(kaddr_struct->version_no){
                                restore_version = kaddr_struct->version_no;
                        }
                        else{
                                err = -EINVAL;
                                goto out;
                        }
                        err = bkpfs_restore_file(dentry, restore_version);
                        break;

                default:
                        break;
        }

MY_UDBG;
out:
        if(kaddr)
                kfree(kaddr);
        if(minval)
                kfree(minval);
        if(maxval)
                kfree(maxval);
        return err;
}

#ifdef CONFIG_COMPAT
static long bkpfs_compat_ioctl(struct file *file, unsigned int cmd,
                                unsigned long arg)
{
        long err = -ENOTTY;
        struct file *lower_file;

        lower_file = bkpfs_lower_file(file);

        /* XXX: use vfs_ioctl if/when VFS exports it */
        if (!lower_file || !lower_file->f_op)
                goto out;
        if (lower_file->f_op->compat_ioctl)
                err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
        return err;
}
#endif