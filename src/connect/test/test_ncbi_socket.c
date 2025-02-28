/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Test suite for "ncbi_socket.[ch]", the portable TCP/IP socket API
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_socket_unix.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if   defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  define X_SLEEP(x)  /*((void) sleep(x))*/
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#  define X_SLEEP(x)  /*((void) Sleep(1000 * x))*/
#else
#  define X_SLEEP(x)  ((void) 0)
#endif /*NCBI_OS*/

#include "test_assert.h"  /* This header must go last */

/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_MSWIN)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN!"
#endif /*!NCBI_OS_UNIX && !NCBI_OS_MSWIN*/

#define DEF_PORT      5555
#define DEF_HOST      "localhost"

#define _STR(x)       #x
#define  STR(x)      _STR(x)

#define TEST_BUFSIZE  8192


/* The simplest randezvous (a plain request-reply) test functions
 *      "TEST__client_1(SOCK sock)"
 *      "TEST__server_1(SOCK sock)"
 */

static const char s_C1[] = "C1";
static const char s_S1[] = "S1";

#define N_SUB_BLOB     10
#define SUB_BLOB_SIZE  70000
#define BIG_BLOB_SIZE  (N_SUB_BLOB * SUB_BLOB_SIZE)


static void TEST__client_1(SOCK sock)
{
    EIO_Status status;
    int        x_check;
    size_t     n, n_io_done;
    char       buf[TEST_BUFSIZE];

    CORE_LOG(eLOG_Note, "TEST__client_1(TC1)");

    /* Send a short string */
    SOCK_SetDataLoggingAPI(eOn);
    n = sizeof(s_C1);
    status = SOCK_Write(sock, s_C1, n, &n_io_done, eIO_WritePersist);
    assert(status == eIO_Success  &&  n == n_io_done);

    /* Read the string back (it must be bounced by the server) */
    SOCK_SetDataLoggingAPI(eOff);
    SOCK_SetDataLogging(sock, eOn);
    n = sizeof(s_S1);
    status = SOCK_Read(sock, buf, n, &n_io_done, eIO_ReadPeek);
    if (status == eIO_Closed)
        CORE_LOG(eLOG_Fatal, "TC1::connection closed");
    assert(status == eIO_Success);
    status = SOCK_Read(sock, buf, n, &n_io_done, eIO_ReadPersist);

    assert(status == eIO_Success  &&  n == n_io_done);
    assert(memcmp(buf, s_S1, n) == 0);
    assert(SOCK_Pushback(sock, buf, n) == eIO_Success);
    memset(buf, '\xFF', n);
    status = SOCK_Read(sock, buf, n, &n_io_done, eIO_ReadPlain);
    assert(status == eIO_Success  &&  n == n_io_done);
    assert(SOCK_Status(sock, eIO_Read) == eIO_Success);
    assert(memcmp(buf, s_S1, n) == 0);

    SOCK_SetDataLogging(sock, eDefault);

    /* Send a very big binary blob */
    {{
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);
        if (!blob) {
            CORE_LOG(eLOG_Fatal, "TC1::out of memory");
            assert(0);
            return;
        }

        for (n = 0;  n < BIG_BLOB_SIZE;  ++n)
            blob[n] = (unsigned char) n;
        for (n = 0;  n < N_SUB_BLOB;  ++n) {
            status = SOCK_Write(sock, blob + n * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                                &n_io_done, eIO_WritePersist);
            assert(status == eIO_Success  &&  n_io_done == SUB_BLOB_SIZE);
        }

        free(blob);
    }}

    /* Send a very big binary blob with read on write */
    /* (it must be bounced by the server) */
    {{
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);
        if (!blob) {
            CORE_LOG(eLOG_Fatal, "TC1::out of memory");
            assert(0);
            return;
        }

        SOCK_SetReadOnWrite(sock, eOn);

        for (n = 0;  n < BIG_BLOB_SIZE;  ++n)
            blob[n] = (unsigned char)(BIG_BLOB_SIZE - n);
        for (n = 0;  n < N_SUB_BLOB;  ++n) {
            status = SOCK_Write(sock, blob + n * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                                &n_io_done, eIO_WritePersist);
            assert(status == eIO_Success  &&  n_io_done == SUB_BLOB_SIZE);
        }
        /* Receive back a very big binary blob, and check its contents */
        memset(blob, 0, BIG_BLOB_SIZE);
        for (n = 0;  n < N_SUB_BLOB;  ++n) {
            status = SOCK_Read(sock, blob + n * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                               &n_io_done, eIO_ReadPersist);
            assert(status == eIO_Success  &&  n_io_done == SUB_BLOB_SIZE);
        }
        for (n = 0; n < BIG_BLOB_SIZE; ++n) {
            x_check = blob[n] - (unsigned char)(BIG_BLOB_SIZE - n);
            assert(x_check == 0);
        }

        free(blob);
    }}

    /* Try to read more data (must hit EOF as the peer is shut down) */
    assert(SOCK_Read(sock, buf, 1, &n_io_done, eIO_ReadPeek)
           == eIO_Closed);
    assert(SOCK_Status(sock, eIO_Read) == eIO_Closed);
    assert(SOCK_Read(sock, buf, 1, &n_io_done, eIO_ReadPlain)
           == eIO_Closed);
    assert(SOCK_Status(sock, eIO_Read) == eIO_Closed);

    /* Shutdown on read */
    verify(SOCK_Shutdown(sock, eIO_Read)  == eIO_Success);
    assert(SOCK_Status  (sock, eIO_Write) == eIO_Success);
    assert(SOCK_Status  (sock, eIO_Read)  == eIO_Closed);
    assert(SOCK_Read    (sock, 0,  0,&n_io_done,eIO_ReadPlain) == eIO_Unknown);
    assert(SOCK_Read    (sock, 0,  0,&n_io_done,eIO_ReadPeek)  == eIO_Unknown);
    assert(SOCK_Status  (sock, eIO_Read)  == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write) == eIO_Success);
    assert(SOCK_Read    (sock, buf,1,&n_io_done,eIO_ReadPlain) == eIO_Unknown);
    assert(SOCK_Read    (sock, buf,1,&n_io_done,eIO_ReadPeek)  == eIO_Unknown);
    assert(SOCK_Status  (sock, eIO_Read)  == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write) == eIO_Success);

    /* Shutdown on write */
    verify(SOCK_Shutdown(sock, eIO_Write)          == eIO_Success);
    assert(SOCK_Status  (sock, eIO_Write)          == eIO_Closed);
    assert(SOCK_Write   (sock, 0, 0, &n_io_done, eIO_WritePersist)
                                                   == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write)          == eIO_Closed);
    assert(SOCK_Write   (sock, buf, 1, &n_io_done, eIO_WritePersist)
                                                   == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write)          == eIO_Closed);

    /* Double shutdown should be okay */
    assert(SOCK_Shutdown(sock, eIO_Read)      == eIO_Success);
    assert(SOCK_Shutdown(sock, eIO_ReadWrite) == eIO_Success);
    assert(SOCK_Shutdown(sock, eIO_Write)     == eIO_Success);
    assert(SOCK_Status  (sock, eIO_Read)      == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write)     == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_ReadWrite) == eIO_InvalidArg);
}


