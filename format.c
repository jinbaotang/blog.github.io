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