struct myfs_sb_info { // 文件系统的特殊数据
  unsigned int rsize;
  unsigned int wsize;
};

inline struct myfs_sb_info *SFS_SB(struct super_block *sb) {
  return sb->s_fs_info;
}