static void TEST__server_1(SOCK sock)
{
    EIO_Status status;
    int        x_check;
    size_t     n, n_io_done;
    char       buf[TEST_BUFSIZE];

    CORE_LOG(eLOG_Note, "TEST__server_1(TS1)");

    /* Receive and send back a short string */
    SOCK_SetDataLogging(sock, eOn);
    n = sizeof(s_C1);
    status = SOCK_Read(sock, buf, n, &n_io_done, eIO_ReadPersist);
    assert(status == eIO_Success  &&  n == n_io_done);
    assert(memcmp(buf, s_C1, n) == 0);

    SOCK_SetDataLogging(sock, eDefault);
    SOCK_SetDataLoggingAPI(eOn);
    n = sizeof(s_S1);
    status = SOCK_Write(sock, s_S1, n, &n_io_done, eIO_WritePersist);
    assert(status == eIO_Success  &&  n == n_io_done);
    SOCK_SetDataLoggingAPI(eOff);

    /* Receive a very big binary blob, and check its content */
    {{
#define DO_LOG_SIZE    300
#define DONT_LOG_SIZE  BIG_BLOB_SIZE - DO_LOG_SIZE
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);
        if (!blob) {
            CORE_LOG(eLOG_Fatal, "TS1::out of memory");
            assert(0);
            return;
        }

        status = SOCK_Read(sock,blob,DONT_LOG_SIZE,&n_io_done,eIO_ReadPersist);
        assert(status == eIO_Success  &&  n_io_done == DONT_LOG_SIZE);

        SOCK_SetDataLogging(sock, eOn);
        status = SOCK_Read(sock, blob + DONT_LOG_SIZE, DO_LOG_SIZE,
                           &n_io_done, eIO_ReadPersist);
        assert(status == eIO_Success  &&  n_io_done == DO_LOG_SIZE);
        SOCK_SetDataLogging(sock, eDefault);

        for (n = 0;  n < BIG_BLOB_SIZE;  ++n) {
            x_check = blob[n] - (unsigned char) n;
            assert(x_check == 0);
        }

        free(blob);
    }}

    /* Receive a very big binary blob, and write data back */
    {{
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);
        if (!blob) {
            CORE_LOG(eLOG_Fatal, "TS1::out of memory");
            assert(0);
            return;
        }

        for (n = 0;  n < N_SUB_BLOB;  ++n) {
            /*            X_SLEEP(1);*/
            status = SOCK_Read(sock, blob + n * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                               &n_io_done, eIO_ReadPersist);
            assert(status == eIO_Success  &&  n_io_done == SUB_BLOB_SIZE);
            status = SOCK_Write(sock, blob + n * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                                &n_io_done, eIO_WritePersist);
            assert(status == eIO_Success  &&  n_io_done == SUB_BLOB_SIZE);
        }
        for (n = 0;  n < BIG_BLOB_SIZE;  ++n) {
            x_check = blob[n] - (unsigned char)(BIG_BLOB_SIZE - n);
            assert(x_check == 0);
        }

        free(blob);
    }}

    /* Shutdown on write */
#ifdef NCBI_OS_MSWIN
    verify(SOCK_Shutdown(sock, eIO_ReadWrite) == eIO_Success);
#else
    verify(SOCK_Shutdown(sock, eIO_Write)     == eIO_Success);
#endif
    assert(SOCK_Status  (sock, eIO_Write)     == eIO_Closed);
    assert(SOCK_Write   (sock, 0, 0, &n_io_done, eIO_WritePersist)
                                              == eIO_Closed);
    assert(SOCK_Status  (sock, eIO_Write)     == eIO_Closed);
#ifdef NCBI_OS_MSWIN
    assert(SOCK_Status  (sock, eIO_Read)      == eIO_Closed);
