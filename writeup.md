# Using threads 
这个lab比较简单，只要加几个锁就行了。  
它这里的测试不是很完善，测的是同时使用get和同时使用put的情况，所以get在这个测试条件下是不需要加锁的。
```c
static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  pthread_mutex_lock(&lock); // 加锁
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }
  pthread_mutex_unlock(&lock); //去锁
}
```
最后结果是：
```
➜ xv6-labs-2020 (thread) ✗ ./ph 1             
100000 puts, 6.346 seconds, 15757 puts/second
0: 0 keys missing
100000 gets, 6.155 seconds, 16247 gets/second
➜ xv6-labs-2020 (thread) ✗ ./ph 2 
100000 puts, 2.981 seconds, 33542 puts/second
0: 0 keys missing
1: 0 keys missing
200000 gets, 5.853 seconds, 34168 gets/second
➜ xv6-labs-2020 (thread) ✗ ./grade-lab-thread ph_safe
make: 'kernel/kernel' is up to date.
== Test ph_safe == make: 'ph' is up to date.
ph_safe: OK (9.0s) 
```
但考虑到实际中一般是两者混着用，因此这样get和put肯定会冲突的。  
这样如果get不加锁就无法通过了。
这个问题可以用读者锁和写者锁来解决。