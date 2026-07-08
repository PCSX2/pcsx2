/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lib_pcap_socket_h
#define lib_pcap_socket_h

/*
 * Some minor differences between sockets on various platforms.
 * We include whatever sockets are needed for Internet-protocol
 * socket access on UN*X and Windows.
 */
#ifdef _WIN32
  /* Need windef.h for defines used in winsock2.h under MingW32 */
  #ifdef __MINGW32__
    #include <windef.h>
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>

  /*
   * Winsock doesn't have this UN*X type; it's used in the UN*X
   * sockets API.
   *
   * XXX - do we need to worry about UN*Xes so old that *they*
   * don't have it, either?
   */
  typedef int socklen_t;

  /*
   * Winsock doesn't have this POSIX type; it's used for the
   * tv_usec value of struct timeval.
   */
  typedef long suseconds_t;
#else /* _WIN32 */
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>		/* for struct addrinfo/getaddrinfo() */
  #include <netinet/in.h>	/* for sockaddr_in, in BSD at least */
  #include <arpa/inet.h>

  /*!
   * \brief In Winsock, a socket handle is of type SOCKET; in UN*X, it's
   * a file descriptor, and therefore a signed integer.
   * We define SOCKET to be a signed integer on UN*X, so that it can
   * be used on both platforms.
   */
  #ifndef SOCKET
    #define SOCKET int
  #endif

  /*!
   * \brief In Winsock, the error return if socket() fails is INVALID_SOCKET;
   * in UN*X, it's -1.
   * We define INVALID_SOCKET to be -1 on UN*X, so that it can be used on
   * both platforms.
   */
  #ifndef INVALID_SOCKET
    #define INVALID_SOCKET -1
  #endif
#endif /* _WIN32 */

#endif /* lib_pcap_socket_h */