#else
    assert(SOCK_Status  (sock, eIO_Read)      == eIO_Success);
#endif
    verify(SOCK_Close   (sock)                == eIO_Success);
}


/* More complicated randezvous test functions
 *      "TEST__client_2(SOCK sock)"
 *      "TEST__server_2(SOCK sock)"
 */

static void s_DoubleTimeout(STimeout *to)
{
    if (to->sec | to->usec) {
        to->sec  = (2 * to->usec) / 1000000 + 2 * to->sec;
        to->usec = (2 * to->usec) % 1000000;
    } else
        to->usec = 1;
}


static void TEST__client_2(SOCK sock)
{
#define W_FIELD      10
#define N_FIELD      1000
#define N_REPEAT     10
#define N_RECONNECT  3
    static char buf[W_FIELD * N_FIELD + 1];
    size_t      n_io, n_io_done, i;
    int         x_check;
    EIO_Status  status;

    CORE_LOGF(eLOG_Note,
              ("TEST__client_2(TC2) @:%hu",
               SOCK_GetLocalPort(sock, eNH_HostByteOrder)));

    /* fill out a buffer to send to server */
    memset(buf, 0, sizeof(buf));
    for (i = 0;  i < N_FIELD;  ++i)
        sprintf(buf + i * W_FIELD, "%*lu", W_FIELD, (unsigned long) i);

    /* send the buffer to server, then get it back */
    for (i = 0;  i < N_REPEAT;  ++i) {
        int/*bool*/ w_timeout_on = (int)(i & 1);  /* whether to start from...*/
        int/*bool*/ r_timeout_on = (int)(i & 1);  /* ...zero or inf. timeout */
        char        buf1[sizeof(buf)];
        STimeout    w_to, r_to;
        char*       x_buf;

        /* set timeout */
        w_to.sec  = 0;
        w_to.usec = 0;
        status = SOCK_SetTimeout(sock, eIO_Write, w_timeout_on ? &w_to : 0);
        assert(status == eIO_Success);

        /* reconnect */
        if ((i % N_RECONNECT) == 0) {
            size_t j = i / N_RECONNECT;
            do {
                SOCK_SetDataLogging(sock, eOn);
                status = SOCK_Reconnect(sock, 0, 0, 0);
                SOCK_SetDataLogging(sock, eDefault);
                CORE_LOGF(eLOG_Note,
                          ("TC2::reconnect @:%hu: i=%lu, j=%lu, status=%s",
                           SOCK_GetLocalPort(sock, eNH_HostByteOrder),
                           (unsigned long) i, (unsigned long) j,
                           IO_StatusStr(status)));
                assert(status == eIO_Success);
                assert(SOCK_Status(sock, eIO_Read)  == eIO_Success);
                assert(SOCK_Status(sock, eIO_Write) == eIO_Success);

                /* give it a break to let server reset the listening socket */
                X_SLEEP(1);
            } while ( j-- );
        }

        /* send */
        x_buf = buf;
        n_io = sizeof(buf);
        do {
            X_SLEEP(1);
            status = SOCK_Write(sock, x_buf, n_io, &n_io_done,
                                eIO_WritePersist);
            if (status == eIO_Closed)
                CORE_LOG(eLOG_Fatal, "TC2::write: connection closed");

            CORE_LOGF(eLOG_Note,
                      ("TC2::write:"
                       " [%lu] status=%7s: n_io=%5lu, n_io_done=%5lu,"
                       " timeout(%d): %5u.%06us",
                       (unsigned long) i, IO_StatusStr(status),
                       (unsigned long) n_io, (unsigned long) n_io_done,
                       w_timeout_on, w_to.sec, w_to.usec));
            if ( w_timeout_on ) {
                const STimeout* x_to;
                assert(status == eIO_Success  ||  status == eIO_Timeout);
                verify((x_to = SOCK_GetTimeout(sock, eIO_Write)) != 0);
                assert(w_to.sec == x_to->sec  &&  w_to.usec == x_to->usec);
            } else
                assert(status == eIO_Success  &&  n_io_done == n_io);
            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eIO_Timeout)
                s_DoubleTimeout(&w_to);
            status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
            assert(status == eIO_Success);
            w_timeout_on = 1/*true*/;
        } while ( n_io );

        /* get back the just sent data */
        r_to.sec  = 0;
        r_to.usec = 0;
        status = SOCK_SetTimeout(sock, eIO_Read, r_timeout_on ? &r_to : 0);
        assert(status == eIO_Success);

        x_buf = buf1;
        n_io = sizeof(buf1);
        do {
            if (i & 1) {
                /* peek a little piece twice and compare */
                char   xx_buf1[128], xx_buf2[128];
                size_t xx_io_done1, xx_io_done2;
                if (SOCK_Read(sock, xx_buf1, sizeof(xx_buf1), &xx_io_done1,
                              eIO_ReadPeek) == eIO_Success  &&
                    SOCK_Read(sock, xx_buf2, xx_io_done1, &xx_io_done2,
                              eIO_ReadPeek) == eIO_Success) {
                    assert(xx_io_done1 >= xx_io_done2);
                    assert(memcmp(xx_buf1, xx_buf2, xx_io_done2) == 0);
                }
            }
            status = SOCK_Read(sock, x_buf, n_io, &n_io_done, eIO_ReadPlain);
            if (status == eIO_Closed) {
                assert(SOCK_Status(sock, eIO_Read) == eIO_Closed);
                CORE_LOG(eLOG_Fatal, "TC2::read: connection closed");
            }
            CORE_LOGF(eLOG_Note,
                      ("TC2::read: "
                       " [%lu] status=%7s: n_io=%5lu, n_io_done=%5lu,"
                       " timeout(%d): %5u.%06us",
                       (unsigned long) i, IO_StatusStr(status),
                       (unsigned long) n_io, (unsigned long) n_io_done,
                       r_timeout_on, r_to.sec, r_to.usec));
            if ( r_timeout_on ) {
                const STimeout* x_to;
                assert(status == eIO_Success  ||  status == eIO_Timeout);
                verify((x_to = SOCK_GetTimeout(sock, eIO_Read)) != 0);
                assert(r_to.sec == x_to->sec  &&  r_to.usec == x_to->usec);
            } else
                assert(status == eIO_Success  &&  n_io_done > 0);

            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eIO_Timeout)
                s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eIO_Read, &r_to);
            assert(status == eIO_Success);
            r_timeout_on = 1/*true*/;
        } while ( n_io );

        x_check = memcmp(buf, buf1, sizeof(buf));
        assert(x_check == 0);
    }
}


