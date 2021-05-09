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
- socket
categories:
- 协议栈
---

> 这篇文章本应该在4月就写好的，但是博客评论系统一直没有搭建好，走了很多弯路，现在好了，delay这么久，终于要要补过来了。

> 该文章完全原创，除通用、广泛的知识点外，均为个人总结，如需转载还望备注出处，同时如有错误还请指出，虚心接受。

# 一、简介
## 1. 题外话
&emsp;&emsp;以这篇文章为第一篇技术文章，一是萌生写博客的契机是换工作，另外就是这篇文章是我在怿星解决的最后一个bug。

![](/medias/lwip-close-socket-select/jinbao-ept.jpg)

&emsp;&emsp;问题来源是，跑在基于LwIP+FreeRTOS环境的DoIP，在反复初始化/反初始化时几次之后就会失败了。年初由于任务紧张，检查了下初始化和反初始化函数的流程，改掉了几处可能会出现问题的地方，问题依旧。但是同样的上层处理代码，在windows和linux环境下是没问题的，基本怀疑是LwIP某处不完善引起。一直拖到要离职，终于在离开的最后一天解决了，也算是给在怿星的DoIP协议栈画上一个属于自己的句号。

&emsp;&emsp; LwIP 全名为 Light weight IP，意思是轻量化的 TCP/IP 协议， 是瑞典计算机科学院(SICS)的 Adam Dunkels 开发的一个小型开源的 TCP/IP 协议栈。 LwIP 的设计初衷是：用少量的资源消耗(RAM)实现一个较为完整的 TCP/IP 协议栈，其中“完整”主要指的是 TCP 协议的完整性， 实现的重点是在保持 TCP 协议主要功能的基础上减少对 RAM 的占用。此外 LwIP既可以移植到操作系统上运行，也可以在无操作系统的情况下独立运行。
## 2. 原因
&emsp;&emsp;引起该问题的根本原因是，LwIP select函数里如果判断对应的socket没有事件产生（读/写/异常），进行简单处理后则改线程休眠，让出cpu控制权。如果在select休眠期间，进行了close socket的操作，会释放对应的socket pcb（**close\(socket\)**是成功的），然后在select休眠结束后，判断该socket资源不存在，则直接退出select函数，**但是**此时该socket的select_wait标志位没被清除。LwIP在分配socket时（资源都是静态分配的，类似于有一个socket数组，若分配则对应标志位为真），socket是否空闲是会对select_wait该标志位进行判断，所以即使该socket没有被使用，调用*socket()*函数时也会认为该socket是被占用的，所以几次之后，socket资源被**假耗尽**。

## 3. 解决
&emsp;&emsp;知道原因后，问题就好解决了。有以下两个解决问题的思路。
1. 更改LwIP源码，对对应的标志位进行判断和清除。该解决方案，如果能够push到LwIP主分支，则是一劳永逸的，否则如果要跟随LwIP官方更新，自己得维护一套代码，并持续merge。
2. 使用者，在使用接口时，做同步。即在select休眠期间不允许进行close socket操作，同时在close socket也不允许进入select函数。所以只要在两个函数之间加上条件判断就好。

&emsp;&emsp;考虑到维护成本，最终选择方案2.


# 二、分析
&emsp;&emsp;解决思路在上面已经给出，下面主要想从源码级对问题进行分析。原因中，涉及三个函数，
1. socket函数，即lwip_socket，函数原型如下：
`int lwip_socket(int domain, int type, int protocol)`

2. close函数，即lwip_close，原型如下：
`int lwip_close(int s)`

3. select函数， 即lwip_select(),原型如下：
`intlwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)`


## 1. 拓展
&emsp;&emsp;LwIP本身提供了类似于bsd socket编程模型，同时也实现了简易版的select函数。

