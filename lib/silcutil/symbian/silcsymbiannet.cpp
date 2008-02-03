/*

  silcsymbiannet.cpp

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2006 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcruntime.h"
#include "silcsymbiansocketstream.h"

/************************ Static utility functions **************************/

static SilcBool silc_net_set_sockaddr(TInetAddr *addr, const char *ip_addr,
                                      int port)
{
  /* Check for IPv4 and IPv6 addresses */
  if (ip_addr) {
    if (!silc_net_is_ip(ip_addr)) {
      SILC_LOG_ERROR(("%s is not IP address", ip_addr));
      return FALSE;
    }

    if (silc_net_is_ip4(ip_addr)) {
      /* IPv4 address */
      unsigned char buf[4];
      TUint32 a;

      if (!silc_net_addr2bin(ip_addr, buf, sizeof(buf)))
        return FALSE;

      SILC_GET32_MSB(a, buf);
      addr->SetAddress(a);
      addr->SetPort(port);
    } else {
#ifdef HAVE_IPV6
      SILC_LOG_ERROR(("IPv6 not supported"));
      return FALSE;
#else
      SILC_LOG_ERROR(("Operating System does not support IPv6"));
      return FALSE;
#endif
    }
  } else {
    addr->SetAddress(0);
    addr->SetPort(port);
  }

  return TRUE;
}

/****************************** TCP Listener ********************************/

class SilcSymbianTCPListener;

extern "C" {

/* Deliver new stream to upper layer */

static void silc_net_accept_stream(SilcResult status,
				   SilcStream stream, void *context)
{
  SilcNetListener listener = (SilcNetListener)context;

  /* In case of error, the socket has been destroyed already via
     silc_stream_destroy. */
  if (status != SILC_OK)
    return;

  listener->callback(SILC_OK, stream, listener->context);
}

} /* extern "C" */

/* TCP Listener class */

class SilcSymbianTCPListener : public CActive {
public:
  /* Constructor */
  SilcSymbianTCPListener() : CActive(CActive::EPriorityStandard)
  {
    CActiveScheduler::Add(this);
  }

  /* Destructor */
  ~SilcSymbianTCPListener()
  {
    Cancel();
  }

  /* Listen for connection */
  void Listen()
  {
    SILC_LOG_DEBUG(("Listen()"));

    new_conn = new RSocket;
    if (!new_conn)
      return;
    if (new_conn->Open(ss) != KErrNone) {
      Listen();
      return;
    }

    /* Start listenning */
    sock.Accept(*new_conn, iStatus);
    SetActive();
  }

  /* Listener callback */
  virtual void RunL()
  {
    SILC_LOG_DEBUG(("RunL(), iStatus=%d", iStatus));

    if (iStatus != KErrNone) {
      if (new_conn)
	delete new_conn;
      new_conn = NULL;
      Listen();
      return;
    }

    SILC_LOG_DEBUG(("Accept new connection"));

    /* Set socket options */
    new_conn->SetOpt(KSoReuseAddr, KSolInetIp, 1);

    /* Create socket stream */
    silc_socket_tcp_stream_create(
		        (SilcSocket)silc_create_symbian_socket(new_conn, NULL),
			listener->lookup, listener->require_fqdn,
			listener->schedule, silc_net_accept_stream,
			(void *)listener);
    new_conn = NULL;

    /* Continue listenning */
    Listen();
  }

  /* Cancel */
  virtual void DoCancel()
  {
    sock.CancelAll();
    ss.Close();
    if (new_conn)
      delete new_conn;
  }

  RSocket *new_conn;
  RSocket sock;
  RSocketServ ss;
  SilcNetListener listener;
};