static void TEST__server_2(SOCK sock, LSOCK lsock)
{
    EIO_Status status;
    char       buf[TEST_BUFSIZE];
    STimeout   r_to, w_to, rc_to;
    size_t     n, n_io, n_io_done;

    CORE_LOG(eLOG_Note, "TEST__server_2(TS2)");

    r_to.sec   = 0;
    r_to.usec  = 0;
    w_to = r_to;

    rc_to.sec  = (unsigned int) DEF_CONN_TIMEOUT;
    rc_to.usec = 123456;

  reconnect: /* reconnection loop */
    SOCK_SetDataLogging(sock, eOn);

    status = SOCK_SetTimeout(sock, eIO_Read,  &r_to);
    assert(status == eIO_Success);
    status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
    assert(status == eIO_Success);

    for (n = 0;  ;  ++n) {
        char* x_buf;

        /* read data from socket */
        n_io = sizeof(buf);
        status = SOCK_Read(sock, buf, n_io, &n_io_done, eIO_ReadPlain);
        switch ( status ) {
        case eIO_Success:
            CORE_LOGF(eLOG_Note,
                      ("TS2::read: "
                       " [%lu] status=%7s: n_io=%5lu, n_io_done=%5lu",
                       (unsigned long) n, IO_StatusStr(status),
                       (unsigned long) n_io, (unsigned long) n_io_done));
            assert(n_io_done > 0);
            break;

        case eIO_Closed:
            CORE_LOGF(eLOG_Note,
                      ("TS2::read: "
                       " [%lu] connection closed", (unsigned long) n));
            assert(SOCK_Status(sock, eIO_Read) == eIO_Closed);
            /* close connection */
            status = SOCK_Close(sock);
            assert(status == eIO_Success  ||  status == eIO_Closed);
            /* reconnect */
            if ( !lsock )
                return;

            CORE_LOG(eLOG_Note, "TS2::reconnect");
            if ((status = LSOCK_Accept(lsock, &rc_to, &sock)) != eIO_Success)
                return;
            assert(SOCK_Status(sock, eIO_Read) == eIO_Success);
            goto reconnect;

        case eIO_Timeout:
            CORE_LOGF(eLOG_Note,
                      ("TS2::read: "
                       " [%lu] timeout expired: %5u.%06us",
                       (unsigned long) n, r_to.sec, r_to.usec));
            assert(n_io_done == 0);
            s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eIO_Read, &r_to);
            assert(status == eIO_Success);
            assert(SOCK_Status(sock, eIO_Read) == eIO_Timeout);
            break;

        default:
            CORE_LOGF(eLOG_Fatal,
                      ("TS2::read: "
                       " [%lu] status=%d", (unsigned long) n, status));
            break/*NOTREACHED*/;
        } /* switch */

        /* write(just the same) data back to the client */
        x_buf = buf;
        n_io  = n_io_done;
        while ( n_io ) {
            status = SOCK_Write(sock, x_buf, n_io, &n_io_done,
                                eIO_WritePersist);
            switch ( status ) {
            case eIO_Success:
                CORE_LOGF(eLOG_Note,
                          ("TS2::write:"
                           " [%lu] status=%7s: n_io=%5lu, n_io_done=%5lu",
                           (unsigned long) n, IO_StatusStr(status),
                           (unsigned long) n_io, (unsigned long) n_io_done));
                assert(n_io_done == n_io);
                break;
            case eIO_Closed:
                CORE_LOGF(eLOG_Fatal,
                          ("TS2::write:"
                           " [%lu] connection closed", (unsigned long) n));
                return/*NOTREACHED*/;
            case eIO_Timeout:
                CORE_LOGF(eLOG_Note,
                          ("TS2::write:"
                           " [%lu] timeout expired: %5u.%06us",
                           (unsigned long) n, w_to.sec, w_to.usec));
                assert(n_io_done >= 0);
                s_DoubleTimeout(&w_to);
                status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
                assert(status == eIO_Success);
                break;
            default:
                CORE_LOGF(eLOG_Fatal,
                          ("TS2::write:"
                           " [%lu] status=%d", (unsigned long) n, status));
                break/*NOTREACHED*/;
            }

            n_io  -= n_io_done;
            x_buf += n_io_done;
        }
    }
}


/* Skeletons for the socket i/o test:
 *   TEST__client(...)
 *   TEST__server(...)
 *   establish and close connection;  call test i/o functions like
 *     TEST__[client|server]_[1|2|...] (...)
 */
