# 自定义 Linux 文件系统

## change log

### 0.1.0

通过模块载入，注册新的文件系统类型到系统中

``` shell
# 编译
make
# 安装
make install
# 卸载
make uninstall
```

可在注册前后查看系统中支持的所有文件系统类型 `cat /proc/filesystem`

### 0.1.1

支持挂载文件系统，查看状态

``` shell
# 下列命令封装挂载命令 sudo mount -t my none /mnt
make mount

# 可用以下命令查看挂载的文件系统
mount
stat -f ./testmp
stat ./testmp

# 下列命令封装卸载文件系统命令 sudo umount /mnt
make umount
```

### 0.1.2

支持创建/删除文件（夹），文件（夹）改名

``` shell
# 在挂载的文件夹下，root 权限运行
# 创建文件夹
mkdir a & cd a
# 删除文件夹
cd .. & rmdir a
# 创建文件
touch f
# 文件改名
mv f ff
# 删除文件
rm ff
```

### 0.1.3

支持遍历目录文件内容，即 “ls”

### 0.1.4

支持打开文件，读/写文件，创建/删除软/硬链接

``` shell
mkdir d
# 测试文件读写
echo "hello world" > d/f
cat d/f
# 测试软链接
ln -s d/f df
cat df
```