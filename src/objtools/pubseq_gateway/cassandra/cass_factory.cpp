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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Casssandra connection factory class
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <sstream>
#include <vector>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>

#include <connect/ncbi_service.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/lbsm_resolver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

const string                    kCassConfigSection = "CASSANDRA_DB";

const unsigned int              kCassConnTimeoutDefault = 30000;
const unsigned int              kCassConnTimeoutMax = UINT_MAX;
const unsigned int              kCassQueryTimeoutDefault = 5000;
const unsigned int              kCassQueryTimeoutMax = UINT_MAX;
const loadbalancing_policy_t    kLoadBalancingDefaultPolicy = LB_DCAWARE;
const unsigned int              kNumThreadsIoMin = 1;
const unsigned int              kNumThreadsIoMax = 32;
const unsigned int              kNumThreadsIoDefault = 4;
const unsigned int              kNumConnPerHostMin = 1;
const unsigned int              kNumConnPerHostMax = 8;
const unsigned int              kNumConnPerHostDefault = 2;
const unsigned int              kKeepaliveMax = UINT_MAX;
const unsigned int              kKeepaliveDefault = 0;
const unsigned int              kCassFallbackWrConsistencyMax = UINT_MAX;
const unsigned int              kCassFallbackWrConsistencyDefault = 0;

///   < 0 means not configured. Should not be used here
///   0 means no limit in auto-restart count,
///   1 means no 2nd start -> no re-starts at all
///   n > 1 means n-1 restart allowed
const int                       kMaxRetriesDefault = 1;

const map<string, loadbalancing_policy_t>     kPolicyArgMap = {
    {"", kLoadBalancingDefaultPolicy},
    {"dcaware", LB_DCAWARE},
    {"roundrobin", LB_ROUNDROBIN}
};


CCassConnectionFactory::CCassConnectionFactory() :
    m_CassConnTimeoutMs(kCassConnTimeoutDefault),
    m_CassQueryTimeoutMs(kCassQueryTimeoutDefault),
    m_CassQueryRetryTimeoutMs(0),
    m_MaxRetries(kMaxRetriesDefault),
    m_CassFallbackRdConsistency(false),
    m_CassFallbackWrConsistency(kCassFallbackWrConsistencyDefault),
    m_LoadBalancing(kLoadBalancingDefaultPolicy),
    m_TokenAware(true),
    m_LatencyAware(true),
    m_NumThreadsIo(kNumThreadsIoDefault),
    m_NumConnPerHost(kNumConnPerHostDefault),
    m_Keepalive(kKeepaliveDefault),
    m_LogSeverity(eDiag_Error),
    m_LogEnabled(false)
{}


CCassConnectionFactory::~CCassConnectionFactory()
{
    CCassConnection::UpdateLogging();
}

void CCassConnectionFactory::AppParseArgs(const CArgs & /*args*/)
{
    ProcessParams();
}

void CCassConnectionFactory::ProcessParams(void)
{
    if (!m_PassFile.empty()) {
        filebuf fb;
        if (!fb.open(m_PassFile.c_str(), ios::in | ios::binary)) {
            NCBI_THROW(CCassandraException, eGeneric, " Cannot open file: " + m_PassFile);
        }

        CNcbiIstream is(&fb);
        CNcbiRegistry registry(is, 0);
        fb.close();
        m_CassUserName = registry.GetString(m_PassSection, "user", "");
        m_CassPassword = registry.GetString(m_PassSection, "password", "");
    }

    x_ValidateArgs();
}

void CCassConnectionFactory::LoadConfig(const string &  cfg_name,
                                        const string &  section)
{
    m_Section = section;
    m_CfgName = cfg_name;
    ReloadConfig();
}

void CCassConnectionFactory::LoadConfig(const CNcbiRegistry &  registry,
                                        const string &  section)
{
    m_Section = section;
    m_CfgName = "";
    ReloadConfig(registry);
}

void CCassConnectionFactory::LoadConfig(IRegistry const* registry, string const& section)
{
    m_Section = section;
    m_CfgName = "";
    ReloadConfig(registry);
}

void CCassConnectionFactory::ReloadConfig()
{
    if (m_CfgName.empty()) {
        NCBI_THROW(CCassandraException, eGeneric, "Configuration file is not specified");
    }
    filebuf fb;
    if (!fb.open(m_CfgName.c_str(), ios::in | ios::binary)) {
        NCBI_THROW(CCassandraException, eGeneric, " Cannot open file: " + m_CfgName);
    }
    CNcbiIstream is(&fb);
    CNcbiRegistry registry(is, 0);
    fb.close();
    ReloadConfig(registry);
}

void CCassConnectionFactory::ReloadConfig(const CNcbiRegistry & registry)
{
    ReloadConfig(&registry);
}

