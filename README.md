# xhttpd

基于epoll实现的简易版http服务器

> epoll 仅支持在linux内核的OS中使用

1. 单文件
2. 仅500行
3. 无依赖
4. 支持get

#### START
```sh
make
./xhttpd 9527 path/to/dir
```

#### TODO
1. 支持post等更多HTTP方法
2. 支持中文编解码
3. 使用libevent实现跨平台
4. 支持Cache
