how to build

```
gcc -o mem_access mem_access.c -lpthread
```



how to use

```
rm -f start_signal

./mem_access 12 8 3

sleep 60

echo "1" > start_signal
```

- 12代表占用内存大小
- 8代表线程数量
- 3代表顺序访问时长为随机访问时长的三倍

sleep 60用于等待内存填充完成

echo "1" > start_signal是一个启动线程的信号，用于在多线程模式下保证多个线程一起开始运行

运行时长60s
