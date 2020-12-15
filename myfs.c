#include <linux/module.h>
#include <linux/fs.h>

static int my_fill_super(struct super_block *sb, void *data, int silent)
{
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
  // 模块初始化时将文件系统类型注册到系统中
  return register_filesystem(&my_fs_type);
}

static void __exit exit_my_fs(void)
{
  unregister_filesystem(&my_fs_type);
}

module_init(init_my_fs);
module_exit(exit_my_fs);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("my filesystem");
MODULE_VERSION("Ver 0.1");