static void TEST__client(const char*     server_host,
                         unsigned short  server_port,
                         const STimeout* timeout)
{
    SOCK       sock;
    EIO_Status status;
    char       tmo[80];

    if ( timeout )
        sprintf(tmo, "%u.%06u", timeout->sec, timeout->usec);
    else
        strcpy(tmo, "INFINITE");
    CORE_LOGF(eLOG_Note,
              ("TEST__client(host = \"%s\", port = %hu, timeout = %s)",
               server_host, server_port, tmo));

    /* Connect to server */
    status = SOCK_Create(server_host, server_port, timeout, &sock);
    assert(status == eIO_Success);
    verify(SOCK_SetTimeout(sock, eIO_ReadWrite, timeout) == eIO_Success);
    verify(SOCK_SetTimeout(sock, eIO_Close,     timeout) == eIO_Success);

    /* Test the simplest randezvous(plain request-reply)
     * The two peer functions are:
     *      "TEST__[client|server]_1(SOCK sock)"
     */
    TEST__client_1(sock);

    /* Test a more complex case
     * The two peer functions are:
     *      "TEST__[client|server]_2(SOCK sock)"
     */
    TEST__client_2(sock);

    /* Close connection and exit */
    status = SOCK_Close(sock);
    assert(status == eIO_Success  ||  status == eIO_Closed);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
}


static void TEST__server(const char* sport)
{
    int            n;
    unsigned short nport;
    LSOCK          lsock;
    EIO_Status     status;
    char           full[128];

    /* Create listening socket */
    if (sscanf(sport, "%hu%n", &nport, &n) < 1  ||  sport[n]) {
        nport = 0;
        n = 0;
    }
    status = LSOCK_CreateEx(nport, N_RECONNECT * 10, &lsock, fSOCK_LogOn);
    if (status == eIO_Success  &&  !nport  &&  sport[n]) {
        FILE* fp;
        nport = LSOCK_GetPort(lsock, eNH_HostByteOrder);
        if (nport  &&  (fp = fopen(sport, "w")) != 0) {
            if (fprintf(fp, "%hu", nport) < 1  ||  fflush(fp) != 0)
                status = eIO_Unknown;
            fclose(fp);
        } else
            status = eIO_Unknown;
    }

    if (!LSOCK_GetListeningAddressString(lsock, full + 1, sizeof(full) - 2))
        sprintf(full, "port = %hu", nport);
    else
        full[0] = '"', strcat(full, "\"");
    CORE_LOGF(eLOG_Note, ("TEST__server(%s)", full));
    assert(status == eIO_Success);

    /* Accept connections from clients and run test sessions */
    for (;;) {
        char addr[sizeof(full)];
        char port[10];
        SOCK sock;

        status = LSOCK_Accept(lsock, NULL, &sock);
        assert(status == eIO_Success);

        assert(SOCK_GetPeerAddressString  (sock, full,sizeof(full)));
        assert(SOCK_GetPeerAddressStringEx(sock, addr,sizeof(addr),eSAF_IP));
        assert(SOCK_GetPeerAddressStringEx(sock, port,sizeof(port),eSAF_Port));
        if (*full == '[') {
            size_t len = strlen(addr);
            memmove(addr + 1, addr, len++);
            *addr = '[';
            addr[len++] = ']';
            addr[len] = '\0';
        }
        assert(strcmp(full, strcat(strcat(addr, ":"), port)) == 0);

        /* Test the simplest randezvous(plain request-reply)
         * The two peer functions are:
         *      "TEST__[client|server]_1(SOCK sock)"
         */
        TEST__server_1(sock);

        status = LSOCK_Accept(lsock, NULL, &sock);
        assert(status == eIO_Success);

        /* Test a more complex case
         * The two peer functions are:
         *      "TEST__[client|server]_2(SOCK sock)"
         */
        TEST__server_2(sock, lsock);
    }
}


/* Fake (printout only) MT critical section callback and data
 */
static char TEST_LockUserData[] = "TEST_LockUserData";


#if defined(__cplusplus)
extern "C" {
    static int/*bool*/ TEST_LockHandler(void* user_data, EMT_Lock how);
    static void        TEST_LockCleanup(void* user_data);
}
#endif /* __cplusplus */


static int/*bool*/ TEST_LockHandler(void* user_data, EMT_Lock how)
{
    const char* what_str = 0;
    switch ( how ) {
    case eMT_Lock:
        what_str = "eMT_Lock";
        break;
    case eMT_LockRead:
        what_str = "eMT_LockRead";
        break;
    case eMT_Unlock:
        what_str = "eMT_Unlock";
        break;
    case eMT_TryLock:
        what_str = "eMT_TryLock";
        break;
    case eMT_TryLockRead:
        what_str = "eMT_TryLockRead";
        break;
    }
    fflush(stdout);
    fprintf(stderr,
            "TEST_LockHandler(\"%s\", %s)\n",
            user_data ? (char*) user_data : "<NULL>", what_str);
    fflush(stderr);
    return 1/*true*/;
}


static void TEST_LockCleanup(void* user_data)
{
    fflush(stdout);
    fprintf(stderr,
            "TEST_LockCleanup(\"%s\")\n",
            user_data ? (char*) user_data : "<NULL>");
    fflush(stderr);
}


static const char* s_ntoa(unsigned int host)
{
    static char buf[256];
    if (SOCK_ntoa(host, buf, sizeof(buf)) != 0) {
        buf[0] = '?';
        buf[1] = '\0';
    }
    return buf;
}


