#ifndef CONNECTION_SUBSYS_HPP
#define CONNECTION_SUBSYS_HPP

#include <sys/ioctl.h>
#include <linux/sockios.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <ifaddrs.h>

#include <cstring>
#include <algorithm>
#include <memory>
#include <iostream>

class Connection {
public:
  bool isServer;
  std::string serverIP;
  int bufferSize;
  int serverPort;
  int receiveTimeout;

  Connection ()
    : bufferSize (0),
      receiveTimeout (1)
  {
  }

  Connection (const Connection &s)
    : isServer (s.isServer),
      bufferSize (s.bufferSize),
      serverIP (s.serverIP),
      serverPort (s.serverPort),
      receiveTimeout (s.receiveTimeout)
  {
  }

  Connection & operator= (const Connection &s)
  {
    if (&s != this)
      {
	Connection tmp(s);
	swap (tmp);
      }
    return *this;
  }

  virtual void swap (Connection &s)
  {
    std::swap (isServer, s.isServer);
    std::swap (bufferSize, s.bufferSize);
    std::swap (serverPort, s.serverPort);
    std::swap (receiveTimeout, s.receiveTimeout);
    std::swap (serverIP, s.serverIP);
  }

  virtual Connection* clone() const
  {
    return new Connection( *this );
  }

  int Initialize(bool server) {
    isServer = server;
    if (isServer) {
      return this->InitializeServer();
    }
    else {
      return this->InitializeClient();
    }
  }

  virtual void Close() {}

  virtual long Send(const char *buffer, long len) {
    return -1;
  }
  virtual long Receive(char *buffer, long len) {
    return -1;
  }

  virtual int InitializeServer() {
    return -1;
  }
  virtual int InitializeClient() {
    return -1;
  }
};

class IPV4_Connection : public Connection {
public:
  int sockfd;
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;

  IPV4_Connection()
    : Connection()
  {
    serverPort = 7777;
    receiveTimeout = 5;
    serverIP = "10.1.1.1";
  }

  IPV4_Connection(const IPV4_Connection &s)
    : Connection(s),
      sockfd(s.sockfd),
      local_addr(s.local_addr),
      remote_addr(s.remote_addr)
  {
  }

  IPV4_Connection & operator= (const IPV4_Connection &s)
  {
    if (&s != this)
      {
	IPV4_Connection tmp (s);
	swap (tmp);
      }
    return *this;
  }

  virtual IPV4_Connection* clone() const
  {
    return new IPV4_Connection( *this );
  }

  virtual void swap (IPV4_Connection &s)
  {
    std::swap (sockfd, s.sockfd);
    std::swap (local_addr, s.local_addr);
    std::swap (remote_addr, s.remote_addr);
  }

  virtual void Close()
  {
    close(sockfd);
  }

  ~IPV4_Connection()
  {
    Close();
  }

  virtual long Send(const char *buffer, long len) {
    long bytes;
    if ((bytes = sendto(sockfd, buffer, len, 0, (struct sockaddr *) &remote_addr, sizeof(remote_addr))) == -1 ) {
      int errsv = errno;
    }
    int size;
    int error = ioctl(sockfd, SIOCOUTQ, &size);
    if ( size > bufferSize )
      bufferSize = size;
    return bytes;
  }

  virtual long Receive(char *buffer, long len) {
    socklen_t remote_addr_len = sizeof(remote_addr);
    long bytes;
    if ((bytes = recvfrom(sockfd, buffer, len,0,(struct sockaddr *)&remote_addr, &remote_addr_len)) == -1) {
      int errsv = errno;
    }
    return bytes;
  }

  virtual int InitializeServer() {
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) <0) {
      int errsv = errno;
      //fprintf(stderr, "DGRAM : %d", errno);
      std::cerr << "DGRAM : " << errno << std::endl;
      return -1;
    }

    int optval = 1;
    int retval = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval))!=0) {
      //fprintf(stderr, "SO_REUSEADDR : %d", errno);
      std::cerr << "SO_REUSEADDR : " << errno << std::endl;
      return -1;
    }

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(serverPort);

    if ( (retval = inet_pton(AF_INET, serverIP.c_str(), (void *)&(local_addr.sin_addr.s_addr))) !=1) {
      //fprintf(stderr, "inet_pton : %d", errno);
      std::cerr << "inet_pton : " << errno << std::endl;
      return -1;
    }

    if (bind(sockfd,(struct sockaddr *)&local_addr, sizeof(local_addr))<0) {
      close(sockfd);
      //fprintf(stderr, "bind : %d", errno);
      std::cerr << "bind : " << errno << std::endl;
      return -1;
    }

    struct timeval tv;
    tv.tv_sec = receiveTimeout;
    tv.tv_usec = 0;
    if ( (retval=setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv))) < 0) {
      close(sockfd);
      //fprintf(stderr, "timeout : %d", errno);
      std::cerr << "timeout : " << errno << std::endl;
      return -1;
    }

    return 0;
  }

  virtual int InitializeClient() {
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) <0) {
      int errsv = errno;
      //fprintf(stderr, "DGRAM : %d", errno);
      std::cerr << "DGRAM : " << errno << std::endl;
      return -1;
    }

    int optval = 1;
    int retval = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval))!=0) {
      int errsv = errno;
      //fprintf(stderr, "SO_REUSEADDR : %d", errno);
      std::cerr << "SO_REUSEADDR : " << errno << std::endl;
      return -1;
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(serverPort);

    if ( (retval = inet_pton(AF_INET, serverIP.c_str(), (void *)&(remote_addr.sin_addr.s_addr))) !=1) {
      //fprintf(stderr, "inet_pton : %d", errno);
      std::cerr << "inet_pton : " << errno << std::endl;
      return -1;
    }

    struct timeval tv;
    tv.tv_sec = receiveTimeout;
    tv.tv_usec = 0;
    if ( (retval=setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv))) < 0) {
      close(sockfd);
      //fprintf(stderr, "timeout : %d", errno);
      std::cerr << "timeout : " << errno << std::endl;
      return -1;
    }
    return 0;
  }

  void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

  u_short get_in_port(struct sockaddr *sa)
  {
    if (sa->sa_family == AF_INET) {
      return ((struct sockaddr_in*)sa)->sin_port;
    }

    return ((struct sockaddr_in6*)sa)->sin6_port;
  }

};
#endif