void CCassConnectionFactory::ReloadConfig(IRegistry const* registry)
{
    CFastMutexGuard _(m_RunTimeParams);
    if (m_Section.empty()) {
        m_Section = kCassConfigSection;
    }

    if (registry && !registry->Empty()) {
        list<string> section_names;
        registry->EnumerateSections(&section_names);
        auto section_itr = find(cbegin(section_names), cend(section_names), m_Section);
        if (section_itr == cend(section_names)) {
            NCBI_THROW(CCassandraException, eGeneric, "Cassandra configuration section '" + m_Section + "' not found!");
        }
        m_CassConnTimeoutMs = registry->GetInt(m_Section, "ctimeout", kCassConnTimeoutDefault);
        m_CassQueryTimeoutMs = registry->GetInt(m_Section, "qtimeout", kCassQueryTimeoutDefault);
        m_CassQueryRetryTimeoutMs = registry->GetInt(m_Section, "qtimeout_retry", 0);
        m_MaxRetries = registry->GetInt(m_Section, "maxretries", kMaxRetriesDefault);
        m_CassDataNamespace = registry->GetString(m_Section, "namespace", "");
        m_CassFallbackRdConsistency = registry->GetBool(m_Section, "fallbackrdconsistency", false);
        m_CassFallbackWrConsistency = registry->GetInt(
            m_Section, "fallbackwriteconsistency", kCassFallbackWrConsistencyDefault);
        m_LoadBalancingStr = registry->GetString(m_Section, "loadbalancing", "");
        m_TokenAware = registry->GetBool(m_Section, "tokenaware", true);
        m_LatencyAware = registry->GetBool(m_Section, "latencyaware", true);
        m_NumThreadsIo = registry->GetInt(m_Section, "numthreadsio", kNumThreadsIoDefault);
        m_NumConnPerHost = registry->GetInt(m_Section, "numconnperhost", kNumConnPerHostDefault);
        m_Keepalive = registry->GetInt(m_Section, "keepalive", kKeepaliveDefault);
        m_PassFile = registry->GetString(m_Section, "password_file", "");
        m_PassSection = registry->GetString(m_Section, "password_section", "");
        m_CassHosts = registry->GetString(m_Section, "service", "");
        m_CassBlackList = registry->GetString(m_Section, "black_list", "");
        m_LogEnabled = registry->GetBool(m_Section, "log", false);

        ProcessParams();
    }
}

void CCassConnectionFactory::SetServiceName(string const& service)
{
    m_CassHosts = service;
}

void CCassConnectionFactory::SetDataNamespace(string const& data_namespace)
{
    m_CassDataNamespace = data_namespace;
}