extern "C" {

/* Create TCP listener */

SilcNetListener
silc_net_tcp_create_listener(const char **local_ip_addr,
			     SilcUInt32 local_ip_count, int port,
			     SilcBool lookup, SilcBool require_fqdn,
			     SilcSchedule schedule,
			     SilcNetCallback callback, void *context)
{
  SilcNetListener listener = NULL;
  SilcSymbianTCPListener *l = NULL;
  TInetAddr server;
  TInt ret;
  int i;

  SILC_LOG_DEBUG(("Creating TCP listener"));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      goto err;
    }
  }

  if (port < 0 || !callback) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    goto err;
  }

  listener = (SilcNetListener)silc_calloc(1, sizeof(*listener));
  if (!listener) {
    callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
    return NULL;
  }
  listener->schedule = schedule;
  listener->callback = callback;
  listener->context = context;
  listener->require_fqdn = require_fqdn;
  listener->lookup = lookup;

  if (local_ip_count > 0) {
    listener->socks = (SilcSocket *)silc_calloc(local_ip_count,
					        sizeof(*listener->socks));
    if (!listener->socks) {
      callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
      return NULL;
    }
  } else {
    listener->socks = (SilcSocket *)silc_calloc(1, sizeof(*listener->socks));
    if (!listener->socks) {
      callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
      return NULL;
    }

    local_ip_count = 1;
  }

  /* Bind to local addresses */
  for (i = 0; i < local_ip_count; i++) {
    SILC_LOG_DEBUG(("Binding to local address %s",
		    local_ip_addr ? local_ip_addr[i] : "0.0.0.0"));

    l = new SilcSymbianTCPListener;
    if (!l)
      goto err;

    /* Connect to socket server */
    ret = l->ss.Connect();
    if (ret != KErrNone)
      goto err;

#ifdef SILC_THREADS
    /* Make our socket shareable between threads */
    l->ss.ShareAuto();
#endif /* SILC_THREADS */

    /* Set listener address */
    if (!silc_net_set_sockaddr(&server, local_ip_addr[i], port))
      goto err;

    /* Create the socket */
    ret = l->sock.Open(l->ss, KAfInet, KSockStream, KProtocolInetTcp);
    if (ret != KErrNone) {
      SILC_LOG_ERROR(("Cannot create socket, error %d", ret));
      goto err;
    }

    /* Set the socket options */
    ret = l->sock.SetOpt(KSoReuseAddr, KSolInetIp, 1);
    if (ret != KErrNone) {
      SILC_LOG_ERROR(("Cannot set socket options, error %d", ret));
      goto err;
    }

    /* Bind the listener socket */
    ret = l->sock.Bind(server);
    if (ret != KErrNone) {
      SILC_LOG_DEBUG(("Cannot bind socket, error %d", ret));
      goto err;
    }

    /* Specify that we are listenning */
    ret = l->sock.Listen(5);
    if (ret != KErrNone) {
      SILC_LOG_ERROR(("Cannot set socket listenning, error %d", ret));
      goto err;
    }
    l->Listen();

    l->listener = listener;
    listener->socks[i] = (SilcSocket)l;
    listener->socks_count++;
  }

  SILC_LOG_DEBUG(("TCP listener created"));

  return listener;

 err:
  if (l)
    delete l;
  if (callback)
    callback(SILC_ERR, NULL, context);
  if (listener)
    silc_net_close_listener(listener);
  return NULL;
}

/* Create TCP listener, multiple ports */

