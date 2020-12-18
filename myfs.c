#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/cred.h>
#include <linux/backing-dev.h>
#include <linux/mm.h>

#include "myfs.h"

/* 文件系统类型的唯一标志 */
#define MY_MAGIC     0x73616d70

struct inode_operations myfs_dir_inode_ops;
struct inode_operations myfs_file_inode_ops;
struct file_operations myfs_file_operations;
struct address_space_operations myfs_aops;
struct super_operations my_super_ops;

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

struct inode *myfs_get_inode(struct super_block *sb, struct inode *dir, int mode, dev_t dev)
{
  struct inode * inode;
	inode = new_inode(sb); // 分配一个新的 inode
	if (!inode)
		goto out;

	// 初始化 inode
	inode->i_ino = get_next_ino(); // 获取下一个可用的 inode 号
	inode_init_owner(inode, dir, mode); // 初始化 uid,gid,mode
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode); // 初始化各种时间
	inode->i_mapping->a_ops = &myfs_aops;
	mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER); // GFP_HIGHUSER 表示保存内存页的物理内存优先从高端内存区域获取。
	mapping_set_unevictable(inode->i_mapping); // 为 inode 的 flags 添加 AS_UNEVICTABLE 标志。这样 myfs 涉及的内存页就会放入 unevictable_list 中，这些内存页不会再被回收
	switch (mode & S_IFMT) {
		default: // 创建除了目录和普通文件之外的其他文件
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG: // 普通文件
			printk(KERN_INFO "file inode: NO.%lu\n", inode->i_ino);
			inode->i_op = &myfs_file_inode_ops;
			inode->i_fop =  &myfs_file_operations;
			break;
		case S_IFDIR: // 目录文件
			printk(KERN_INFO "directory inode: NO.%lu\n", inode->i_ino);
			inode->i_op = &myfs_dir_inode_ops;
			inode->i_fop = &simple_dir_operations; // 通过其中的 iterate_shared 方法支持 ls 调用
			// 目录的硬链接计数为 2，自身的 dentry 和自身的 "."
			inc_nlink(inode);
			break;
		case S_IFLNK: // 符号链接文件
			printk(KERN_INFO "soft link inode: NO.%lu\n", inode->i_ino);
			inode->i_op = &page_symlink_inode_operations;
			inode_nohighmem(inode);
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
	s->s_fs_info = myfs_info; // 额外信息

	inode = myfs_get_inode(s, NULL, S_IFDIR | 0755, 0); // 创建根节点
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

// 该函数将帮助我们创建目录和文件，这里考虑更普遍的情况——创建结点（mknod）
// dir：要创建的文件的父目录
// dentry：新建的文件的目录项
static int
myfs_mknod(struct inode *dir, struct dentry *dchild, umode_t mode, dev_t dev)
{
	struct inode * inode = myfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;
	
	printk(KERN_INFO "myfs: mknod\n");
	if (inode) {
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
	int retval = myfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir); // 父目录的硬链接数+1，因为子目录的“..”
	return retval;
}

// 创建普通文件
static int myfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool want_excl)
{
	return myfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int myfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = myfs_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname) + 1;
		error = page_symlink(inode, symname, l); // 在 inode mapping 的也页缓存中写符号链接
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = current_time(dir);
		} else
			iput(inode);
	}
	return error;
}

/*
 * For address_spaces which do not use buffers nor write back.
 */
int __set_page_dirty_no_writeback(struct page *page)
{
	if (!PageDirty(page))
		return !TestSetPageDirty(page);
	return 0;
}

struct inode_operations myfs_dir_inode_ops = {
	.create         = myfs_create,
	.lookup         = simple_lookup, // 可以在此处重新设置 dentry 的操作函数
	.link						= simple_link, // 硬链接
	.unlink         = simple_unlink, // 删除文件
	.symlink				= myfs_symlink, // 软链接
	.mkdir          = myfs_mkdir,
	.rmdir          = simple_rmdir,
	.mknod          = myfs_mknod,
	.rename         = simple_rename,
};
struct inode_operations myfs_file_inode_ops = {
	.getattr        = simple_getattr,
	.setattr 				= simple_setattr,
};

static struct file_system_type my_fs_type = {
  .owner 					= THIS_MODULE,
  .name 					= "my",
  .mount 					= myfs_get_sb,
  .kill_sb				= kill_litter_super,
	.fs_flags				= FS_USERNS_MOUNT,
};


struct super_operations my_super_ops = { // 自定义 super_block 操作集合
	.statfs         = simple_statfs, // 给出文件系统的统计信息，例如使用和未使用的数据块的数目，或者文件件名的最大长度。
	.drop_inode			= generic_delete_inode, // 当inode的引用计数降为0时，将inode删除。
	.put_super      = myfs_put_super,
};

struct address_space_operations myfs_aops = {
	.readpage       = simple_readpage, // 用于从后备存储器将一页数据读入页框
	.write_begin	  = simple_write_begin, // 根据文件的读写位置pos计算出文件的页偏移值，根据索引查找或者分配一个page结构。将页中数据初始化为“０”。
	.write_end   		= simple_write_end, // 在对page执行写入操作后，执行相关更新操作。
	.set_page_dirty	= __set_page_dirty_no_writeback, // 将某个页标记为“脏”
};

struct file_operations myfs_file_operations = {
	.read_iter			= generic_file_read_iter,
	.write_iter			= generic_file_write_iter,
	.splice_read		= generic_file_splice_read,
	.splice_write		= iter_file_splice_write,
	.fsync          = noop_fsync, // 写回设备，内存文件系统不进行操作
	.llseek         = generic_file_llseek,
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