static unsigned int TEST_gethostbyname(const char* name)
{
    char         buf[CONN_HOST_LEN + 1];
    unsigned int host;

    CORE_LOG(eLOG_Note, "------------");

    host = SOCK_gethostbyname(name);
    CORE_LOGF(eLOG_Note,
              ("SOCK_gethostbyname(\"%s\"):  0x%08X [%s]",
               name, (unsigned int) SOCK_NetToHostLong(host), s_ntoa(host)));
    if ( host ) {
        name = SOCK_gethostbyaddr(host, buf, sizeof(buf));
        if ( name ) {
            assert(name == buf);
            assert(0 < strlen(buf)  &&  strlen(buf) < sizeof(buf));
            CORE_LOGF(eLOG_Note,
                      ("SOCK_gethostbyaddr(0x%08X [%s]):  \"%s\"",
                       (unsigned int) SOCK_NetToHostLong(host), s_ntoa(host),
                       name));
        } else {
            CORE_LOGF(eLOG_Note,
                      ("SOCK_gethostbyaddr(0x%08X [%s]):  <not found>",
                       (unsigned int) SOCK_NetToHostLong(host), s_ntoa(host)));
            /* NB: Unknown IPs always get converted to bare notations */
            assert(0);
            return 0;
        }
    }
    return host;
}


static int/*bool*/ TEST_gethostbyaddr(unsigned int host)
{
    const char*  name;
    char         buf[CONN_HOST_LEN + 1];

    CORE_LOG(eLOG_Note, "------------");

    name = SOCK_gethostbyaddr(host, buf, sizeof(buf));
    if ( name ) {
        assert(name == buf);
        assert(0 < strlen(buf)  &&  strlen(buf) < sizeof(buf));
        CORE_LOGF(eLOG_Note,
                  ("SOCK_gethostbyaddr(0x%08X [%s]):  \"%s\"",
                   (unsigned int) SOCK_NetToHostLong(host),
                   s_ntoa(host), name));
    } else {
        CORE_LOGF(eLOG_Note,
                  ("SOCK_gethostbyaddr(0x%08X [%s]):  <not found>",
                   (unsigned int) SOCK_NetToHostLong(host), s_ntoa(host)));
        /* NB: Unknown IPs always get converted to bare notations */
        assert(0);
        return 0/*failure*/;
    }

    host = SOCK_gethostbyname(name);
    CORE_LOGF(eLOG_Note,
              ("SOCK_gethostbyname(\"%s\"):  0x%08X [%s]",
               name, (unsigned int) SOCK_NetToHostLong(host), s_ntoa(host)));

    return 1/*success*/;
}


static TNCBI_IPv6Addr* TEST_gethostbyname6(TNCBI_IPv6Addr* addr, const char* host)
{
    char        buf[CONN_HOST_LEN + 1];
    char        addrstr[80];
    int/*bool*/ fail;

    assert(addr);
    CORE_LOG(eLOG_Note, "------------");

    fail = !SOCK_gethostbyname6(addr, host);
    NcbiAddrToString(addrstr, sizeof(addrstr), addr);
    CORE_LOGF(eLOG_Note,
        ("SOCK_gethostbyname6(\"%s\"):  [%s]",
            host, addrstr));
    if ( !fail ) {
        const char* name = SOCK_gethostbyaddr6(addr, buf, sizeof(buf));
        if ( name ) {
            assert(name == buf);
            assert(0 < strlen(buf)  &&  strlen(buf) < sizeof(buf));
            CORE_LOGF(eLOG_Note,
                ("SOCK_gethostbyaddr6(%s):  \"%s\"",
                    addrstr, name));
        } else {
            CORE_LOGF(eLOG_Note,
                ("SOCK_gethostbyaddr6(%s):  <not found>",
                    addrstr));
            /* NB: Unknown IPs always get converted to bare notations */
            assert(0);
            addr = 0;
        }
    }

    return addr;
}


static int/*bool*/ TEST_gethostbyaddr6(const TNCBI_IPv6Addr* addr)
{
    const char*    host;
    TNCBI_IPv6Addr temp;
    int/*bool*/    fail;
    char           addrstr[80];
    char           buf[CONN_HOST_LEN + 1];

    CORE_LOG(eLOG_Note, "------------");

    host = SOCK_gethostbyaddr6(addr, buf, sizeof(buf));
    NcbiAddrToString(addrstr, sizeof(addrstr), addr);
    if ( host ) {
        assert(host == buf);
        assert(0 < strlen(buf)  &&  strlen(buf) < sizeof(buf));
        CORE_LOGF(eLOG_Note,
            ("SOCK_gethostbyaddr6(%s):  \"%s\"",
                addrstr, host));
    } else {
        CORE_LOGF(eLOG_Note,
            ("SOCK_gethostbyaddr6(%s):  <not found>",
                addrstr));
        /* NB: Unknown IPs always get converted to bare notations */
        assert(0);
        return 0/*failure*/;
    }

    fail = !SOCK_gethostbyname6(&temp, host);
    NcbiAddrToString(addrstr, sizeof(addrstr), &temp);
    CORE_LOGF(eLOG_Note,
        ("SOCK_gethostbyname6(\"%s\"):  [%s]",
            host, addrstr));

    return !fail;
}


/* Try SOCK_htonl(), SOCK_gethostbyname() and SOCK_gethostbyaddr()
 */
