/* Extracted from anet.c to work properly with Hiredis error reporting.
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "net.h"
#include "sds.h"

/* Forward declaration */
void __redisSetError(redisContext *c, int type, sds err);

static int redisCreateSocket(redisContext *c, int type) {
    int s, on = 1;
    if ((s = socket(type, SOCK_STREAM, 0)) == -1) {
        __redisSetError(c,REDIS_ERR_IO,NULL);
        return REDIS_ERR;
    }
    if (type == AF_INET) {
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
            __redisSetError(c,REDIS_ERR_IO,NULL);
            close(s);
            return REDIS_ERR;
        }
    }
    return s;
}

static int redisSetNonBlock(redisContext *c, int fd) {
    int flags;

    /* Set the socket nonblocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        __redisSetError(c,REDIS_ERR_IO,
            sdscatprintf(sdsempty(), "fcntl(F_GETFL): %s", strerror(errno)));
        close(fd);
        return REDIS_ERR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        __redisSetError(c,REDIS_ERR_IO,
            sdscatprintf(sdsempty(), "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno)));
        close(fd);
        return REDIS_ERR;
    }
    return REDIS_OK;
}

static int redisSetTcpNoDelay(redisContext *c, int fd) {
    int yes = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
        __redisSetError(c,REDIS_ERR_IO,
            sdscatprintf(sdsempty(), "setsockopt(TCP_NODELAY): %s", strerror(errno)));
        return REDIS_ERR;
    }
    return REDIS_OK;
}

int redisContextConnectTcp(redisContext *c, const char *addr, int port) {
    int s;
    int blocking = (c->flags & REDIS_BLOCK);
    struct sockaddr_in sa;

    if ((s = redisCreateSocket(c,AF_INET)) == REDIS_ERR)
        return REDIS_ERR;
    if (!blocking && redisSetNonBlock(c,s) == REDIS_ERR)
        return REDIS_ERR;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_aton(addr, &sa.sin_addr) == 0) {
        struct hostent *he;

        he = gethostbyname(addr);
        if (he == NULL) {
            __redisSetError(c,REDIS_ERR_OTHER,
                sdscatprintf(sdsempty(),"Can't resolve: %s",addr));
            close(s);
            return REDIS_ERR;
        }
        memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
    }

    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        if (errno == EINPROGRESS && !blocking) {
            /* This is ok. */
        } else {
            __redisSetError(c,REDIS_ERR_IO,NULL);
            close(s);
            return REDIS_ERR;
        }
    }

    if (redisSetTcpNoDelay(c,s) != REDIS_OK) {
        close(s);
        return REDIS_ERR;
    }

    c->fd = s;
    c->flags |= REDIS_CONNECTED;
    return REDIS_OK;
}

int redisContextConnectUnix(redisContext *c, const char *path) {
    int s;
    int blocking = (c->flags & REDIS_BLOCK);
    struct sockaddr_un sa;

    if ((s = redisCreateSocket(c,AF_LOCAL)) == REDIS_ERR)
        return REDIS_ERR;
    if (!blocking && redisSetNonBlock(c,s) != REDIS_OK)
        return REDIS_ERR;

    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        if (errno == EINPROGRESS && !blocking) {
            /* This is ok. */
        } else {
            __redisSetError(c,REDIS_ERR_IO,NULL);
            close(s);
            return REDIS_ERR;
        }
    }

    c->fd = s;
    c->flags |= REDIS_CONNECTED;
    return REDIS_OK;
}


int redisContextConnectTcpS3E(redisContext *c, const char *addr, int port) {
	    
    int32 counter = 0;    
    s3eInetAddress s3eaddr;
	uint64 testStartTime;


    memset(&s3eaddr, 0, sizeof(s3eaddr));

	
	c->socket = s3eSocketCreate(S3E_SOCKET_TCP, 0);
    if (c->socket == NULL) return REDIS_ERR;

	c->flags &= ~REDIS_CONNECTED;


    if (s3eInetLookup(addr, &s3eaddr, NULL, NULL) == S3E_RESULT_ERROR) return REDIS_ERR;
    s3eaddr.m_Port = s3eInetHtons(port);


	if (s3eSocketConnect(c->socket, &s3eaddr, &redisContextConnectTcpS3ECB, c) != S3E_RESULT_SUCCESS)
    {
        switch (s3eSocketGetError())
        {
            // These errors are 'OK', because they mean,
            // that a connect is in progress
            case S3E_SOCKET_ERR_INPROGRESS:
            case S3E_SOCKET_ERR_ALREADY:
            case S3E_SOCKET_ERR_WOULDBLOCK:
                break;
            default:
                return REDIS_ERR;
        }

        testStartTime = s3eTimerGetMs();

		while (!s3eDeviceCheckQuitRequest() && s3eTimerGetMs()-testStartTime < 30000)        {
            // Stop waiting since socket is now connected
            if (c->flags & REDIS_CONNECTED)
                break;
			// trying to connect ...
            s3eDeviceYield(30);
        }
    }

	if (c->flags & REDIS_CONNECTED)
    {
		s3eSocketSetOpt(c->socket,S3E_SOCKET_NODELAY,0,0);
		return REDIS_OK;
	}
    else
    {
        s3eSocketClose(c->socket);
        c->socket=NULL;
		return REDIS_ERR;

    }
}


int32 redisContextConnectTcpS3ECB(s3eSocket* g_Sock, void* sys, void* data)
{
    s3eResult res = *(s3eResult*)sys;
	redisContext * c = (redisContext*) data;

    if (res == S3E_RESULT_SUCCESS)
        c->flags |= REDIS_CONNECTED;
    else
        c->flags &= ~REDIS_CONNECTED;

    return 0;
}

