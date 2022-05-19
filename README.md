# xhttpd

基于epoll实现的简易版http并发服务器

> 当前仅linux支持`epoll`并发模式

1. 单文件
2. 500行
3. 无依赖
4. 支持GET

#### START
```sh
make
./xhttpd 9527 path/to/dir
```

#### TODO
1. 支持更多HTTP方法
2. 支持中文编解码

