### 哈希算法介绍
哈希算法是一致性哈希算法中重要的一个组成部分，你可以借助 Java 中的 int hashCode() 去理解它。 说到哈希算法，你想到了什么？Jdk 中的 hashCode、SHA-1、MD5，除了这些耳熟能详的哈希算法，还存在很多其他实现，详见 HASH 算法一览。可以将他们分成三代：

第一代：SHA-1（1993），MD5（1992），CRC（1975），Lookup3（2006）
第二代：MurmurHash（2008）
第三代：CityHash， SpookyHash（2011）

### 一致性哈希

一致性Hash算法的基本优势是：当slot数发生变化时，能够尽量少的移动数据。

首先将服务器（ip+ 端口号）进行哈希，映射成环上的一个节点，在请求到来时，根据指定的 hash key 同样映射到环上，并顺时针选取最近的一个服务器节点进行请求（在本图中，使用的是 userId 作为 hash key）。

当环上的服务器较少时，即使哈希算法选择得当，依旧会遇到大量请求落到同一个节点的问题，为避免这样的问题，大多数一致性哈希算法的实现度引入了虚拟节点的概念。

![image](https://201911-1251969284.cos.ap-shanghai.myqcloud.com/consistent_hash.png)



### REF
- [https://github.com/Yikun/hashes](https://github.com/Yikun/hashes)
- [https://www.cnkirito.moe/consistent-hash-lb/](https://www.cnkirito.moe/consistent-hash-lb/)
- [https://www.zsythink.net/archives/1182](https://www.zsythink.net/archives/1182)
