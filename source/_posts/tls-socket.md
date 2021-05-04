---
title: 基于openssl实现tls+socket的安全通信
top: false
cover: false
toc: true
mathjax: true
date: 2021-05-05 01:13
password:
summary:
tags:
- tls,opensll
categories:
- 协议栈
---

> 记录基于openssl来实现tls的安全通信，借助BSD socket接口来实现。

# 一、 背景
&emsp;&emsp;由于业务需要，得使用tls来完成安全通信，需求也是借助开源的openssl来实现。整了好几天，现在还没实现openssl+socket来进行安全通信。目前是证书都已经生成，但是server获取client证书出错。

&emsp;&emsp;其中在生成证书的过程中，也走了不少弯路，网上没有找到一篇介绍加密通信、证书原理以及使用openssl的文章，所以就萌生了自己在尝试的过程中，记录一下自己的解决轨迹。