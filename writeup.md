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