SilcNetListener
silc_net_tcp_create_listener2(const char *local_ip_addr, int *ports,
			      SilcUInt32 port_count,
			      SilcBool ignore_port_error,
			      SilcBool lookup, SilcBool require_fqdn,
			      SilcSchedule schedule,
			      SilcNetCallback callback, void *context)
{
  SilcNetListener listener = NULL;
  SilcSymbianTCPListener *l = NULL;
  TInetAddr server;
  TInt ret;
  int i;

  SILC_LOG_DEBUG(("Creating TCP listener"));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      goto err;
    }
  }

  if (!callback) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    goto err;
  }

  listener = (SilcNetListener)silc_calloc(1, sizeof(*listener));
  if (!listener) {
    callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
    return NULL;
  }
  listener->schedule = schedule;
  listener->callback = callback;
  listener->context = context;
  listener->require_fqdn = require_fqdn;
  listener->lookup = lookup;

  if (port_count > 0) {
    listener->socks = (SilcSocket *)silc_calloc(port_count,
					        sizeof(*listener->socks));
    if (!listener->socks) {
      callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
      return NULL;
    }
  } else {
    listener->socks = (SilcSocket *)silc_calloc(1, sizeof(*listener->socks));
    if (!listener->socks) {
      callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
      return NULL;
    }

    port_count = 1;
  }

  /* Bind to ports */
  for (i = 0; i < port_count; i++) {
    SILC_LOG_DEBUG(("Binding to local address %s:%d",
		    local_ip_addr ? local_ip_addr : "0.0.0.0",
		    ports ? ports[i] : 0));

    l = new SilcSymbianTCPListener;
    if (!l)
      goto err;

    /* Connect to socket server */
    ret = l->ss.Connect();
    if (ret != KErrNone)
      goto err;

#ifdef SILC_THREADS
    /* Make our socket shareable between threads */
    l->ss.ShareAuto();
#endif /* SILC_THREADS */

    /* Set listener address */
    if (!silc_net_set_sockaddr(&server, local_ip_addr, ports ? ports[i] : 0)) {
      if (ignore_port_error) {
	delete l;
	continue;
      }
      goto err;
    }

    /* Create the socket */
    ret = l->sock.Open(l->ss, KAfInet, KSockStream, KProtocolInetTcp);
    if (ret != KErrNone) {
      if (ignore_port_error) {
	delete l;
	continue;
      }
      SILC_LOG_ERROR(("Cannot create socket, error %d", ret));
      goto err;
    }

    /* Set the socket options */
    ret = l->sock.SetOpt(KSoReuseAddr, KSolInetIp, 1);
    if (ret != KErrNone) {
      if (ignore_port_error) {
	delete l;
	continue;
      }
      SILC_LOG_ERROR(("Cannot set socket options, error %d", ret));
      goto err;
    }

    /* Bind the listener socket */
    ret = l->sock.Bind(server);
    if (ret != KErrNone) {
      if (ignore_port_error) {
	delete l;
	continue;
      }
      SILC_LOG_DEBUG(("Cannot bind socket, error %d", ret));
      goto err;
    }

    /* Specify that we are listenning */
    ret = l->sock.Listen(5);
    if (ret != KErrNone) {
      if (ignore_port_error) {
	delete l;
	continue;
      }
      SILC_LOG_ERROR(("Cannot set socket listenning, error %d", ret));
      goto err;
    }
    l->Listen();

    l->listener = listener;
    listener->socks[i] = (SilcSocket)l;
    listener->socks_count++;
  }

  if (ignore_port_error && !listener->socks_count) {
    l = NULL;
    goto err;
  }

  SILC_LOG_DEBUG(("TCP listener created"));

  return listener;

 err:
  if (l)
    delete l;
  if (callback)
    callback(SILC_ERR, NULL, context);
  if (listener)
    silc_net_close_listener(listener);
  return NULL;
}

/* Close network listener */

void silc_net_close_listener(SilcNetListener listener)
{
  int i;

  SILC_LOG_DEBUG(("Closing network listener"));

  if (!listener)
    return;

  for (i = 0; i < listener->socks_count; i++) {
    SilcSymbianTCPListener *l = (SilcSymbianTCPListener *)listener->socks[i];
    l->sock.CancelAll();
    l->sock.Close();
    l->ss.Close();
    if (l->new_conn)
      delete l->new_conn;
    delete l;
  }

  silc_free(listener->socks);
  silc_free(listener);
}


/**************************** TCP/IP connecting *****************************/

static void silc_net_connect_stream(SilcResult status,
				    SilcStream stream, void *context);

} /* extern "C" */

/* TCP connecting class */

class SilcSymbianTCPConnect : public CActive {
public:
  /* Constructor */
  SilcSymbianTCPConnect() : CActive(CActive::EPriorityStandard)
  {
    CActiveScheduler::Add(this);
  }

  /* Destructor */
  ~SilcSymbianTCPConnect()
  {
    silc_free(remote);
    if (op)
      silc_async_free(op);
    Cancel();
  }

  /* Connect to remote host */
  void Connect(TSockAddr &addr)
  {
    SILC_LOG_DEBUG(("Connect()"));
    sock->Connect(addr, iStatus);
    SetActive();
  }

