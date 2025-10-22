## Project Overview

`mini_ext2` 是一个 **类 Ext2 文件系统实现**，在用户态模拟磁盘操作，实现了：

- 多级目录结构
- inode 索引机制
- 块位图 / inode 位图管理
- 文件的创建、删除、读写、打开与关闭
- 权限控制与 `chmod` 生效
- 多用户登录与 `/.session` 持久化
- 命令行 CLI 接口（可执行文件 `mini_ext2`）

整个系统仅依赖 C 标准库运行于 Linux 环境，使用 `disk.img` 作为虚拟磁盘文件。

------

## Directory Structure

```bash
project3/
├── include/
│   ├── errors.h
│   ├── fs.h
│   ├── layout.h
│   └── util.h
├── src/
│   ├── bitmap.c
│   ├── cli.c
│   ├── dev.c
│   ├── dir.c
│   ├── file.c
│   ├── fs.c
│   ├── inode.c
│   ├── security.c
│   └── util.c
├── Makefile
└── README.md
```

------

## Build & Run

### 编译项目

```
make
```

生成主程序：

```
./mini_ext2
```

### 格式化文件系统

首次运行必须初始化磁盘：

```
./mini_ext2 format
```

格式化后，会自动创建：

- 根目录 `/`
- 系统账户文件 `/.users`（包含 `root:root:0`）
- 会话文件 `/.session`（记录当前登录用户）

------

## Command Reference & Test Examples

下面是每个功能的命令及典型测试示例。

------

### 1. 文件系统初始化与目录操作

| 功能               | 命令                      |
| ------------------ | ------------------------- |
| 格式化虚拟磁盘     | `./mini_ext2 format`      |
| 创建目录           | `./mini_ext2 mkdir /doc`  |
| 进入目录           | `./mini_ext2 cd /doc`     |
| 列出目录内容       | `./mini_ext2 ls /doc`     |
| 删除目录（需为空） | `./mini_ext2 delete /doc` |

**测试示例**

```
./mini_ext2 format
./mini_ext2 mkdir /doc
./mini_ext2 ls /
```

------

### 2. 文件创建、读写与关闭

| 功能                 | 命令                                    |
| -------------------- | --------------------------------------- |
| 创建文件             | `./mini_ext2 create /doc/a.txt`         |
| 打开文件             | `./mini_ext2 open /doc/a.txt w`         |
| 写入数据（fd 方式）  | `./mini_ext2 write 0 "hello world"`     |
| 关闭文件             | `./mini_ext2 close 0`                   |
| 快速写入（路径方式） | `./mini_ext2 writef /doc/a.txt "hello"` |
| 读取文件             | `./mini_ext2 readf /doc/a.txt 5`        |
| 删除文件             | `./mini_ext2 delete /doc/a.txt`         |

**测试示例**

```
./mini_ext2 mkdir /doc
./mini_ext2 create /doc/a.txt
./mini_ext2 writef /doc/a.txt "hello world"
./mini_ext2 readf /doc/a.txt 11
```

------

### 3. 用户管理与登录验证

| 功能                  | 命令                                        |
| --------------------- | ------------------------------------------- |
| 登录                  | `./mini_ext2 login <username> <password>`   |
| 添加新用户（仅 root） | `./mini_ext2 useradd <username> <password>` |
| 修改密码              | `./mini_ext2 password <old> <new>`          |
| 查看当前用户          | `./mini_ext2 whoami`                        |

**测试示例**

```
# 以 root 身份登录
./mini_ext2 login root root

# 添加新用户 user / 密码 0000
./mini_ext2 useradd user 0000

# 切换为普通用户
./mini_ext2 login user 0000

# 查看身份
./mini_ext2 whoami
```

------

### 4. 权限控制 (`chmod`)

文件权限以 Linux 风格（`rwx`）管理，仅 root 或文件所有者可修改。

| 功能     | 命令                              |
| -------- | --------------------------------- |
| 修改权限 | `./mini_ext2 chmod <mode> <path>` |

示例：

```
# root 设置 600 权限，仅自己可读写
./mini_ext2 chmod 600 /doc/a.txt

# 普通用户访问被拒
./mini_ext2 login user 0000
./mini_ext2 readf /doc/a.txt 5
./mini_ext2 writef /doc/a.txt "xx"

# root 放开权限
./mini_ext2 login root root
./mini_ext2 chmod 666 /doc/a.txt
./mini_ext2 login user 0000
./mini_ext2 writef /doc/a.txt "OK"
```

输出：

```
read=ERR
wrote=2
```

说明权限控制生效。

------

### 5. 多级目录与路径解析

系统支持绝对与相对路径。

**测试示例**

```
./mini_ext2 mkdir /work
./mini_ext2 create /work/log.txt
./mini_ext2 writef /work/log.txt "log-entry"
./mini_ext2 ls /work
```

输出：

```
-rw-r--r-- 0 file log.txt 9 2025-10-22 ...
```

###  6. 时间戳与属性显示

`ls` 命令展示完整文件属性：

```
mode       uid  type name          size  ctime                mtime                atime
drwxr-xr-x 0    dir  .             192   2025-10-22 23:29:40 2025-10-22 23:29:45 2025-10-22 23:29:40
-rw------- 0    file a.txt         5     2025-10-22 23:29:53 2025-10-22 23:29:49 2025-10-22 23:29:45
```

------

### 7. 文件删除与资源回收

**示例**

```
./mini_ext2 delete /doc/a.txt
./mini_ext2 ls /doc
```

删除后 inode 与块位图同步更新。

------

### 8. 权限与会话持久化验证

每次运行都是独立进程，但系统会自动从 `/.session` 恢复登录态。

**测试**

```
./mini_ext2 login user 0000
# 退出终端后再运行：
./mini_ext2 whoami
```

输出：

```
current user: user (uid=1)
```

------

## Example Full Workflow

```
# 初始化
./mini_ext2 format
./mini_ext2 mkdir /doc
./mini_ext2 create /doc/a.txt
./mini_ext2 writef /doc/a.txt "hello world"
./mini_ext2 ls /doc

# 用户与权限测试
./mini_ext2 login root root
./mini_ext2 useradd user 0000
./mini_ext2 chmod 600 /doc/a.txt
./mini_ext2 login user 0000
./mini_ext2 readf /doc/a.txt 5     # 无权限
./mini_ext2 login root root
./mini_ext2 chmod 666 /doc/a.txt
./mini_ext2 login user 0000
./mini_ext2 writef /doc/a.txt "OK"
./mini_ext2 readf /doc/a.txt 13

# 验证目录操作
./mini_ext2 mkdir /work
./mini_ext2 create /work/test.txt
./mini_ext2 writef /work/test.txt "WorkDone"
./mini_ext2 ls /work
```

------

## Design Highlights

- 模拟 Ext2 文件系统结构：inode + 目录项 + 位图
- 单级间接块支持文件 > 10 个数据块
- 用户态实现，支持持久化登录态
- 权限完全模拟 UNIX 语义（`rwx` 位）
- 时间戳同步更新 ( `atime` / `mtime` / `ctime` )

------

## Clean Up

删除编译文件：

```
make clean
```

删除虚拟磁盘：

```
rm -f disk.img
```

------

## Notes

- `root` 拥有所有权限，可管理用户与权限。
- 普通用户受 `mode` 限制，只能访问自己创建的可读写文件。
- 每次操作会自动 mount 并恢复会话。
- 所有元数据立即写回，退出后状态保留于 `disk.img`。