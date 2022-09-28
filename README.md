## myfuse

this is for my OS lecture file system lab

I intend to implement a XV6-like file system on fuse that can work in a real Linux machine

到目前为止已经基本完成，此项目只是玩具（还挺好玩的），并且存在许多 bug，我会尽量慢慢修复，虽然只是可能。虽然有单元测试，但是覆盖面并不全（写测试还是有点困难，特别是如果想要 cover 所有的 cases）

### make

```
mkdir build && cd ./build
cmake ..
ninja
```

### format a disk(or any file/block device)

```
./build/mkfs/mkfs.myfuse <path to file>
```

### mount!

```
./build/myfuse /path/to/mount --device_path <path to formated device>
```

### demo