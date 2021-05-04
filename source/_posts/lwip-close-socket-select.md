---
title: LwIP使用select，close socket资源释放不完全问题
top: false
cover: false
toc: true
mathjax: true
date: 2021-04-12 18:56:38
password:
summary:
tags:
- LwIP
categories:
- 协议栈
---

> 这篇文章本应该在4月就写好的，但是博客评论系统一直没有搭建好，走了很多弯路，现在好了，delay这么久，终于要要补过来了。


# 一、简介
## 1. 题外话
&emsp;&emsp;以这篇文章为第一篇技术文章，一是萌生写博客的契机是换工作，另外就是这篇文章是我在怿星解决的最后一个bug。

![](/medias/lwip-close-socket-select/jinbao-ept.jpg)

&emsp;&emsp;问题来源是，跑在基于LwIP+FreeRTOS环境的DoIP，在反复初始化/反初始化时几次之后就会失败了。年初由于任务紧张，检查了下初始化和反初始化函数的流程，改掉了几处可能会出现问题的地方，问题依旧。但是同样的上层处理代码，在windows和linux环境下是没问题的，基本怀疑是LwIP某处不完善引起。一直拖到要离职，终于在离开的最后一天解决了，也算是给在怿星的DoIP协议栈画上一个属于自己的句号。

## 2. 原因
&emsp;&emsp;引起该问题的根本原因是，LwIP select函数里如果判断对应的socket没有事件产生（读/写/异常），进行简单处理后则改线程休眠，让出cpu控制权。如果在select休眠期间，进行了close socket的操作，会释放对应的socket pcb（**close(socket)**是成功的），然后在select休眠结束后，判断该socket资源不存在，则直接退出select函数，**但是**此时该socket的select_wait标志位没被清除。LwIP在分配socket时（资源都是静态分配的，类似于有一个socket数组，若分配则对应标志位为真），socket是否空闲是会对select_wait该标志位进行判断，所以即使该socket没有被使用，调用*socket()*函数时也会认为该socket是被占用的，所以几次之后，socket资源被**假耗尽**。

## 3. 解决
&emsp;&emsp;知道原因后，问题就好解决了。有以下两个解决问题的思路。
1. 更改LwIP源码，对对应的标志位进行判断和清除。该解决方案，如果能够push到LwIP主分支，则是一劳永逸的，否则如果要跟随LwIP官方更新，自己得维护一套代码，并持续merge。
2. 使用者，在使用接口时，做同步。即在select休眠期间不允许进行close socket操作，同时在close socket也不允许进入select函数。所以只要在两个函数之间加上条件判断就好。

&emsp;&emsp;考虑到维护成本，最终选择方案2.


# 二、分析
&emsp;&emsp;解决思路在上面已经给出，下面主要想从源码级对问题进行分析。



