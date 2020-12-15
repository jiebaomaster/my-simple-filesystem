# 自定义 Linux 文件系统

## 安装

``` shell
$ make
$ make install
```

## change log

### 0.1.0

通过模块载入，注册新的文件系统类型到系统中
可在注册前后查看系统中支持的所有文件系统类型 `cat /proc/filesystem`
