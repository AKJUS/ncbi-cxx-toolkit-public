/*  $Id$
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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/impl/ncbi_uv_nghttp2.hpp>
#include <connect/ncbi_socket.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/request_ctx.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_UV_WRITE_TRACE(message)        _TRACE(message)
#define NCBI_UV_TCP_TRACE(message)          _TRACE(message)
#define NCBI_NGHTTP2_SESSION_TRACE(message) _TRACE(message)

SUv_Write::SUv_Write(void* user_data, size_t buf_size) :
    m_UserData(user_data),
    m_BufSize(buf_size)
{
    NewBuffer();
    NCBI_UV_WRITE_TRACE(this << " created");
}

int SUv_Write::Write(uv_stream_t* handle, uv_write_cb cb)
{
    _ASSERT(m_CurrentBuffer);
    auto& request     = m_CurrentBuffer->request;
    auto& data        = m_CurrentBuffer->data;
    auto& in_progress = m_CurrentBuffer->in_progress;

    _ASSERT(!in_progress);

    if (data.empty()) {
        NCBI_UV_WRITE_TRACE(this << " empty write");
        return 0;
    }

    uv_buf_t buf;
    buf.base = data.data();
    buf.len = static_cast<decltype(buf.len)>(data.size());

    auto try_rv = uv_try_write(handle, &buf, 1);

    // If immediately sent everything
    if (try_rv == static_cast<int>(data.size())) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-wrote: " << try_rv);
        data.clear();
        return 0;

    // If sent partially
    } else if (try_rv > 0) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-wrote partially: " << try_rv);
        _ASSERT(try_rv < static_cast<int>(data.size()));
        buf.base += try_rv;
        buf.len -= try_rv;

    // If unexpected error
    } else if (try_rv != UV_EAGAIN) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-write failed: " << uv_strerror(try_rv));
        return try_rv;
    }

    auto rv = uv_write(&request, handle, &buf, 1, cb);

    if (rv < 0) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " pre-write failed");
        return rv;
    }

    NCBI_UV_WRITE_TRACE(this << '/' << &request << " writing: " << data.size());
    in_progress = true;

    // Looking for unused buffer
    for (auto& buffer : m_Buffers) {
        if (!buffer.in_progress) {
            _ASSERT(buffer.data.empty());

            NCBI_UV_WRITE_TRACE(this << '/' << &buffer.request << " switching to");
            m_CurrentBuffer = &buffer;
            return 0;
        }
    }

    // Need more buffers
    NewBuffer();
    return 0;
}

void SUv_Write::OnWrite(uv_write_t* req)
{
    for (auto& buffer : m_Buffers) {
        if (&buffer.request == req) {
            _ASSERT(buffer.data.size());
            _ASSERT(buffer.in_progress);

            NCBI_UV_WRITE_TRACE(this << '/' << req << " wrote");
            buffer.data.clear();
            buffer.in_progress = false;
            return;
        }
    }

    _TROUBLE;
}

void SUv_Write::Reset()
{
    NCBI_UV_WRITE_TRACE(this << " reset");

    for (auto& buffer : m_Buffers) {
        buffer.data.clear();
        buffer.in_progress = false;
    }
}

void SUv_Write::NewBuffer()
{
    m_Buffers.emplace_front();
    m_CurrentBuffer = &m_Buffers.front();

    NCBI_UV_WRITE_TRACE(this << '/' << &m_CurrentBuffer->request << " new buffer");
    m_CurrentBuffer->request.data = m_UserData;
    m_CurrentBuffer->data.reserve(m_BufSize);
}


SUv_Connect::SUv_Connect(void* user_data, const SSocketAddress& address)
{
    m_Request.data = user_data;

    m_Address.sin_family = AF_INET;
    m_Address.sin_addr.s_addr = address.host;
    m_Address.sin_port = CSocketAPI::HostToNetShort(address.port);
#ifdef HAVE_SIN_LEN
    m_Address.sin_len = sizeof(m_Address);
#endif
}

int SUv_Connect::operator()(uv_tcp_t* handle, uv_connect_cb cb)
{
    return uv_tcp_connect(&m_Request, handle, reinterpret_cast<sockaddr*>(&m_Address), cb);
}


SUv_Tcp::SUv_Tcp(uv_loop_t *l, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size,
        TConnectCb connect_cb, TReadCb rcb, TWriteCb write_cb) :
    SUv_Handle<uv_tcp_t>(s_OnClose),
    m_Loop(l),
    m_Connect(this, address),
    m_Write(this, wr_buf_size),
    m_ConnectCb(connect_cb),
    m_ReadCb(rcb),
    m_WriteCb(write_cb)
{
    data = this;
    m_ReadBuffer.reserve(rd_buf_size);

    NCBI_UV_TCP_TRACE(this << " created");
}

int SUv_Tcp::Write()
{
    if (m_State == eClosed) {
        auto rv = uv_tcp_init(m_Loop, this);

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << " init failed: " << uv_strerror(rv));
            return rv;
        }

        rv = m_Connect(this, s_OnConnect);

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << " pre-connect failed: " << uv_strerror(rv));
            Close();
            return rv;
        }

        NCBI_UV_TCP_TRACE(this << " connecting");
        m_State = eConnecting;
    }

    if (m_State == eConnected) {
        auto rv = m_Write.Write((uv_stream_t*)this, s_OnWrite);

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << "  pre-write failed: " << uv_strerror(rv));
            Close();
            return rv;
        }

        NCBI_UV_TCP_TRACE(this << " writing");
    }

    return 0;
}

void SUv_Tcp::Close()
{
    if (m_State == eConnected) {
        auto rv = uv_read_stop(reinterpret_cast<uv_stream_t*>(this));

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << " read stop failed: " << uv_strerror(rv));
        } else {
            NCBI_UV_TCP_TRACE(this << " read stopped");
        }
    }

    m_Write.Reset();

    if ((m_State != eClosing) && (m_State != eClosed)) {
        NCBI_UV_TCP_TRACE(this << " closing");
        m_State = eClosing;
        SUv_Handle<uv_tcp_t>::Close();
    } else {
        NCBI_UV_TCP_TRACE(this << " already closing/closed");
    }
}

void SUv_Tcp::OnConnect(uv_connect_t*, int status)
{
    if (status >= 0) {
        status = uv_tcp_nodelay(this, 1);

        if (status >= 0) {
            status = uv_read_start((uv_stream_t*)this, s_OnAlloc, s_OnRead);

            if (status >= 0) {
                NCBI_UV_TCP_TRACE(this << " connected");
                m_State = eConnected;
                m_ConnectCb(status);
                return;
            } else {
                NCBI_UV_TCP_TRACE(this << " read start failed: " << uv_strerror(status));
            }
        } else {
            NCBI_UV_TCP_TRACE(this << " nodelay failed: " << uv_strerror(status));
        }
    } else {
        NCBI_UV_TCP_TRACE(this << " connect failed: " << uv_strerror(status));
    }

    Close();
    m_ConnectCb(status);
}

void SUv_Tcp::OnAlloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
{
    m_ReadBuffer.resize(suggested_size);
    buf->base = m_ReadBuffer.data();
    buf->len = static_cast<decltype(buf->len)>(m_ReadBuffer.size());
}

void SUv_Tcp::OnRead(uv_stream_t*, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0) {
        NCBI_UV_TCP_TRACE(this << " read failed: " << s_LibuvError(nread));
        Close();
    } else {
        NCBI_UV_TCP_TRACE(this << " read: " << nread);
    }

    m_ReadCb(buf->base, nread);
}

void SUv_Tcp::OnWrite(uv_write_t* req, int status)
{
    if (status < 0) {
        NCBI_UV_TCP_TRACE(this << '/' << req << " write failed: " << uv_strerror(status));
        Close();
    } else {
        NCBI_UV_TCP_TRACE(this << '/' << req << " wrote");
        m_Write.OnWrite(req);
    }

    m_WriteCb(status);
}

void SUv_Tcp::OnClose(uv_handle_t*)
{
    NCBI_UV_TCP_TRACE(this << " closed");
    m_State = eClosed;
}

struct SUvNgHttp2_UserAgentImpl : string
{
    SUvNgHttp2_UserAgentImpl();
};

SUvNgHttp2_UserAgentImpl::SUvNgHttp2_UserAgentImpl()
{
    if (auto app = CNcbiApplication::InstanceGuard()) {
        const auto& full_version = app->GetFullVersion();
        const auto& app_version = full_version.GetVersionInfo();
        const auto pkg_version = full_version.GetPackageVersion();

        assign(app->GetProgramDisplayName());
        append(1, '/');

        if (app_version.IsAny() && !pkg_version.IsAny()) {
            append(1, 'p');
            append(pkg_version.Print());
        } else {
            append(app_version.Print());
        }
    } else {
        assign("UNKNOWN/UNKNOWN");
    }

    append(" NcbiCxxToolkit/"
#if defined(NCBI_PRODUCTION_VER)
        "P" NCBI_AS_STRING(NCBI_PRODUCTION_VER)
#elif defined(NCBI_SUBVERSION_REVISION)
        "r" NCBI_AS_STRING(NCBI_SUBVERSION_REVISION)
#if defined(NCBI_DEVELOPMENT_VER)
        ".D" NCBI_AS_STRING(NCBI_DEVELOPMENT_VER)
#elif defined(NCBI_SC_VERSION)
        ".SC" NCBI_AS_STRING(NCBI_SC_VERSION)
#endif
#else
        "UNKNOWN"
#endif
        );
}

string SUvNgHttp2_UserAgent::Init()
{
    return SUvNgHttp2_UserAgentImpl();
}

SNgHttp2_Session::SNgHttp2_Session(void* user_data, uint32_t max_streams,
        nghttp2_on_data_chunk_recv_callback on_data,
        nghttp2_on_stream_close_callback    on_stream_close,
        nghttp2_on_header_callback          on_header,
        nghttp2_error_callback              on_error,
        nghttp2_on_frame_recv_callback      on_frame_recv) :
    m_UserData(user_data),
    m_OnData(on_data),
    m_OnStreamClose(on_stream_close),
    m_OnHeader(on_header),
    m_OnError(on_error),
    m_OnFrameRecv(on_frame_recv),
    m_MaxStreams(max_streams, max_streams)
{
    NCBI_NGHTTP2_SESSION_TRACE(this << " created");
}

int SNgHttp2_Session::Init()
{
    if (m_Session) return 0;

    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, m_OnData);
    nghttp2_session_callbacks_set_on_stream_close_callback(   callbacks, m_OnStreamClose);
    nghttp2_session_callbacks_set_on_header_callback(         callbacks, m_OnHeader);
    nghttp2_session_callbacks_set_error_callback(             callbacks, m_OnError);
    if (m_OnFrameRecv) nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, m_OnFrameRecv);

    nghttp2_session_client_new(&m_Session, callbacks, m_UserData);
    nghttp2_session_callbacks_del(callbacks);

    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, m_MaxStreams.second}
    };

    /* client 24 bytes magic string will be sent by nghttp2 library */
    if (auto rv = nghttp2_submit_settings(m_Session, NGHTTP2_FLAG_NONE, iv, sizeof(iv) / sizeof(iv[0]))) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submit settings failed: " << s_NgHttp2Error(rv));
        return x_DelOnError(rv);
    }

    NCBI_NGHTTP2_SESSION_TRACE(this << " initialized");
    auto max_streams = nghttp2_session_get_remote_settings(m_Session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    m_MaxStreams.first = min(max_streams, m_MaxStreams.second);
    return 0;
}

