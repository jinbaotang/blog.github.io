---
title: socket-can基础知识和使用
top: false
cover: false
toc: true
mathjax: true
date: 2021-05-07 22:13
password:
summary:
tags:
- socket
- can
categories:
- 协议栈
---

> 第一次接触socket can, 学习和使用的过程，也记录下来，加深印象。

# 一、 原理
&emsp;&emsp;虽然作为程序员最希望看到的技术文章就是直接上来甩出demo code，最好直接搬到对应的环境直接执行脚本就能跑起来，再不济也是有cmake，Makefile啥的。不管三七二十一，先跑起来再说，然后再深入学习，效率是最高的。但是这篇文章对我来说是个学习记录，还是得从原理上开始记录，后面整理时再调整顺序咯。


## 1. can总线原理
&emsp;&emsp;socket can涉及到CAN总线协议、套接字、Linux网络设备驱动等。