  /* Connection callback */
  virtual void RunL()
  {
    SILC_LOG_DEBUG(("RunL(), iStatus=%d", iStatus));

    if (iStatus != KErrNone) {
      if (callback)
	callback(SILC_ERR, NULL, context);
      sock->CancelConnect();
      delete sock;
      ss->Close();
      delete ss;
      sock = NULL;
      ss = NULL;
      delete this;
      return;
    }

    SILC_LOG_DEBUG(("Connected to host %s on %d", remote_ip, port));

    /* Create stream */
    if (callback) {
      silc_socket_tcp_stream_create(
			     (SilcSocket)silc_create_symbian_socket(sock, ss),
			     TRUE, FALSE, schedule, silc_net_connect_stream,
			     (void *)this);
      sock = NULL;
      ss = NULL;
    } else {
      sock->Close();
      delete sock;
      ss->Close();
      delete ss;
      sock = NULL;
      ss = NULL;
      delete this;
    }
  }

  /* Cancel */
  virtual void DoCancel()
  {
    if (ss) {
      ss->Close();
      delete ss;
    }
    if (sock) {
      sock->CancelConnect();
      delete sock;
    }
  }

  RSocket *sock;
  RSocketServ *ss;
  char *remote;
  char remote_ip[64];
  int port;
  SilcAsyncOperation op;
  SilcSchedule schedule;
  SilcNetCallback callback;
  void *context;
};

extern "C" {

/* TCP stream creation callback */

static void silc_net_connect_stream(SilcResult status,
				    SilcStream stream, void *context)
{
  SilcSymbianTCPConnect *conn = (SilcSymbianTCPConnect *)context;

  SILC_LOG_DEBUG(("Socket stream creation status %d", status));

  /* Call connection callback */
  if (conn->callback)
    conn->callback(status, stream, conn->context);
  else if (stream)
    silc_stream_destroy(stream);

  delete conn;
}

/* Connecting abort callback */

static void silc_net_connect_abort(SilcAsyncOperation op, void *context)
{
  SilcSymbianTCPConnect *conn = (SilcSymbianTCPConnect *)context;

  /* Abort */
  conn->callback = NULL;
  conn->op = NULL;
  if (conn->sock)
    conn->sock->CancelConnect();
}

/* Create TCP/IP connection */

SilcAsyncOperation silc_net_tcp_connect(const char *local_ip_addr,
					const char *remote_ip_addr,
					int remote_port,
					SilcSchedule schedule,
					SilcNetCallback callback,
					void *context)
{
  SilcSymbianTCPConnect *conn;
  TInetAddr local, remote;
  SilcResult status;
  TInt ret;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return NULL;
    }
  }

  if (!remote_ip_addr || remote_port < 1 || !callback) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return NULL;
  }

  SILC_LOG_DEBUG(("Creating connection to host %s port %d",
		  remote_ip_addr, remote_port));

  conn = new SilcSymbianTCPConnect;
  if (!conn) {
    callback(SILC_ERR_OUT_OF_MEMORY, NULL, context);
    return NULL;
  }
  conn->schedule = schedule;
  conn->callback = callback;
  conn->context = context;
  conn->port = remote_port;
  conn->remote = strdup(remote_ip_addr);
  if (!conn->remote) {
    status = SILC_ERR_OUT_OF_MEMORY;
    goto err;
  }

  /* Allocate socket */
  conn->sock = new RSocket;
  if (!conn->sock) {
    status = SILC_ERR_OUT_OF_MEMORY;
    goto err;
  }

  /* Allocate socket server */
  conn->ss = new RSocketServ;
  if (!conn->ss) {
    status = SILC_ERR_OUT_OF_MEMORY;
    goto err;
  }

  /* Connect to socket server */
  ret = conn->ss->Connect();
  if (ret != KErrNone) {
    SILC_LOG_ERROR(("Error connecting to socket server, error %d", ret));
    status = SILC_ERR;
    goto err;
  }

#ifdef SILC_THREADS
  /* Make our socket shareable between threads */
  conn->ss->ShareAuto();