static void TEST_gethostby(void)
{
    const char* p;

    CORE_LOG(eLOG_Note, "===============================");

    assert( SOCK_HostToNetLong(0) == 0 );
    assert( SOCK_HostToNetLong(0xFFFFFFFF) == 0xFFFFFFFF );

    assert( !TEST_gethostbyname("  ") );
    assert( !TEST_gethostbyname("a1....b1") );
    assert( !TEST_gethostbyname("boo.foo.bar.doo") );

    (void) TEST_gethostbyname("localhost");
    (void) TEST_gethostbyname("ncbi.nlm.nih.gov");

    (void) TEST_gethostbyname("127.0.0.1");
    (void) TEST_gethostbyname("130.14.25.1");

    (void) TEST_gethostbyaddr(0);
    (void) TEST_gethostbyaddr(SOCK_gethostbyname("127.0.0.1"));
    (void) TEST_gethostbyaddr(SOCK_gethostbyname("130.14.25.1"));
    (void) TEST_gethostbyaddr(SOCK_gethostbyname("234.234.234.234"));
    (void) TEST_gethostbyaddr(0xFFFFFFFF);

    TNCBI_IPv6Addr addr, www;

    /*SOCK_SetIPv6API(eDefault);*/
    (void) TEST_gethostbyname6(&addr, "www.ncbi.nlm.nih.gov");
 
    SOCK_SetIPv6API(eOn);
    (void) TEST_gethostbyname6(&addr, "www.ncbi.nlm.nih.gov");

    SOCK_SetIPv6API(eOff);
    (void) TEST_gethostbyname6(&addr, "www.ncbi.nlm.nih.gov");

    SOCK_SetIPv6API(eDefault);
    (void) TEST_gethostbyname6(&addr, "www.ncbi.nlm.nih.gov");

    assert((p = NcbiIPToAddr(&addr, "2607:f220:41e:4290::110", 0))  &&  !*p);
    assert(!NcbiIsEmptyIPv6(&addr)  &&  !NcbiIsIPv4(&addr));
    assert((p = NcbiIPToAddr(&www, "130.14.29.110", 0))  &&  !*p);
    assert(!NcbiIsEmptyIPv6(&www)  &&  NcbiIsIPv4(&www));

    /*SOCK_SetIPv6API(eDefault);*/
    (void) TEST_gethostbyaddr6(&addr);
    (void) TEST_gethostbyaddr6(&www);

    SOCK_SetIPv6API(eOn);
    (void) TEST_gethostbyaddr6(&addr);
    (void) TEST_gethostbyaddr6(&www);

    SOCK_SetIPv6API(eOff);
    (void) TEST_gethostbyaddr6(&addr);
    (void) TEST_gethostbyaddr6(&www);

    SOCK_SetIPv6API(eDefault);
    (void) TEST_gethostbyaddr6(&addr);
    (void) TEST_gethostbyaddr6(&www);

    /*SOCK_SetIPv6API(eDefault);*/
    (void) TEST_gethostbyname6(&addr, "localhost");

    SOCK_SetIPv6API(eOn);
    (void) TEST_gethostbyname6(&addr, "localhost");

    SOCK_SetIPv6API(eOff);
    (void) TEST_gethostbyname6(&addr, "localhost");

    SOCK_SetIPv6API(eDefault);
    (void) TEST_gethostbyname6(&addr, "localhost");

    CORE_LOG(eLOG_Note, "===============================");
}


static int/*bool*/ TEST_isip(const char* ip)
{
    int retval = SOCK_isip(ip);

    CORE_LOG(eLOG_Note, "------------");
    
    CORE_LOGF(eLOG_Note,
              ("SOCK_isip(\"%s\"):  %s", ip, retval ? "True" : "False"));

    return retval;
}


static int/*bool*/ TEST_isipEx(const char* ip)
{
    int retval = SOCK_isipEx(ip, 1/*fullquad*/);

    CORE_LOG(eLOG_Note, "------------");

    CORE_LOGF(eLOG_Note,
              ("SOCK_isipEx(\"%s\", 1):  %s", ip, retval ? "True" : "False"));

    return retval;
}


/* Try SOCK_isip()
 */
static void TEST_SOCK_isip(void)
{
    CORE_LOG(eLOG_Note, "===============================");

    assert(TEST_isip("0")  &&  TEST_isip("0.0.0.0"));
    assert(TEST_gethostbyname("0") == 0);
    assert(TEST_isip("1"));
    assert(TEST_gethostbyname("1") == SOCK_HostToNetLong(1));
    assert(TEST_isip("0x7F000002"));
    assert(TEST_gethostbyname("0x7F000002") ==
           TEST_gethostbyname("127.0.0.2"));

    assert(TEST_isip("127.234"));
    assert(TEST_gethostbyname("127.234") ==
           TEST_gethostbyname("127.0.0.234"));
    assert(TEST_isip("127.16777215"));
    assert(TEST_gethostbyname("127.16777215") ==
           TEST_gethostbyname("127.255.255.255"));

    assert(TEST_isip("127.234.0x345"));
    assert(TEST_gethostbyname("127.234.3.69") ==
           TEST_gethostbyname("127.234.3.69"));
    assert(TEST_isip("127.234.0xFFFF"));
    assert(TEST_gethostbyname("127.234.0xFFFF") ==
           TEST_gethostbyname("127.234.255.255"));
    assert(TEST_isip("127.234.0xDEAD"));
    assert(TEST_gethostbyname("127.234.0xDEAD") ==
           TEST_gethostbyname("127.234.222.173")); 

    assert(TEST_isip("127.012344321"));
    assert(TEST_gethostbyname("127.012344321") ==
           TEST_gethostbyname("127.41.200.209"));
    assert(TEST_isip("127.077777777"));
    assert(TEST_gethostbyname("127.077777777") ==
           TEST_gethostbyname("127.255.255.255"));

    assert(TEST_isip("0.0321.0xAB.123"));
    assert(TEST_gethostbyname("0.0321.0xAB.123"));
    assert(!TEST_isipEx("0.0321.0xAB.123"));
    assert(TEST_isip("255.255.255.255"));

    assert(!TEST_isip("a"));
    assert(!TEST_isip("-1"));
    assert(!TEST_isip("1.2.3a"));
    assert(!TEST_isip("1.0xDEATH"));
    assert(!TEST_isip("1.2.3.256"));
    assert(!TEST_isip("1.2.0200000"));
    assert(!TEST_isip("1.1.1.1."));
    assert(!TEST_isip("1.1.-1.1"));
    assert(!TEST_isip("1.0x100.1.1"));
    assert(!TEST_isip("1.0100000000"));
    assert(!TEST_isip("0x100000000"));

    CORE_LOG(eLOG_Note, "===============================");
}


