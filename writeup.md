```python
In [21]: def divide(start,end,part):
    ...:     number = end - start + 1
    ...:     chunk = number // part
    ...:     new_start = start
    ...:     new_end = number % part + chunk + start - 1
    ...:     print("[{},{}]".format(new_start,new_end))
    ...:     for i in range(new_end+1,end,chunk):
    ...:         print("[{},{}]".format(i,i+chunk-1))
```


因为之前写过lru+hash（不带锁），因此很自然的想法就是在此基础上加锁：做一个lru链表，使用hash索引，每次访问把键移到最前面去。   
但是按这个思路，它的提示就有点奇怪了。而且这样并行的效率不是很高。  
后面瞄了一眼网上的解答，发现是把`BUCKET`