#endif /* SILC_THREADS */

  /* Start async operation */
  conn->op = silc_async_alloc(silc_net_connect_abort, NULL, (void *)conn);
  if (!conn->op) {
    status = SILC_ERR_OUT_OF_MEMORY;
    goto err;
  }

  /* Do host lookup */
  if (!silc_net_gethostbyname(remote_ip_addr, FALSE, conn->remote_ip,
			      sizeof(conn->remote_ip))) {
    SILC_LOG_ERROR(("Network (%s) unreachable: could not resolve the "
		    "host", conn->remote));
    status = SILC_ERR_UNREACHABLE;
    goto err;
  }

  /* Create the connection socket */
  ret = conn->sock->Open(*conn->ss, KAfInet, KSockStream, KProtocolInetTcp);
  if (ret != KErrNone) {
    SILC_LOG_ERROR(("Cannot create socket, error %d", ret));
    status = SILC_ERR;
    goto err;
  }

  /* Set appropriate options */
  conn->sock->SetOpt(KSoTcpNoDelay, KSolInetTcp, 1);
  conn->sock->SetOpt(KSoTcpKeepAlive, KSolInetTcp, 1);

  /* Bind to the local address if provided */
  if (local_ip_addr)
    if (silc_net_set_sockaddr(&local, local_ip_addr, 0))
      conn->sock->Bind(local);

  /* Connect to the host */
  if (!silc_net_set_sockaddr(&remote, conn->remote_ip, remote_port)) {
    SILC_LOG_ERROR(("Cannot connect (cannot set address)"));
    status = SILC_ERR;
    goto err;
  }
  conn->Connect(remote);

  SILC_LOG_DEBUG(("Connection operation in progress"));

  return conn->op;

 err:
  if (conn->ss) {
    conn->ss->Close();
    delete conn->ss;
  }
  if (conn->sock)
    delete conn->sock;
  if (conn->remote)
    silc_free(conn->remote);
  if (conn->op)
    silc_async_free(conn->op);
  callback(status, NULL, context);
  delete conn;
  return NULL;
}

/****************************** UDP routines ********************************/

/* Create UDP/IP connection */

SilcStream silc_net_udp_connect(const char *local_ip_addr, int local_port,
				const char *remote_ip_addr, int remote_port,
				SilcSchedule schedule)
{
  SilcSymbianSocket *s;
  SilcStream stream;
  TInetAddr local, remote;
  TRequestStatus status;
  RSocket *sock = NULL;
  RSocketServ *ss = NULL;
  TInt ret;

  SILC_LOG_DEBUG(("Creating UDP stream"));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      goto err;
    }
  }

  SILC_LOG_DEBUG(("Binding to local address %s",
		  local_ip_addr ? local_ip_addr : "0.0.0.0"));

  sock = new RSocket;
  if (!sock)
    goto err;

  ss = new RSocketServ;
  if (!ss)
    goto err;

  /* Open socket server */
  ret = ss->Connect();
  if (ret != KErrNone)
    goto err;

#ifdef SILC_THREADS
  /* Make our socket shareable between threads */
  ss->ShareAuto();
#endif /* SILC_THREADS */

  /* Get local bind address */
  if (!silc_net_set_sockaddr(&local, local_ip_addr, local_port))
    goto err;

  /* Create the socket */
  ret = sock->Open(*ss, KAfInet, KSockDatagram, KProtocolInetUdp);
  if (ret != KErrNone) {
    SILC_LOG_ERROR(("Cannot create socket"));
    goto err;
  }

  /* Set the socket options */
  sock->SetOpt(KSoReuseAddr, KSolInetIp, 1);

  /* Bind the listener socket */
  ret = sock->Bind(local);
  if (ret != KErrNone) {
    SILC_LOG_DEBUG(("Cannot bind socket"));
    goto err;
  }

  /* Set to connected state if remote address is provided. */
  if (remote_ip_addr && remote_port) {
    if (silc_net_set_sockaddr(&remote, remote_ip_addr, remote_port)) {
      sock->Connect(remote, status);
      if (status != KErrNone) {
	SILC_LOG_DEBUG(("Cannot connect UDP stream"));
	goto err;
      }
    }
  }

  /* Encapsulate into socket stream */
  s = silc_create_symbian_socket(sock, ss);
  if (!s)
    goto err;
  stream =
    silc_socket_udp_stream_create((SilcSocket)s, local_ip_addr ?
				  silc_net_is_ip6(local_ip_addr) : FALSE,
				  remote_ip_addr ? TRUE : FALSE, schedule);
  if (!stream)
    goto err;

  SILC_LOG_DEBUG(("UDP stream created, fd=%d", sock));
  return stream;

 err:
  if (sock)
    delete sock;
  if (ss) {
    ss->Close();
    delete ss;
  }
  return NULL;
}

