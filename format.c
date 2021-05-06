static int alloc_socket(struct netconn *newconn, int accepted) {
    int i;
    SYS_ARCH_DECL_PROTECT(lev);         /* allocate a new socket identifier */
    for (i = 0; i < NUM_SOCKETS; ++i) { /* Protect socket array */
        SYS_ARCH_PROTECT(lev);
        if (!sockets[i].conn && (sockets[i].select_waiting == 0)) {
            sockets[i].conn =
                newconn; /* The socket is not yet known to anyone, so no need to
                            protect after having marked it as used. */
            SYS_ARCH_UNPROTECT(lev);
            sockets[i].lastdata = NULL;
            sockets[i].lastoffset = 0;
            sockets[i].rcvevent = 0;
            /* TCP sendbuf is empty, but the socket is not yet writable
                  until connected * (unless it has been created by
                  accept()). */
            sockets[i].sendevent =
                (NETCONNTYPE_GROUP(newconn->type) == NETCONN_TCP
                     ? (accepted != 0)
                     : 1);
            sockets[i].errevent = 0;
            sockets[i].err = 0;
            return i + LWIP_SOCKET_OFFSET;
        }
        SYS_ARCH_UNPROTECT(lev);
    }
    return -1;
}