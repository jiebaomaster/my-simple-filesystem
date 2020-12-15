#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include "myfs.h"

/* 文件系统类型的唯一标志 */
#define MY_MAGIC     0x73616d70

static void my_put_super(struct super_block *sb)
{
  struct myfs_sb_info *sfs_sb = NULL;
	sfs_sb = SFS_SB(sb);
	if (sfs_sb == NULL) {
		/* Empty superblock info passed to unmount */
		return;
	}

	/* FS-FILLIN your fs specific umount logic here */

	kfree(sfs_sb);
	return;
}

struct super_operations my_super_ops = { // 自定义 super_block 操作集合
	.statfs         = simple_statfs,
	.put_super      = my_put_super,
};

/**
 * 解析挂载参数，给特殊属性赋值
 */
static void
myfs_parse_mount_options(char *options, struct myfs_sb_info *sfs_sb)
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
					sfs_sb->rsize = size;
					printk(KERN_INFO "myfs: rsize %d\n", size);
				}
			}
		} else if (strncasecmp(data, "wsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0) {
					sfs_sb->wsize = size;
					printk(KERN_INFO "myfs: wsize %d\n", size);
				}
			}
		}  else {
			printk(KERN_WARNING "myfs: bad mount option %s\n", data);
		}
	}
}

static int my_fill_super(struct super_block *s, void *data, int silent)
{
  struct inode *inode; // 根目录的文件inode
	struct dentry *root; // 根目录的目录项
	struct myfs_sb_info *sfs_info;

	// 初始化超级块信息
	s->s_blocksize = PAGE_SIZE;
	s->s_blocksize_bits = PAGE_SHIFT;
	s->s_magic = MY_MAGIC;
	s->s_op = &my_super_ops;
	s->s_time_gran = 1;
	sfs_info = kzalloc(sizeof(struct myfs_sb_info), GFP_KERNEL);
	if (!sfs_info) {
		return -ENOMEM;
	}
	s->s_fs_info = sfs_info;
	inode = new_inode(s); // 创建根目录的 inode
	if (!inode) {
		kfree(sfs_info);
		return -ENOMEM;
	}
	/*
	 * because the root inode is 1, the files array must not contain an
	 * entry at index 1
	 */
	// 初始化根 inode
	inode->i_ino = 1;
	inode->i_mode = S_IFDIR | 0755;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	set_nlink(inode, 2); // 硬链接计数为2，自身的dentry和根目录的"."
	root = d_make_root(inode); // 创建根 dentry
	if (!root) {
		kfree(sfs_info);
		return -ENOMEM;
	}
	s->s_root = root;

	// 解析 mount 参数，赋值 myfs 的特殊数据
	myfs_parse_mount_options(data, sfs_info);
	/* FS-FILLIN your filesystem specific mount logic/checks here */

	return 0;
}

static struct dentry *my_get_sb(struct file_system_type *fs_type,
                                int flags, const char *dev_name, void *data)
{
  return mount_nodev(fs_type, flags, data, my_fill_super);
}

static struct file_system_type my_fs_type = {
    .owner = THIS_MODULE,
    .name = "my",
    .mount = my_get_sb,
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
	printk("unloading myfs.../n");
  unregister_filesystem(&my_fs_type);
}

module_init(init_my_fs);
module_exit(exit_my_fs);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("my filesystem");
MODULE_VERSION("Ver 0.1");