#ifdef NCBI_OS_LINUX
static void TEST_OnTopSock(void)
{
    LSOCK pipe;
    SOCK  server, client, ontop[2];
    const char* unique = tmpnam(0);
    CORE_LOGF(eLOG_Note, ("SOCK_OnTop(\"%s\")", unique));
    SOCK_SetDataLoggingAPI(eOn);
    verify(LSOCK_CreateUNIX(unique, 64, &pipe, fSOCK_LogDefault)
           == eIO_Success);
    verify(SOCK_CreateUNIX(unique, 0, &client, 0, 0, fSOCK_LogDefault)
           == eIO_Success);
    verify(LSOCK_Accept(pipe, 0, &server)
           == eIO_Success);
    verify(SOCK_CreateOnTop(server, 0, &ontop[0])
           == eIO_Success);
    verify(SOCK_CreateOnTop(client, 0, &ontop[1])
           == eIO_Success);
    verify(SOCK_Destroy(client)
           == eIO_Closed);
    verify(SOCK_Destroy(server)
           == eIO_Closed);
    verify(SOCK_Destroy(ontop[0])
           == eIO_Success);
    verify(SOCK_Destroy(ontop[1])
           == eIO_Success);
    verify(LSOCK_Close(pipe)
           == eIO_Success);
    remove(unique);
}
#endif /*NCBI_OS_LINUX*/


/* Main function
 * Parse command-line options, initialize and cleanup API internals;
 * run client or server test
 */
int main(int argc, const char* argv[])
{
    /* Setup log stream */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    /* Parse cmd.-line args and decide whether it's a client or a server
     */
    switch ( argc ) {
    case 1:
        /*** Try to set various fake MT safety locks ***/
        CORE_SetLOCK( MT_LOCK_Create(0,
                                     TEST_LockHandler, TEST_LockCleanup) );
        CORE_SetLOCK(0);
        CORE_SetLOCK(0);
        CORE_SetLOCK( MT_LOCK_Create(&TEST_LockUserData,
                                     TEST_LockHandler, TEST_LockCleanup) );

        SOCK_SetDataLoggingAPI(eOn);
        verify(SOCK_InitializeAPI() == eIO_Success);
        SOCK_SetDataLoggingAPI(eOff);

        {{
            char local_host[64];
            verify(SOCK_gethostname(local_host, sizeof(local_host)) == 0);
            CORE_LOGF(eLOG_Note,
                      ("Running NCBISOCK test on host \"%s\"", local_host));
        }}

        TEST_gethostby();

        TEST_SOCK_isip();

#ifdef NCBI_OS_LINUX
        TEST_OnTopSock();
#endif/*NCBI_OS_LINUX*/

        verify(SOCK_ShutdownAPI() == eIO_Success);

        CORE_SetLOCK(0);
        break;

    case 2: {
        /*** SERVER ***/
        TEST__server(*argv[1] ? argv[1] : STR(DEF_PORT));
        verify(SOCK_ShutdownAPI() == eIO_Success);
        CORE_SetLOG(0);
        return 0;
    }

    case 3: case 4: {
        /*** CLIENT ***/
        const char*    host;
        unsigned short port;
        STimeout*      tmo;
        STimeout     x_tmo;

        /* host */
        host = *argv[1] ? argv[1] : DEF_HOST;

        /* port */
        if (!*argv[2])
            port = DEF_PORT;
        else if (sscanf(argv[2], "%hu", &port) != 1)
            break;

        /* timeout */
        if (argc == 4) {
            double v;
            char*  e = (char*) argv[3];
            if (!*e  ||  (v = NCBI_simple_atof(e, &e)) < 0.0  ||  errno  || *e)
                break;
            x_tmo.sec  = (unsigned int)  v;
            x_tmo.usec = (unsigned int)((v - x_tmo.sec) * 1000000.0);
            tmo        = &x_tmo;
        } else
            tmo = 0/*infinite*/;

        TEST__client(host, port, tmo);
        verify(SOCK_ShutdownAPI() == eIO_Success);
        CORE_SetLOG(0);
        return 0;
    }
    } /* switch */

    /* USAGE */
    fprintf(stderr,
            "\nClient/Server USAGE:\n"
            "Client: %s <host> <port> [timeout]\n"
            "Server: %s <port>\n\n",
            argv[0], argv[0]);
    CORE_SetLOG(0);
    return argc == 1 ? 0 : 1;
}