void CCassConnectionFactory::GetHostPort(string & cass_hosts, short & cass_port)
{
    string hosts = m_CassHosts;
    if (hosts.empty()) {
        NCBI_THROW(CCassandraException, eGeneric, "Cassandra connection point is not specified");
    }

    bool is_hostlist = (m_CassHosts.find(':') != string::npos)
        || (m_CassHosts.find(' ') != string::npos)
        || (m_CassHosts.find(',') != string::npos);

    if (!is_hostlist) {
        ERR_POST(Info << "Cassandra connection uses service name: '" << hosts << "'");
        hosts = LbsmLookup::s_Resolve(m_CassHosts, ',');
        if (hosts.empty()) {
            NCBI_THROW(CCassandraException, eGeneric, "Failed to resolve service name: '" + m_CassHosts + "'");
        }
        ERR_POST(Info << "Cassandra service name resolved as: '" << hosts << "'");
    } else {
        ERR_POST(Info << "Cassandra connection uses host list: '" << hosts << "'");
    }

    // Here: the 'hosts' variable has a list of host[:port] items came
    //       from a config file or from an LBSM resolver.
    vector<string> items;
    NStr::Split(hosts, ", ", items, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    cass_port = 0;
    for (const auto & item : items) {
        string item_host;
        string item_port_token;
        if (NStr::SplitInTwo(item, ":", item_host, item_port_token)) {
            // Delimiter was found, i.e. there is a port number
            short item_port = NStr::StringToNumeric<short>(item_port_token);
            if (cass_port == 0) {
                cass_port = item_port;
            } else {
                if (item_port != cass_port) {
                    NCBI_THROW(CCassandraException, eGeneric, "Unmatching port numbers found: " +
                        to_string(cass_port) + " and " + to_string(item_port));
                }
            }
        }

        if (!cass_hosts.empty()) {
            cass_hosts += ",";
        }
        cass_hosts += item_host;
    }
    ERR_POST(Info << "Cassandra connection parameters parsed as: hosts - '"
        << cass_hosts << "'; port - " << to_string(cass_port));
}

string CCassConnectionFactory::GetUserName() const
{
    return m_CassUserName;
}

string CCassConnectionFactory::GetPassword() const
{
    return m_CassPassword;
}


shared_ptr<CCassConnection> CCassConnectionFactory::CreateInstance()
{
    shared_ptr<CCassConnection> rv(new CCassConnection());

    rv->SetLoadBalancing(m_LoadBalancing);
    rv->SetTokenAware(m_TokenAware);
    rv->SetLatencyAware(m_LatencyAware);
    rv->SetRtLimits(m_NumThreadsIo, m_NumConnPerHost);
    rv->SetKeepAlive(m_Keepalive);

    rv->SetTimeouts(m_CassConnTimeoutMs, m_CassQueryTimeoutMs);
    rv->SetQueryTimeoutRetry(m_CassQueryRetryTimeoutMs);
    rv->SetMaxRetries(m_MaxRetries);
    rv->SetFallBackRdConsistency(m_CassFallbackRdConsistency);
    rv->SetBlackList(m_CassBlackList);

    if (m_CassFallbackWrConsistency != 0) {
        rv->SetFallBackWrConsistency(m_CassFallbackWrConsistency);
    }

    if (m_LogEnabled) {
        rv->SetLogging(m_LogSeverity);
    }
    else {
        rv->DisableLogging();
    }

    string hostlist;
    short port;
    GetHostPort(hostlist, port);
    rv->SetConnectionPoint(hostlist, port);
    rv->SetCredentials(m_CassUserName, m_CassPassword);
    rv->SetKeyspace(m_CassDataNamespace);
    return rv;
}


void CCassConnectionFactory::x_ValidateArgs(void)
{
    if (m_CassConnTimeoutMs > kCassConnTimeoutMax) {
        ERR_POST("The cassandra connection timeout is out of range. Allowed "
                 "range: 0..." << kCassConnTimeoutMax << ". Received: " <<
                 m_CassConnTimeoutMs << ". Reset to default: " <<
                 kCassConnTimeoutDefault);
        m_CassConnTimeoutMs = kCassConnTimeoutDefault;
    }

    if (m_CassQueryTimeoutMs > kCassQueryTimeoutMax) {
        ERR_POST("The cassandra query timeout is out of range. Allowed "
                 "range: 0..." << kCassQueryTimeoutMax << ". Received: " <<
                 m_CassQueryTimeoutMs << ". Reset to "
                 "default: " << kCassQueryTimeoutDefault);
        m_CassQueryTimeoutMs = kCassQueryTimeoutDefault;
    }

    if (m_CassQueryRetryTimeoutMs > kCassQueryTimeoutMax) {
        ERR_POST("The cassandra query retry timeout is out of range. Allowed "
                 "range: 0..." << kCassQueryTimeoutMax << ". Received: " <<
                 m_CassQueryRetryTimeoutMs << ". Reset to "
                 "default: 0");
        m_CassQueryTimeoutMs = 0;
    }

    if (m_MaxRetries < 0) {
        ERR_POST("The max retries value is negative. Reset to " << kMaxRetriesDefault);
        m_MaxRetries = kMaxRetriesDefault;
    }

    string      lowercase_policy = NStr::ToLower(m_LoadBalancingStr);
    auto        policy_item = kPolicyArgMap.find(lowercase_policy);
    if (policy_item != kPolicyArgMap.end()) {
        m_LoadBalancing = policy_item->second;
    } else {
        string allowed, default_name;
        for (const auto & item : kPolicyArgMap) {
            if (!item.first.empty()) {
                if (!allowed.empty()) {
                    allowed += ", ";
                }
                allowed += item.second;
                if (item.second == kLoadBalancingDefaultPolicy) {
                    default_name = item.first;
                }
            }
        }
        ERR_POST("The load balancing value is not recognized. Allowed values: " <<
                 allowed << ". Received: " << m_LoadBalancingStr << ". Reset to: " <<
                 default_name);
        m_LoadBalancing = LB_DCAWARE;
    }

    if (m_NumThreadsIo < kNumThreadsIoMin ||
        m_NumThreadsIo > kNumThreadsIoMax) {
        ERR_POST("The number of IO threads is out of range. Allowed "
                 "range: " << kNumThreadsIoMin << "..." <<
                 kNumThreadsIoMax << ". Received: " <<
                 m_NumThreadsIo << ". Reset to "
                 "default: " << kNumThreadsIoDefault);
        m_NumThreadsIo = kNumThreadsIoDefault;
    }

    if (m_NumConnPerHost < kNumConnPerHostMin ||
        m_NumConnPerHost > kNumConnPerHostMax) {
        ERR_POST("The number of connections per host is out of range. Allowed "
                 "range: " << kNumConnPerHostMin << "..." <<
                 kNumConnPerHostMax << ". Received: " <<
                 m_NumConnPerHost << ". Reset to "
                 "default: " << kNumConnPerHostDefault);
        m_NumConnPerHost = kNumConnPerHostDefault;
    }

    if (m_Keepalive > kKeepaliveMax) {
        ERR_POST("The TCP keep-alive the initial delay is out of range. Allowed "
                 "range: 0..." << kKeepaliveMax << ". Received: " <<
                 m_Keepalive << ". Reset to "
                 "default: " << kKeepaliveDefault);
        m_Keepalive = kKeepaliveDefault;
    }

    if (m_CassFallbackWrConsistency > kCassFallbackWrConsistencyMax) {
        ERR_POST("The cassandra write quorum is out of range. Allowed "
                 "range: 0..." << kCassFallbackWrConsistencyMax << ". Received: " <<
                 m_CassFallbackWrConsistency << ". Reset to "
                 "default: " << kCassFallbackWrConsistencyDefault);
        m_CassFallbackWrConsistency = kCassFallbackWrConsistencyDefault;
    }
}

END_IDBLOB_SCOPE
