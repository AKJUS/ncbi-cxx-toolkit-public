#ifndef APP__PUBSEQ_GATEWAY__CLIENT__PERFORMANCE_HPP
#define APP__PUBSEQ_GATEWAY__CLIENT__PERFORMANCE_HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <array>
#include <chrono>
#include <sstream>
#include <thread>

#include <corelib/ncbi_param.hpp>

BEGIN_NCBI_SCOPE

struct SMetricType
{
    enum EType : size_t
    {
        // External metrics (collected by SMetrics)
        eStart,
        eSubmit,
        eReply,
        eDone,
        eSize,
        eError = eSize,

        // Internal metrics (must correspond to values of SDebugPrintout::EType)
        eSend = 1000,
        eReceive,
        eClose,
        eRetry,
        eFail,

        // Should always be the last
        eLastType
    };

    static const char* Name(EType t);
};

struct SMetrics : string, private SMetricType
{
    using time_point = chrono::steady_clock::time_point;
    using duration = chrono::duration<double, milli>;

    SMetrics() : string(to_string(++CurrentRequestId)) {}

    void Set(EType t)
    {
        _ASSERT(t < eSize);
        m_Data[t] = chrono::steady_clock::now();
    }

    void SetSuccess() { m_Success = true; }
    void NewItem() { ++m_Items; }

    static bool GetSuccess(const string& rest)
    {
        return (rest.length() > 4) && (rest.compare(rest.length() - 4, 4, "true") == 0);
    }

private:
    duration::rep Get(EType t) const { return duration(m_Data[t].time_since_epoch()).count(); }

    array<time_point, eSize> m_Data;
    bool m_Success = false;
    size_t m_Items = 0;

    static size_t CurrentRequestId;

    friend ostream& operator<<(ostream& os, const SMetrics& metrics)
    {
        os << fixed << boolalpha;

        for (auto t : { eStart, eSubmit, eReply, eDone}) {
            os << string(metrics) << '\t' <<
                metrics.Get(t) << '\t' << t << '\t' << this_thread::get_id();
           
            if (t == eDone) {
                os << "\titems=" << metrics.m_Items << "\tsuccess=" << metrics.m_Success;
            }

            os << '\n';
        }

        return os.flush();
    }
};

struct SMessage
{
    double milliseconds;
    SMetricType::EType type = SMetricType::eError;
    string thread_id;
    string rest;

    bool operator<(const SMessage& rhs)
    {
        return milliseconds < rhs.milliseconds;
    }

    template <SMetricType::EType type>
    static bool IsSameType(const SMessage& message) { return type == message.type; }

    friend istream& operator>>(istream& is, SMessage& message)
    {
        size_t type;
        is >> message.milliseconds >> type >> message.thread_id;

        if (!is) return is;

        switch (type) {
            case SMetricType::eStart:
            case SMetricType::eSubmit:
            case SMetricType::eReply:
            case SMetricType::eDone:
            case SMetricType::eSend:
            case SMetricType::eReceive:
            case SMetricType::eClose:
                message.type = static_cast<SMetricType::EType>(type);
                break;

            default:
                _TROUBLE;
        }

        // Read the rest of the line if the converion above has failed
        is.clear();
        return getline(is, message.rest);
    }

    friend ostream& operator<<(ostream& os, const SMessage& message)
    {
        return os << message.milliseconds << '\t' <<
            SMetricType::Name(message.type) << '\t' <<
            message.thread_id <<
            message.rest;
    }
};

struct SPoint
{
    enum EIndex { eFirst, eLast };

    SMetricType::EType type;
    EIndex index;

    SPoint(SMetricType::EType t, EIndex i) : type(t), index(i) {}
};

struct SRule
{
    string name;
    SPoint start;
    SPoint stop;

    SRule(string n, SPoint a, SPoint b) : name(n), start(a), stop(b) {}

    using TRules = vector<SRule>;
    static const TRules Rules;
};

struct SComplexMetrics
{
    SComplexMetrics(size_t request, bool success) : m_Request(request), m_Success(success), m_Data(SRule::Rules.size()) {}

    void Add(double milliseconds, SMetricType::EType type);
    double Get(size_t i) const { return m_Data[i].second - m_Data[i].first; }

    struct SHeader {};

private:
    void Set(const SPoint& point, SMetricType::EType type, double milliseconds, double& value);

    const size_t m_Request;
    const bool m_Success;
    vector<pair<double, double>> m_Data;

    friend ostream& operator<<(ostream& os, SHeader header);
    friend ostream& operator<<(ostream& os, const SComplexMetrics& complex_metrics);
};

struct SPercentiles
{
    SPercentiles() { m_Data.resize(SRule::Rules.size()); }

    void Add(const SComplexMetrics& complex_metrics);

private:
    static const vector<pair<size_t, string>> PercentileTypes;

    vector<set<double>> m_Data;

    friend ostream& operator<<(ostream& os, SPercentiles& percentiles);
};

template <class TStream>
struct SIoRedirector : TStream
{
    template <class... TArgs>
    SIoRedirector(ios& io, TArgs&&... args) :
        TStream(forward<TArgs>(args)...),
        m_Io(io),
        m_Buf(m_Io.rdbuf())
    {
        m_Io.rdbuf(TStream::rdbuf());
    }

    void Reset() { m_Io.rdbuf(m_Buf); TStream::seekg(0); }

private:
    ios& m_Io;
    streambuf* m_Buf;
};

struct SPostProcessing : SIoRedirector<stringstream>
{
    SPostProcessing(bool raw_metrics);
    ~SPostProcessing();

private:
    const bool m_RawMetrics;
};


// These CParam declarations are copied from psg_client_transport.hpp and must be kept in sync.
// We did not want to expose them, so they were not moved to a publicly available header.

NCBI_PARAM_DECL(unsigned, PSG, num_io);
typedef NCBI_PARAM_TYPE(PSG, num_io) TPSG_NumIo;

enum class EPSG_UseCache { eDefault, eNo, eYes };
NCBI_PARAM_ENUM_DECL(EPSG_UseCache, PSG, use_cache);
typedef NCBI_PARAM_TYPE(PSG, use_cache) TPSG_UseCache;

enum class EPSG_PsgClientMode { eOff, eInteractive, ePerformance, eIo };
NCBI_PARAM_ENUM_DECL(EPSG_PsgClientMode, PSG, internal_psg_client_mode);
typedef NCBI_PARAM_TYPE(PSG, internal_psg_client_mode) TPSG_PsgClientMode;

NCBI_PARAM_DECL(unsigned, PSG, requests_per_io);
typedef NCBI_PARAM_TYPE(PSG, requests_per_io) TPSG_RequestsPerIo;

NCBI_PARAM_DECL(bool, PSG, delayed_completion);
typedef NCBI_PARAM_TYPE(PSG, delayed_completion) TPSG_DelayedCompletion;

NCBI_PARAM_DECL(unsigned, PSG, max_concurrent_streams);
typedef NCBI_PARAM_TYPE(PSG, max_concurrent_streams) TPSG_MaxConcurrentStreams;

NCBI_PARAM_DECL(unsigned, PSG, request_timeout);
typedef NCBI_PARAM_TYPE(PSG, request_timeout) TPSG_RequestTimeout;

END_NCBI_SCOPE

#endif
