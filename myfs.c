#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/cred.h>

#include "myfs.h"

/* 文件系统类型的唯一标志 */
#define MY_MAGIC     0x73616d70

struct inode_operations myfs_dir_inode_ops;
struct inode_operations myfs_file_inode_ops;

static void myfs_put_super(struct super_block *sb)
{
  struct myfs_sb_info *myfs_sb = NULL;
	myfs_sb = SFS_SB(sb);
	if (myfs_sb == NULL) {
		/* Empty superblock info passed to unmount */
		return;
	}

	/* FS-FILLIN your fs specific umount logic here */

	kfree(myfs_sb);
	return;
}

struct super_operations my_super_ops = { // 自定义 super_block 操作集合
	.statfs         = simple_statfs,
	.put_super      = myfs_put_super,
};

/**
 * 解析挂载参数，给特殊属性赋值
 */
static void
myfs_parse_mount_options(char *options, struct myfs_sb_info *myfs_sb)
{
	char *value;
	char *data;
	int size;

	if (!options)
		return;

	printk(KERN_INFO "myfs: parsing mount options %s\n", options);
	while ((data = strsep(&options, ",")) != NULL) {
		if (!*data)
			continue;
		if ((value = strchr(data, '=')) != NULL)
			*value++ = '\0';

		if (strncasecmp(data, "rsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0) {
					myfs_sb->rsize = size;
					printk(KERN_INFO "myfs: rsize %d\n", size);
				}
			}
		} else if (strncasecmp(data, "wsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0) {
					myfs_sb->wsize = size;
					printk(KERN_INFO "myfs: wsize %d\n", size);
				}
			}
		}  else {
			printk(KERN_WARNING "myfs: bad mount option %s\n", data);
		}
	}
}

/* No sense hanging on to negative dentries as they are only
in memory - we are not saving anything as we would for network
or disk filesystem */
int myfs_delete_dentry(const struct dentry *dentry)
{
	return 1;
}

static struct dentry_operations myfs_ci_dentry_ops = {
/*	.d_revalidate = xxxd_revalidate, Not needed for this type of fs */
	.d_delete = myfs_delete_dentry,
};

struct inode *myfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
  struct inode * inode;
	inode = new_inode(sb); // 分配一个新的 inode
	if (!inode)
		goto out;

	// 初始化 inode
	inode->i_mode = mode;
	inode->i_uid = current_fsuid();
  inode->i_gid = current_fsgid();
	inode->i_blocks = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	switch (mode & S_IFMT) {
		default: // 创建除了目录和普通文件之外的其他文件
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG: // 普通文件
			printk(KERN_INFO "file inode\n");
			inode->i_op = &myfs_file_inode_ops;
			break;
		case S_IFDIR: // 目录文件
			printk(KERN_INFO "directory inode super_block: %p\n", sb);
			inode->i_op = &myfs_dir_inode_ops;
			inode->i_fop = &simple_dir_operations; // 通过其中的 iterate_shared 方法支持 ls 调用
			// 目录的硬链接计数为 2，自身的 dentry 和自身的 "."
			set_nlink(inode, 2);
			break;
	}
out:	
	return inode;
}

static int myfs_fill_super(struct super_block *s, void *data, int silent)
{
  struct inode *inode; // 根目录的文件inode
	struct dentry *root; // 根目录的目录项
	struct myfs_sb_info *myfs_info;

	// 初始化超级块信息
	s->s_blocksize = PAGE_SIZE;
	s->s_blocksize_bits = PAGE_SHIFT;
	s->s_magic = MY_MAGIC;
	s->s_op = &my_super_ops;
	s->s_time_gran = 1;
	myfs_info = kzalloc(sizeof(struct myfs_sb_info), GFP_KERNEL);
	if (!myfs_info) {
		return -ENOMEM;
	}
	s->s_fs_info = myfs_info;

	inode = myfs_get_inode(s, S_IFDIR | 0755, 0);
	if (!inode) {
		kfree(myfs_info);
		return -ENOMEM;
	}
	root = d_make_root(inode); // 创建根 dentry
	if (!root) {
		kfree(myfs_info);
		return -ENOMEM;
	}
	s->s_root = root;

	// 解析 mount 参数，赋值 myfs 的特殊数据
	myfs_parse_mount_options(data, myfs_info);
	/* FS-FILLIN your filesystem specific mount logic/checks here */

	return 0;
}

static struct dentry *myfs_get_sb(struct file_system_type *fs_type,
                                int flags, const char *dev_name, void *data)
{
  return mount_nodev(fs_type, flags, data, myfs_fill_super);
}


/*
 * Lookup the data, if the dentry didn't already exist, it must be
 * negative.  Set d_op to delete negative dentries to save memory
 * (and since it does not help performance for in memory filesystem).
 */
// 用于支持在目录中找文件（根据文件名锁定文件），将结果填充给dentry
static struct dentry *myfs_lookup(struct inode *parent, struct dentry *dentry, unsigned int flags)
{
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	if (!dentry->d_sb->s_d_op)
		d_set_d_op(dentry, &myfs_ci_dentry_ops);
	d_add(dentry, NULL); // 查找过一次就重新进入 hash 表
	return NULL;
}

// 该函数将帮助我们创建目录和文件，这里考虑更普遍的情况——创建结点（mknod）
// dir：要创建的文件的父目录
// dentry：新建的文件的目录项
static int
myfs_mknod(struct inode *dir, struct dentry *dchild, umode_t mode, dev_t dev)
{
	struct inode * inode = myfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;
	
	printk(KERN_INFO "myfs: mknod\n");
	if (inode) {
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
		d_instantiate(dchild, inode); // 将新建的 inode 和 dentry 相关联
		dget(dchild); // 增加 dentry 的引用计数
		dir->i_mtime = dir->i_ctime = current_time(dir); // 更新父目录的时间

		error = 0;

		/* real filesystems would normally use i_size_write function */
		dir->i_size += 0x20;  /* bogus small size for each dir entry */
	}
	return error;
}

// 创建目录
static int myfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
	return myfs_mknod(dir, dentry, mode | S_IFDIR, 0);
}

// 创建普通文件
static int myfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool want_excl)
{
	return myfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

struct inode_operations myfs_dir_inode_ops = {
	.create         = myfs_create,
	.lookup         = myfs_lookup,
	.unlink         = simple_unlink,
	.mkdir          = myfs_mkdir,
	.rmdir          = simple_rmdir,
	.mknod          = myfs_mknod,
	.rename         = simple_rename,
};
struct inode_operations myfs_file_inode_ops = {
	.getattr        = simple_getattr,
};

static struct file_system_type my_fs_type = {
    .owner = THIS_MODULE,
    .name = "my",
    .mount = myfs_get_sb,
    .kill_sb = kill_anon_super,
    /*  .fs_flags */
};

static int __init init_my_fs(void)
{
	printk("init myfs\n");
  // 模块初始化时将文件系统类型注册到系统中
  return register_filesystem(&my_fs_type);
}

static void __exit exit_my_fs(void)
{
	printk("unloading myfs...\n");
  unregister_filesystem(&my_fs_type);
}

module_init(init_my_fs);
module_exit(exit_my_fs);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("my filesystem");
MODULE_VERSION("Ver 0.1.3");