&emsp;&emsp;关于socket编程的教程是实在太多了，在这不再重复去描述，[socket编程参考链接](https://blog.csdn.net/weixin_39634961/article/details/80236161)。辅导过一些人进行socket编程，初学者包括我自己，容易忽略的一点就是，作为server时，listen-socket和accept-socket不是一回事。可以理解为listen-socket窗口，窗口只是负责监听有谁要走通道，走哪个通道，并把真正的通道--accept-socket给到上层。对于其他的，感觉跑跑示例程序，单步走一下，就基本理解了。

&emsp;&emsp;在不使用select时，并没有发现socket资源释放不完全的问题。本文不展开讲解lwip select的实现，但是对于select的使用需要稍微展开下，[select编程参考链接](https://www.cnblogs.com/skyfsm/p/7079458.html)。关于select本质上是一个同步I/O函数，只不过改同步函数可以同时监控多个"IO"通道，所以也称为多路复用。熟悉了上面的socket编程后，如果需要实现多个socket同时通信的话，就应该给每个socket开一个线程，在负载不是特别高的情况下会显得效率特别低，同时线程太多，就不得不考虑资源竞争的问题，如果竞态条件太多，也容易产生问题（**多线程资源竞争问题**）。多路复用即是用一个线程监听多个通道（描述符），一旦某个描述符就绪（可读、可写或者异常），就通知程序进行相应的读写操作。上庙的描述，看起来select是异步的，其实不然，因为产生读写事件后，应用程序必须自己负责读写操作，读写操作本身是阻塞的，而异步I/O是不需要自己读写；同时即使没有读写事件产生，select函数本身也是阻塞的，加了超时也是阻塞的，只不过给阻塞增加了一个时间限制。

&emsp;&emsp;select最早于1983年出现在4.2BSD中，它通过一个select()系统调用来监视多个文件描述符的数组，当select()返回后，该数组中就绪的文件描述符便会被内核修改标志位，使得进程可以获得这些文件描述符从而进行后续的读写操作。从[select编程参考链接](https://www.cnblogs.com/skyfsm/p/7079458.html)中可以看出最终每个socket都对应到每个bit上，如果对应的socket有事件产生，则会被置位。

## 2. 函数分析
&emsp;&emsp;该节分析函数socket，close，select实现细节。**LwIP版本2.1.4**。

### 2.1 socket函数
&emsp;&emsp;lwip中`#define socket lwip_socket`.
```c
int lwip_socket(int domain, int type, int protocol) {
    struct netconn *conn;
    int i;
    LWIP_UNUSED_ARG(domain);
    /* @todo: check this */
    /* create a netconn */
    /* 下面主要是针对不同的socket类型，分配空间，对相应的成员进行赋值，空间资源为预分配给lwip的堆空间
     */
    switch (type) {
    case SOCK_RAW:
        conn = netconn_new_with_proto_and_callback(
            DOMAIN_TO_NETCONN_TYPE(domain, NETCONN_RAW), (u8_t)protocol,
            event_callback);
        LWIP_DEBUGF(SOCKETS_DEBUG,
                    ("lwip_socket(%s, SOCK_RAW, %d) = ",
                     domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
        break;
    case SOCK_DGRAM:
        conn = netconn_new_with_callback(
            DOMAIN_TO_NETCONN_TYPE(domain, ((protocol == IPPROTO_UDPLITE)
                                                ? NETCONN_UDPLITE
                                                : NETCONN_UDP)),
            event_callback);
        LWIP_DEBUGF(SOCKETS_DEBUG,
                    ("lwip_socket(%s, SOCK_DGRAM, %d) = ",
                     domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
        break;
    case SOCK_STREAM:
        conn = netconn_new_with_callback(
            DOMAIN_TO_NETCONN_TYPE(domain, NETCONN_TCP), event_callback);
        LWIP_DEBUGF(SOCKETS_DEBUG,
                    ("lwip_socket(%s, SOCK_STREAM, %d) = ",
                     domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
        break;
    default:
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_socket(%d, %d/UNKNOWN, %d) = -1\n",
                                    domain, type, protocol));
        set_errno(EINVAL);
        return -1;
    }
    if (!conn) {
        LWIP_DEBUGF(SOCKETS_DEBUG,
                    ("-1 / ENOBUFS (could not create netconn)\n"));
        set_errno(ENOBUFS);
        return -1;
    }
    /* 
    *上面已经分配好了，对应的connection空间，最终要对应的socket上，即socket数组，见下面alloc_socket实现。 
    */
    i = alloc_socket(conn, 0);
    if (i == -1) {
        netconn_delete(conn);
        set_errno(ENFILE);
        return -1;
    }
    conn->socket = i;
    LWIP_DEBUGF(SOCKETS_DEBUG, ("%d\n", i));
    set_errno(0);
    return i;
}
```
```c
static int alloc_socket(struct netconn *newconn, int accepted){
  int i;
  SYS_ARCH_DECL_PROTECT(lev);

  /* allocate a new socket identifier */
  for (i = 0; i < NUM_SOCKETS; ++i) {
    /* Protect socket array */
    SYS_ARCH_PROTECT(lev);
    if (!sockets[i].conn && (sockets[i].select_waiting == 0)) {
      sockets[i].conn       = newconn;
      /* The socket is not yet known to anyone, so no need to protect
         after having marked it as used. */
      SYS_ARCH_UNPROTECT(lev);
      sockets[i].lastdata   = NULL;
      sockets[i].lastoffset = 0;
      sockets[i].rcvevent   = 0;
      /* TCP sendbuf is empty, but the socket is not yet writable until connected
       * (unless it has been created by accept()). */
      sockets[i].sendevent  = (NETCONNTYPE_GROUP(newconn->type) == NETCONN_TCP ? (accepted != 0) : 1);
      sockets[i].errevent   = 0;
      sockets[i].err        = 0;
      return i + LWIP_SOCKET_OFFSET;
    }
    SYS_ARCH_UNPROTECT(lev);
  }
  return -1;
}
```
&emsp;&emsp;可以看到，判断socket资源是否有人在使用时，除了判断socket->conn是否为空，还会判断select_waiting是否等于0。其中select_waiting标识该socket正在被多少个线程在使用。即要释放socket资源（说释放有点不是很准确，因为在lwip中，socket资源是编译前分配的），两个重要条件是，socket->conn必须为空，并且select_waiting要为0.

### 2.2 close函数
&emsp;&emsp;接下来看看close函数的实现，看为啥会导致资源释放不完全。
```c
int lwip_close(int s){
  struct lwip_sock *sock;
  int is_tcp = 0;
  err_t err;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_close(%d)\n", s));
	/* 本质上是，通过socket数组下标获取到socket结构体 */
  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (sock->conn != NULL) {
    is_tcp = NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP;
  } else {
    LWIP_ASSERT("sock->lastdata == NULL", sock->lastdata == NULL);
  }

#if LWIP_IGMP
  /* drop all possibly joined IGMP memberships */
  lwip_socket_drop_registered_memberships(s);
#endif /* LWIP_IGMP */
	/* 释放从lwip内存堆里分配到空间 */
  err = netconn_delete(sock->conn);
  if (err != ERR_OK) {
    sock_set_errno(sock, err_to_errno(err));
    return -1;
  }
	/* 主要是对socket结构体成员进行反初始化，并对数据空间进行释放，看下述对该函数实现分析 */
  free_socket(sock, is_tcp);
  set_errno(0);
  return 0;
}
```
```c
static void free_socket(struct lwip_sock *sock, int is_tcp){
  void *lastdata;

  lastdata         = sock->lastdata;
  sock->lastdata   = NULL;
  sock->lastoffset = 0;
  sock->err        = 0;

  /* Protect socket array */
  /* 对socket->conn进行置空 */
  SYS_ARCH_SET(sock->conn, NULL);
  /* don't use 'sock' after this line, as another task might have allocated it */

  if (lastdata != NULL) {
    if (is_tcp) {
      pbuf_free((struct pbuf *)lastdata);
    } else {
      netbuf_delete((struct netbuf *)lastdata);
    }
  }
}
```
&emsp;&emsp;上述两个函数分析可知，`close`函数只能使socket->conn为空，并不能使select_waiting为0，所以其实只有`close`函数是不能使socket资源完全释放的。

### 2.3 select函数
&emsp;&emsp;从*select_waiting*名字中能比较容易的猜到，该变量跟select函数肯定是强相关的。全局搜索select_waiting，果然只有select函数有进行写操作。下面分析select函数，该函数较长，做必要的简化。

```c
int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout){
  u32_t waitres = 0;
  int nready;
  fd_set lreadset, lwriteset, lexceptset;
  u32_t msectimeout;
  struct lwip_select_cb select_cb;
  int i;
  int maxfdp2;
#if LWIP_NETCONN_SEM_PER_THREAD
  int waited = 0;
#endif
  /* Go through each socket in each list to count number of sockets which
     currently match */
  /* 
  *扫描所有socket对应的bit，如果有准备好，则直接将对应的bit置上，后面可以看出，该函数简单的赋值后就退出了，
  *不涉及对select_waiting的操作。
   */
  nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);

  /* If we don't have any current events, then suspend if we are supposed to */
  /* 只有没有相应的socket准备好并且没有超时，才回置位select_waiting, 并挂起线程。 */
  if (!nready) {
    if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_select: no timeout, returning 0\n"));
      /* This is OK as the local fdsets are empty and nready is zero,
         or we would have returned earlier. */
      goto return_copy_fdsets;
    }

    /* 省略一堆处理，可以看到只要该socket设置了，读写异常通知，并且socket是存在的，则会将select_wainting增加1 */
    /* Increase select_waiting for each socket we are interested in */
    maxfdp2 = maxfdp1;
    for (i = LWIP_SOCKET_OFFSET; i < maxfdp1; i++) {
      if ((readset && FD_ISSET(i, readset)) ||
          (writeset && FD_ISSET(i, writeset)) ||
          (exceptset && FD_ISSET(i, exceptset))) {
        struct lwip_sock *sock;
        SYS_ARCH_PROTECT(lev);
        sock = tryget_socket(i);
        if (sock != NULL) {
          sock->select_waiting++;
          LWIP_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
        } else {
          /* Not a valid socket */
          nready = -1;
          maxfdp2 = i;
          SYS_ARCH_UNPROTECT(lev);
          break;
        }
        SYS_ARCH_UNPROTECT(lev);
      }
    }

    if (nready >= 0) {
    /* 
    *执行完上述操作，还会再扫描一次是否有socket有事件产生，删除细节。
    *因为上述，如果socket资源过多，会消耗不少资源，再扫描一次可以提高效率。
    */
      /* 休眠指定时间，让出cpu控制权 */
      waitres = sys_arch_sem_wait(SELECT_SEM_PTR(select_cb.sem), msectimeout);
    }
    /* 休眠结束， 将对应socket->select_waiting减1 */
    /* Decrease select_waiting for each socket we are interested in */
    for (i = LWIP_SOCKET_OFFSET; i < maxfdp2; i++) {
      if ((readset && FD_ISSET(i, readset)) ||
          (writeset && FD_ISSET(i, writeset)) ||
          (exceptset && FD_ISSET(i, exceptset))) {
        struct lwip_sock *sock;
        SYS_ARCH_PROTECT(lev);
        sock = tryget_socket(i);
        /* 减1，必须socket是还在的 */
        if (sock != NULL) {
          /* for now, handle select_waiting==0... */
          LWIP_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
          if (sock->select_waiting > 0) {
            sock->select_waiting--;
          }
        } else {
          /* Not a valid socket */
          nready = -1;
        }
        SYS_ARCH_UNPROTECT(lev);
      }
    }
  }
  /* 删除不影响分析代码，感兴趣参考源码。 */
  return nready;
}
```

**<center>这是这一张来自未来的select函数处理流程图</center>**

&emsp;&emsp;参考上述代码分析，特别注意*socket->select_waiting*加1和减1的地方，可以看到，如果socket存在且的确需要监听事件，且并不是进来事件就已经产生或者已经超时，一定会加1；然后线程会有可能会进行休眠；正常情况下，休眠结束后，*socket->select_waiting*减1，离开该函数，*socket->select_waiting*恢复原值。**但是**，如果在线程休眠期间，恰巧在另外一个线程进行了close操作，事件就变味了。

&emsp;&emsp;如果在休眠期间进行了`close(socket)`,则通过`tyr_socket(socket)`获取不到socket结构体，则*socket->select_waiting*不会进行减1，后面执行一系列语句后，退出该函数，*socket->select_waiting*没有恢复原值，且比进来时大1。针对该函数，*socket->select_waiting*加1的次数是*>=*减1的次数，所以如果只要在函数退出时没有恢复原值，则*socket->select_waiting*永远不可能再减为0了，此时socket资源就出现了**假占用**，该socket再也不能被其他人使用了。

# 三、解决方案
&emsp;&emsp;第二章已经对产生的原因进行了分析。解决问题的思路也想一开始提到的有两种，为了不改lwip源码，使用了第二种思路。下面用伪代码给出解决方案。需要使用到两个flag`closing_socket_flag`和·selecting_flag`。
**thread1**
```c
int adaptor_closesocket(int socket){
    while(get_select_processing()){
        sleep(1);
    }
    set_closesocket_processing(true);
    ret = close(socket);
    set_closescoket_processing(false);
}
```

**thread2**
```c
int select_loop(int socket){
    while(get_closesocket_processing()){
        sleep(1);
    }
    set_select_processing(true);
    select_return = select(sockMAX + 1, &read_set, NULL, &exception_set, &timeout);
    set_select_processing(false);
}
```
&emsp;&emsp;上面的解决方案，我认为是最为简单通用的解决方案，当然针对两个flag肯定还是需要加锁的。另外还有一种思路就是使用通知类似于condition的方法。知道了错误原因，解决方法的思路就是做同步。

# 四、写在最后
&emsp;&emsp;LwIP无疑是一个很优秀的轻量版的TCP/IP协议实现了，虽然上面的socket接口都是简化版，当时以为如果功能是支持的，在使用以为可以跟BSD的一样。因为在开发DoIP时是跨平台，上层应用代码是一样的，在windows和linux都是支持的，所以比较简单就初步定位出了问题应该是出在了LwIP协议本身，但是当时由于现象特别奇怪（略过不表），也费了一般周折才最终定位出来。一开始觉得认为这是一个bug，后面跟老虞（技术偶像）深度讨论过，觉得这也不属于LwIP本身的一个bug，感觉更像是feature实现的不够完整，但是light weight也已经足够了。同时在使用LwIP本身也学到了很多技巧，如连接符**##**的使用、在MCU上实现分配空间的解决方案。

![](https://savannah.nongnu.org/images/Savannah.theme/floating.png)