void SNgHttp2_Session::Del()
{
    if (!m_Session) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " already terminated");
        return;
    }

    auto rv = nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);

    if (rv) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " terminate failed: " << s_NgHttp2Error(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " terminated");
    }

    x_DelOnError(-1);
}

int32_t SNgHttp2_Session::Submit(const nghttp2_nv *nva, size_t nvlen, nghttp2_data_provider* data_prd)
{
    if (auto rv = Init()) return rv;

    auto rv = nghttp2_submit_request(m_Session, nullptr, nva, nvlen, data_prd, nullptr);

    if (rv < 0) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submit failed: " << s_NgHttp2Error(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submitted");
    }

    return x_DelOnError(rv);
}

int SNgHttp2_Session::Resume(int32_t stream_id)
{
    if (auto rv = Init()) return rv;

    auto rv = nghttp2_session_resume_data(m_Session, stream_id);

    if (rv < 0) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " resume failed: " << s_NgHttp2Error(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " resumed");
    }

    return x_DelOnError(rv);
}

ssize_t SNgHttp2_Session::Send(vector<char>& buffer)
{
    if (auto rv = Init()) return rv;

    if (nghttp2_session_want_write(m_Session) == 0) {
        if (nghttp2_session_want_read(m_Session) == 0) {
            NCBI_NGHTTP2_SESSION_TRACE(this << " does not want to write and read");
            return x_DelOnError(-1);
        }

        NCBI_NGHTTP2_SESSION_TRACE(this << " does not want to write");
        return 0;
    }

    ssize_t total = 0;

    for (;;) {
        const uint8_t* data;
        auto rv = nghttp2_session_mem_send(m_Session, &data);

        if (rv > 0) {
            buffer.insert(buffer.end(), data, data + rv);
            total += rv;

        } else if (rv < 0) {
            NCBI_NGHTTP2_SESSION_TRACE(this << " send failed: " << s_NgHttp2Error(rv));
            return x_DelOnError(rv);

        } else {
            NCBI_NGHTTP2_SESSION_TRACE(this << " sended: " << total);
            return total;
        }
    }
}

ssize_t SNgHttp2_Session::Recv(const uint8_t* buffer, size_t size)
{
    if (auto rv = Init()) return rv;

    const size_t total = size;

    for (;;) {
        auto rv = nghttp2_session_mem_recv(m_Session, buffer, size);

        if (rv > 0) {
            size -= rv;

            if (size > 0) continue;
        }

        if (rv < 0) {
            NCBI_NGHTTP2_SESSION_TRACE(this << " receive failed: " << s_NgHttp2Error(rv));
            return x_DelOnError(rv);
        } else {
            NCBI_NGHTTP2_SESSION_TRACE(this << " received: " << total);
            return total;
        }
    }
}

END_NCBI_SCOPE