/* Sets socket to non-blocking mode */

int silc_net_set_socket_nonblock(SilcSocket sock)
{
  /* Nothing to do in Symbian where blocking socket mode is asynchronous
     already (ie. non-blocking). */
  return 0;
}

/* Converts the IP number string from numbers-and-dots notation to
   binary form. */

SilcBool silc_net_addr2bin(const char *addr, void *bin, SilcUInt32 bin_len)
{
  int ret = 0;
  struct in_addr tmp;

  ret = inet_aton(addr, &tmp);
  if (bin_len < 4)
    return FALSE;

  memcpy(bin, (unsigned char *)&tmp.s_addr, 4);

  return ret != 0;
}

/* Get remote host and IP from socket */

SilcBool silc_net_check_host_by_sock(SilcSocket sock, char **hostname,
				     char **ip)
{
  SilcSymbianSocket *s = (SilcSymbianSocket *)sock;
  TInetAddr addr;
  char host[256];
  TBuf<64> tmp;

  if (hostname)
    *hostname = NULL;
  *ip = NULL;

  s->sock->RemoteName(addr);
  addr.Output(tmp);

  *ip = (char *)silc_memdup(tmp.Ptr(), tmp.Length());
  if (*ip == NULL)
    return FALSE;

  /* Do reverse lookup if we want hostname too. */
  if (hostname) {
    /* Get host by address */
    if (!silc_net_gethostbyaddr(*ip, host, sizeof(host)))
      return FALSE;

    *hostname = (char *)silc_memdup(host, strlen(host));
    SILC_LOG_DEBUG(("Resolved hostname `%s'", *hostname));

    /* Reverse */
    if (!silc_net_gethostbyname(*hostname, TRUE, host, sizeof(host)))
      return FALSE;

    if (strcmp(*ip, host))
      return FALSE;
  }

  SILC_LOG_DEBUG(("Resolved IP address `%s'", *ip));
  return TRUE;
}

/* Get local host and IP from socket */

SilcBool silc_net_check_local_by_sock(SilcSocket sock, char **hostname,
				      char **ip)
{
  SilcSymbianSocket *s = (SilcSymbianSocket *)sock;
  TInetAddr addr;
  char host[256];
  TBuf<64> tmp;

  if (hostname)
    *hostname = NULL;
  *ip = NULL;

  s->sock->LocalName(addr);
  addr.Output(tmp);

  *ip = (char *)silc_memdup(tmp.Ptr(), tmp.Length());
  if (*ip == NULL)
    return FALSE;

  /* Do reverse lookup if we want hostname too. */
  if (hostname) {
    /* Get host by address */
    if (!silc_net_gethostbyaddr(*ip, host, sizeof(host)))
      return FALSE;

    *hostname = (char *)silc_memdup(host, strlen(host));
    SILC_LOG_DEBUG(("Resolved hostname `%s'", *hostname));

    /* Reverse */
    if (!silc_net_gethostbyname(*hostname, TRUE, host, sizeof(host)))
      return FALSE;

    if (strcmp(*ip, host))
      return FALSE;
  }

  SILC_LOG_DEBUG(("Resolved IP address `%s'", *ip));
  return TRUE;
}

/* Get remote port from socket */

SilcUInt16 silc_net_get_remote_port(SilcSocket sock)
{
  SilcSymbianSocket *s = (SilcSymbianSocket *)sock;
  TInetAddr addr;

  s->sock->RemoteName(addr);
  return (SilcUInt16)addr.Port();
}

/* Get local port from socket */

SilcUInt16 silc_net_get_local_port(SilcSocket sock)
{
  SilcSymbianSocket *s = (SilcSymbianSocket *)sock;
  TInetAddr addr;

  s->sock->LocalName(addr);
  return (SilcUInt16)addr.Port();
}

} /* extern "C" */
