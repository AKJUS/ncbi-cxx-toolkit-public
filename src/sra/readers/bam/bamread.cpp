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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to BAM files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <util/simple_buffer.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <klib/text.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <align/bam.h>
#include <align/align-access.h>

#include <corelib/ncbifile.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <util/sequtil/sequtil_manip.hpp>

#ifdef HAVE_NEW_PILEUP_COLLECTOR
#  include <immintrin.h>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;


DEFINE_BAM_REF_TRAITS(AlignAccessMgr, const);
DEFINE_BAM_REF_TRAITS(AlignAccessDB,  const);
DEFINE_BAM_REF_TRAITS(AlignAccessRefSeqEnumerator, );
DEFINE_BAM_REF_TRAITS(AlignAccessAlignmentEnumerator, );
DEFINE_BAM_REF_TRAITS(BAMFile, const);
DEFINE_BAM_REF_TRAITS(BAMAlignment, const);
SPECIALIZE_BAM_REF_TRAITS(KNSManager, );
DEFINE_BAM_REF_TRAITS(KNSManager, );


CBamException::CBamException(void)
    : m_RC(0)
{
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(0)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             const string& param,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc),
      m_Param(param)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CBamException& other)
    : CException( other),
      m_RC(other.m_RC),
      m_Param(other.m_Param)
{
    x_Assign(other);
}


CBamException::~CBamException(void) throw()
{
}


const CException* CBamException::x_Clone(void) const
{
    return new CBamException(*this);
}


const char* CBamException::GetType(void) const
{
    return "CBamException";
}


CBamException::TErrCode CBamException::GetErrCode(void) const
{
    return typeid(*this) == typeid(CBamException) ?
        x_GetErrCode() : CException::GetErrCode();
}


const char* CBamException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eOtherError:   return "eOtherError";
    case eNullPtr:      return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg:   return "eInvalidArg";
    case eInitFailed:   return "eInitFailed";
    case eNoData:       return "eNoData";
    case eBadCIGAR:     return "eBadCIGAR";
    case eInvalidBAMFormat: return "eInvalidBAMFormat";
    case eInvalidBAIFormat: return "eInvalidBAIFormat";
    default:            return CException::GetErrCodeString();
    }
}


ostream& operator<<(ostream& out, const CBamRcFormatter& rc)
{
    char buffer[1024];
    size_t error_len;
    RCExplain(rc.GetRC(), buffer, sizeof(buffer), &error_len);
    out << "0x" << hex << rc.GetRC() << dec << ": " << buffer;
    return out;
}


void CBamException::ReportExtra(ostream& out) const
{
    if ( m_RC ) {
        out << CBamRcFormatter(m_RC);
    }
    if ( !m_Param.empty() ) {
        if ( m_RC ) {
            out << ": ";
        }
        out << m_Param;
    }
}


void CBamException::ReportError(const char* msg, rc_t rc)
{
    ERR_POST(msg<<": "<<CBamRcFormatter(rc));
}


void CBamString::x_reserve(size_t min_capacity)
{
    size_t capacity = m_Capacity;
    if ( capacity == 0 ) {
        capacity = min_capacity;
    }
    else {
        while ( capacity < min_capacity ) {
            capacity <<= 1;
        }
    }
    m_Buffer.reset(new char[capacity]);
    m_Capacity = capacity;
}


const char* CSrzException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadFormat:        return "eBadFormat";
    case eNotFound:         return "eNotFound";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


CSrzPath::CSrzPath(void)
{
    x_Init();
    AddRepPath(GetDefaultRepPath());
    AddVolPath(GetDefaultVolPath());
}


CSrzPath::CSrzPath(const string& rep_path, const string& vol_path)
{
    x_Init();
    AddRepPath(rep_path.empty()? GetDefaultRepPath(): rep_path);
    AddVolPath(vol_path.empty()? GetDefaultVolPath(): vol_path);
}


void CSrzPath::x_Init(void)
{
}


void CSrzPath::AddRepPath(const string& rep_path)
{
    NStr::Split(rep_path, ":", m_RepPath);
}


void CSrzPath::AddVolPath(const string& vol_path)
{
    NStr::Split(vol_path, ":", m_VolPath);
}


NCBI_PARAM_DECL(string, SRZ, REP_PATH);
NCBI_PARAM_DEF_EX(string, SRZ, REP_PATH, NCBI_SRZ_REP_PATH,
                  eParam_NoThread, SRZ_REP_PATH);


NCBI_PARAM_DECL(string, SRZ, VOL_PATH);
NCBI_PARAM_DEF_EX(string, SRZ, VOL_PATH, NCBI_SRZ_VOL_PATH,
                  eParam_NoThread, SRZ_VOL_PATH);


string CSrzPath::GetDefaultRepPath(void)
{
    return NCBI_PARAM_TYPE(SRZ, REP_PATH)::GetDefault();
}


string CSrzPath::GetDefaultVolPath(void)
{
    return NCBI_PARAM_TYPE(SRZ, VOL_PATH)::GetDefault();
}


string CSrzPath::FindAccPath(const string& acc, EMissing missing)
{
    if ( acc.size() != 9 && acc.size() != 12 ) {
        // bad length
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accession must be 9 or 12 chars long: "+acc);
        }
        return kEmptyStr;
    }

    string prefix = acc.substr(0, 3);
    NStr::ToUpper(prefix);
    if ( prefix != "SRZ" && prefix != "DRZ" && prefix != "ERZ" ) {
        // bad prefix
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accession must start with SRZ, DRZ, or ERZ: "+acc);
        }
        return kEmptyStr;
    }

    unsigned num;
    try {
        num = NStr::StringToUInt(CTempString(acc).substr(3));
    }
    catch( CException& /*ignored*/ ) {
        // bad number
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accesion is improperly formatted: "+acc);
        }
        return kEmptyStr;
    }

    unsigned level1 = num/1000;
    char sub_dir[128];
    sprintf(sub_dir, "%s/%06u/%s%s/provisional",
            prefix.c_str(), level1, prefix.c_str(), acc.c_str()+3);

    ITERATE ( vector<string>, rep_it, m_RepPath ) {
        ITERATE ( vector<string>, vol_it, m_VolPath ) {
            string dir =
                CFile::MakePath(CFile::MakePath(*rep_it, *vol_it), sub_dir);
            if ( CFile(CFile::MakePath(dir, SRZ_CONFIG_NAME)).Exists() ) {
                return dir;
            }
        }
    }
    if ( missing == eMissing_Throw ) {
        NCBI_THROW(CSrzException, eNotFound,
                   "SRZ accession not found: "+acc);
    }
    return kEmptyStr;
}


NCBI_PARAM_DECL(bool, BAM, CIGAR_IN_ALIGN_EXT);
NCBI_PARAM_DEF(bool, BAM, CIGAR_IN_ALIGN_EXT, true);


static bool s_GetCigarInAlignExt(void)
{
    static bool value = NCBI_PARAM_TYPE(BAM, CIGAR_IN_ALIGN_EXT)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, BAM, USE_RAW_INDEX);
NCBI_PARAM_DEF_EX(bool, BAM, USE_RAW_INDEX, false,
                  eParam_NoThread, BAM_USE_RAW_INDEX);


bool CBamDb::UseRawIndex(EUseAPI use_api)
{
    if ( use_api == eUseDefaultAPI )
        return NCBI_PARAM_TYPE(BAM, USE_RAW_INDEX)::GetDefault();
    else
        return use_api == eUseRawIndex;
}


static
void sx_MapId(CSeq_id& id, IIdMapper* idmapper)
{
    if ( idmapper ) {
        try {
            idmapper->MapObject(id);
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
}


static
CRef<CSeq_id> sx_GetRefSeq_id(const string& str, IIdMapper* idmapper)
{
    CRef<CSeq_id> id;
    try {
        id = new CSeq_id(str);
    }
    catch ( CException& /*ignored*/ ) {
    }
    if ( !id && str.find('|') != NPOS ) {
        try {
            CBioseq::TId ids;
            CSeq_id::ParseIDs(ids, str);
            if ( !ids.empty() ) {
                id = *ids.begin();
            }
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
    if ( !id || (id->IsGi() && id->GetGi() < GI_CONST(1000) ) ) {
        id = new CSeq_id(CSeq_id::e_Local, str);
    }
    sx_MapId(*id, idmapper);
    return id;
}


static
CRef<CSeq_id> sx_GetShortSeq_id(const string& str, IIdMapper* idmapper, bool external)
{
    if ( external ) {
        try {
            CRef<CSeq_id> id(new CSeq_id(str));
            return id;
        }
        catch ( CException& /*ignored*/ ) {
            // continue with local id
        }
    }
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, str));
    //sx_MapId(*id, idmapper);
    return id;
}


CBamMgr::CBamMgr(void)
{
    if ( rc_t rc = AlignAccessMgrMake(x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CBamException, eInitFailed,
                    "Cannot create AlignAccessMgr", rc);
    }
    
    if ( CNcbiApplication* app = CNcbiApplication::Instance() ) {
        string host = app->GetConfig().GetString("CONN", "HTTP_PROXY_HOST", kEmptyStr);
        int port = app->GetConfig().GetInt("CONN", "HTTP_PROXY_PORT", 0);
        if ( !host.empty() && port != 0 ) {
            CBamVFSManager vfs_mgr;
            CBamRef<KNSManager> kns_mgr;
            if ( rc_t rc = VFSManagerGetKNSMgr(vfs_mgr, kns_mgr.x_InitPtr()) ) {
                NCBI_THROW2(CBamException, eInitFailed,
                            "Cannot get KNSManager", rc);
            }
            if ( rc_t rc = KNSManagerSetHTTPProxyPath(kns_mgr, "%s:%d", host.c_str(), port) ) {
                NCBI_THROW2(CBamException, eInitFailed,
                            "Cannot set KNSManager proxy parameters", rc);
            }
            KNSManagerSetHTTPProxyEnabled(kns_mgr, true);
        }
    }
}


#ifdef NCBI_OS_MSWIN
static inline
bool s_HasWindowsDriveLetter(const string& s)
{
    // first symbol is letter, and second symbol is colon (':')
    return s.length() >= 2 && isalpha(Uchar(s[0])) && s[1] == ':';
}


static
bool s_IsSysPath(const string& s)
{
    if ( s_HasWindowsDriveLetter(s) ) {
        return true;
    }
    if ( s.find_first_of("/\\") == NPOS ) {
        // may be plain accession or local file
        if ( CDirEntry(s).Exists() ) {
            // file -> sys path
            return true;
        }
        else {
            // accession
            return false;
        }
    }
    else {
        // may be path or URI
        if ( s[0] == 'h' &&
             (NStr::StartsWith(s, "http://") ||
              NStr::StartsWith(s, "https://")) ) {
            // URI
            return false;
        }
        if ( s[0] == 'f' &&
             NStr::StartsWith(s, "ftp://") ) {
            // URI
            return false;
        }
        // path
        return true;
    }
}
#endif


static VPath* sx_GetVPath(const string& path)
{
#ifdef NCBI_OS_MSWIN
    // SRA SDK doesn't work with UNC paths with backslashes:
    // \\host\share\dir\file
    // As a workaroung we'll replace backslashes with forward slashes.
    string fixed_path = path;
    if ( s_IsSysPath(path) ) {
        try {
            fixed_path = CDirEntry::CreateAbsolutePath(path);
        }
        catch (exception&) {
            // CDirEntry::CreateAbsolutePath() can fail on remote access URL
        }
        replace(fixed_path.begin(), fixed_path.end(), '\\', '/');
        if ( s_HasWindowsDriveLetter(fixed_path) ) {
            // move drive letter from first symbol to second (in place of ':')
            fixed_path[1] = toupper(Uchar(fixed_path[0]));
            // add leading slash
            fixed_path[0] = '/';
        }
    }
    const char* c_path = fixed_path.c_str();
#else
    const char* c_path = path.c_str();
#endif

    CBamVFSManager mgr;
    VPath* kpath;
    if ( rc_t rc = VFSManagerMakePath(mgr, &kpath, c_path) ) {
        NCBI_THROW2(CBamException, eInitFailed,
                    "Cannot create VPath object", rc);
    }
    return kpath;
}

struct VPathReleaser
{
    static void Delete(const VPath* kpath)
        { VPathRelease(kpath); }
};


CBamDb::SAADBImpl::SAADBImpl(const CBamMgr& mgr,
                             const string& db_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name(sx_GetVPath(db_name));
    if ( rc_t rc = AlignAccessMgrMakeBAMDB(mgr, m_DB.x_InitPtr(), kdb_name.get()) ) {
        *m_DB.x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}

CBamDb::SAADBImpl::SAADBImpl(const CBamMgr& mgr,
                             const string& db_name,
                             const string& idx_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name (sx_GetVPath(db_name));
    AutoPtr<VPath, VPathReleaser> kidx_name(sx_GetVPath(idx_name));
    if ( rc_t rc = AlignAccessMgrMakeIndexBAMDB(mgr, m_DB.x_InitPtr(),
                                                kdb_name.get(),
                                                kidx_name.get()) ) {
        *m_DB.x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               EUseAPI use_api)
    : m_DbName(db_name)
{
    if ( UseRawIndex(use_api) ) {
        m_RawDB = new CObjectFor<CBamRawDb>(db_name);
    }
    else {
        m_AADB = new SAADBImpl(mgr, db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               const string& idx_name,
               EUseAPI use_api)
    : m_DbName(db_name),
      m_IndexName(idx_name)
{
    if ( UseRawIndex(use_api) ) {
        m_RawDB = new CObjectFor<CBamRawDb>(db_name, idx_name);
    }
    else {
        m_AADB = new SAADBImpl(mgr, db_name, idx_name);
    }
}


CRef<CSeq_id> CBamDb::GetRefSeq_id(const string& label) const
{
    if ( !m_RefSeqIds ) {
        DEFINE_STATIC_FAST_MUTEX(sx_RefSeqMutex);
        CFastMutexGuard guard(sx_RefSeqMutex);
        if ( !m_RefSeqIds ) {
            AutoPtr<TRefSeqIds> ids(new TRefSeqIds);
            for ( CBamRefSeqIterator it(*this); it; ++it ) {
                string label = it.GetRefSeqId();
                (*ids)[label] = sx_GetRefSeq_id(label, GetIdMapper());
            }
            m_RefSeqIds = ids;
        }
    }
    TRefSeqIds::const_iterator it = m_RefSeqIds->find(label);
    if ( it != m_RefSeqIds->end() ) {
        return it->second;
    }
    return sx_GetRefSeq_id(label, GetIdMapper());
}


CRef<CSeq_id> CBamDb::GetShortSeq_id(const string& str, bool external) const
{
    return sx_GetShortSeq_id(str, GetIdMapper(), external);
}


TSeqPos CBamDb::GetRefSeqLength(const string& id) const
{
    if ( !m_RefSeqLengths ) {
        DEFINE_STATIC_FAST_MUTEX(sx_RefSeqMutex);
        CFastMutexGuard guard(sx_RefSeqMutex);
        if ( !m_RefSeqLengths ) {
            AutoPtr<TRefSeqLengths> lengths(new TRefSeqLengths);
            for ( CBamRefSeqIterator it(*this); it; ++it ) {
                TSeqPos len;
                try {
                    len = it.GetLength();
                }
                catch ( CBamException& /*ignored*/ ) {
                    len = kInvalidSeqPos;
                }
                (*lengths)[it.GetRefSeqId()] = len;
            }
            m_RefSeqLengths = lengths;
        }
    }
    TRefSeqLengths::const_iterator it = m_RefSeqLengths->find(id);
    return it == m_RefSeqLengths->end()? kInvalidSeqPos: it->second;
}


string CBamDb::GetHeaderText(void) const
{
    if ( UsesRawIndex() ) {
        return m_RawDB->GetData().GetHeader().GetText();
    }
    else {
        CMutexGuard guard(m_AADB->m_Mutex);
        CBamRef<const BAMFile> file;
        if ( rc_t rc = AlignAccessDBExportBAMFile(m_AADB->m_DB, file.x_InitPtr()) ) {
            NCBI_THROW2(CBamException, eOtherError,
                        "Cannot get BAMFile pointer", rc);
        }
        const char* header;
        size_t size;
        if ( rc_t rc = BAMFileGetHeaderText(file, &header, &size) ) {
            NCBI_THROW2(CBamException, eOtherError,
                        "Cannot get BAM header text", rc);
        }
        return string(header, size);
    }
}


#ifdef HAVE_NEW_PILEUP_COLLECTOR
CBamDb::ICollectPileupCallback::~ICollectPileupCallback()
{
}


CBamDb::SPileupValues::SPileupValues()
{
}


CBamDb::SPileupValues::SPileupValues(CRange<TSeqPos> ref_range)
{
    initialize(ref_range);
}


void CBamDb::SPileupValues::initialize(CRange<TSeqPos> ref_range)
{
    m_RefToOpen = m_RefFrom = ref_range.GetFrom();
    m_RefStop = ref_range.GetToOpen();
    TSeqPos len = ref_range.GetLength()+32;
    for ( int i = 0; i < kNumStat; ++i ) {
        cc[i].clear();
        cc[i].resize(len);
    }
    cc[kStat_Gap][0] = 0;
}


void CBamDb::SPileupValues::decode_gap(TSeqPos len)
{
    // restore gap counts from delta encoding
    TCount g = 0;
    for ( TSeqPos i = 0; i <= len; ++i ) {
        g += cc[kStat_Gap][i];
        cc[kStat_Gap][i] = g;
    }
}


void CBamDb::SPileupValues::advance_current_beg(TSeqPos ref_pos, ICollectPileupCallback* callback)
{
    if ( ref_pos > m_RefToOpen ) {
        // gap must be filled with zeros
        if ( ref_pos > m_RefToOpen+FLUSH_SIZE ) {
            // gap is big enough to call AddZeros()
            if ( m_RefToOpen != m_RefFrom ) {
                // flush non-zero part
                advance_current_beg(m_RefToOpen, callback);
            }
            _ASSERT(m_RefToOpen == m_RefFrom);
            TSeqPos add_zeros = ref_pos-m_RefToOpen;
            TSeqPos flush_zeros = add_zeros & ~15; // align
            _ASSERT(flush_zeros%16 == 0);
            callback->AddZerosBy16(flush_zeros);
            m_RefToOpen = m_RefFrom += flush_zeros;
            if ( ref_pos > m_RefToOpen ) {
                advance_current_end(ref_pos);
            }
            return;
        }
        advance_current_end(ref_pos);
    }
    TSeqPos flush = ref_pos-m_RefFrom;
    if ( ref_pos != m_RefStop ) {
        flush &= ~15;
    }
    if ( flush ) {
        decode_gap(flush);
        TSeqPos total = m_RefToOpen-m_RefFrom;
        if ( flush >= 16 ) {
            _ASSERT(flush%16 == 0);
            callback->AddValuesBy16(flush&~15, *this);
            if ( TSeqPos copy = total-flush ) {
                TCount gap_save = cc[kStat_Gap][total];
                for ( int i = 0; i < kNumStat; ++i ) {
                    NFast::copy_n_aligned16(cc[i].data()+flush, (copy+15)&~15, cc[i].data());
                }
                cc[kStat_Gap][flush] = gap_save;
            }
            m_RefFrom += flush;
        }
        else {
            _ASSERT(ref_pos == m_RefStop);
            callback->AddValuesTail(flush, *this);
            m_RefFrom = m_RefStop;
        }
    }
}


void CBamDb::SPileupValues::advance_current_end(TSeqPos ref_end)
{
    _ASSERT(ref_end > m_RefToOpen);
    _ASSERT(ref_end <= m_RefStop);
    TSeqPos cur_pos = m_RefToOpen-m_RefFrom;
    TSeqPos new_pos = (min(m_RefStop + 15, ref_end + FLUSH_SIZE) - m_RefFrom) & ~15;
    TCount gap_save = cc[kStat_Gap][cur_pos];
    for ( int i = 0; i < kNumStat; ++i ) {
        NFast::fill_n_zeros_aligned16(cc[i].data()+cur_pos, new_pos-cur_pos);
    }
    cc[kStat_Gap][cur_pos] = gap_save;
    cc[kStat_Gap][new_pos] = 0;
    m_RefToOpen = min(m_RefStop, m_RefFrom + new_pos);
}


void CBamDb::SPileupValues::finalize(ICollectPileupCallback* callback)
{
    if ( m_RefToOpen < m_RefStop ) {
        advance_current_end(m_RefStop);
    }
    _ASSERT(m_RefToOpen == m_RefStop);
    decode_gap(m_RefStop - m_RefFrom);
    if ( callback ) {
        if ( TSeqPos flush = m_RefToOpen-m_RefFrom ) {
            _ASSERT(flush < 16);
            callback->AddValuesTail(flush, *this);
            m_RefFrom += flush;
        }
    }
}


CBamDb::SPileupValues::TCount CBamDb::SPileupValues::get_max_count(int type, TSeqPos length) const
{
    return NFast::max_element_n_aligned16(cc[type].data(), (length+15)&~15);
}


size_t CBamDb::CollectPileup(SPileupValues& values,
                             const string& ref_id,
                             CRange<TSeqPos> graph_range,
                             Uint1 min_quality,
                             ICollectPileupCallback* callback) const
{
    values.initialize(graph_range);
    
    size_t count = 0;

    CBamAlignIterator ait(*this, ref_id, graph_range.GetFrom(), graph_range.GetLength());
    if ( CBamRawAlignIterator* rit = ait.GetRawIndexIteratorPtr() ) {
        for( ; *rit; ++*rit ){
            if ( min_quality > 0 && rit->GetMapQuality() < min_quality ) {
                continue;
            }
            ++count;

            TSeqPos ref_pos = rit->GetRefSeqPos();
            values.update_current_ref_start(ref_pos, callback);
            CTempString read_raw = rit->GetShortSequenceRaw();
            TSeqPos read_pos = 0;
            for ( Uint2 i = 0, count = rit->GetCIGAROpsCount(); i < count; ++i ) {
                if ( ref_pos >= graph_range.GetToOpen() ) {
                    // passed beyond the end of graph range
                    break;
                }
                Uint4 op = rit->GetCIGAROp(i);
                Uint4 seglen = op >> 4;
                op &= 0xf;
                
                TSeqPos ref_end = ref_pos + seglen;
                switch ( op ) {
                case SBamAlignInfo::kCIGAR_eq: // =
                    // match
                    values.add_match_ref_range(ref_pos, ref_end);
                    ref_pos += seglen;
                    read_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_M: // M
                case SBamAlignInfo::kCIGAR_X: // X
                    // mismatch ('X') or
                    // unspecified 'alignment match' ('M') that can be a mismatch too
                    values.add_bases_ref_range_raw(ref_pos, ref_end, read_raw, read_pos);
                    ref_pos += seglen;
                    read_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_I: // I
                case SBamAlignInfo::kCIGAR_S: // S
                    read_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_N: // N
                    // intron
                    ref_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_D: // D
                    // gap or intron
                    values.add_gap_ref_range(ref_pos, ref_end);
                    ref_pos += seglen;
                    break;
                default: // P
                    break;
                }
            }
        }
    }
    else {
        for( ; ait; ++ait ){
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                continue;
            }
            ++count;

            TSeqPos ref_pos = ait.GetRefSeqPos();
            values.update_current_ref_start(ref_pos, callback);
            _ASSERT((values.m_RefFrom-graph_range.GetFrom())%16 == 0);
            _ASSERT((values.m_RefToOpen-values.m_RefFrom)%16 == 0 || values.m_RefToOpen == values.m_RefStop);
            CTempString read = ait.GetShortSequence();
            TSeqPos read_pos = ait.GetCIGARPos();
            CTempString cigar = ait.GetCIGAR();
            const char* ptr = cigar.data();
            const char* end = ptr + cigar.size();
            while ( ptr != end ) {
                if ( ref_pos >= graph_range.GetToOpen() ) {
                    // passed beyond the end of graph range
                    break;
                }
                char type = *ptr;
                TSeqPos seglen = 0;
                for ( ; ++ptr != end; ) {
                    char c = *ptr;
                    if ( c >= '0' && c <= '9' ) {
                        seglen = seglen*10+(c-'0');
                    }
                    else {
                        break;
                    }
                }
                if ( seglen == 0 ) {
                    ERR_POST("Bad CIGAR length: "<<type<<"0 in "<<cigar);
                    break;
                }

                TSeqPos ref_end = ref_pos + seglen;
                if ( type == '=' ) {
                    // match
                    values.add_match_ref_range(ref_pos, ref_end);
                    ref_pos += seglen;
                    read_pos += seglen;
                }
                else if ( type == 'M' || type == 'X' ) {
                    // mismatch ('X') or
                    // unspecified 'alignment match' ('M') that can be a mismatch too
                    values.add_bases_ref_range(ref_pos, ref_end, read, read_pos);
                    ref_pos += seglen;
                    read_pos += seglen;
                }
                else if ( type == 'S' ) {
                    // soft clipping already accounted in seqpos
                }
                else if ( type == 'I' ) {
                    read_pos += seglen;
                }
                else if ( type == 'N' ) {
                    // intron
                    ref_pos += seglen;
                }
                else if ( type == 'D' ) {
                    // gap or intron
                    values.add_gap_ref_range(ref_pos, ref_end);
                    ref_pos += seglen;
                }
                else if ( type != 'P' ) {
                    ERR_POST("Bad CIGAR char: "<<type<<" in "<<cigar);
                    break;
                }
                _ASSERT((values.m_RefFrom-graph_range.GetFrom())%16 == 0);
                _ASSERT((values.m_RefToOpen-values.m_RefFrom)%16 == 0 || values.m_RefToOpen == values.m_RefStop);
            }
        }
    }
    if ( count ) {
        //values.update_current_ref_start(graph_range.GetToOpen(), callback);
        if ( callback && graph_range.GetToOpen() != values.m_RefFrom ) {
            TSeqPos flush = graph_range.GetToOpen() - values.m_RefFrom;
            if ( flush & ~15 ) {
                values.advance_current_beg(values.m_RefFrom+(flush&~15), callback);
            }
            if ( flush & 15 ) {
                values.advance_current_beg(values.m_RefFrom+(flush&15), callback);
            }
            _ASSERT(values.m_RefFrom == graph_range.GetToOpen());
        }
        values.finalize(callback);
    }
    return count;
}
#endif // HAVE_NEW_PILEUP_COLLECTOR

/////////////////////////////////////////////////////////////////////////////

CBamRefSeqIterator::CBamRefSeqIterator()
    : m_DB(0)
{
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamDb& bam_db)
    : m_DB(&bam_db)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawDB = bam_db.m_RawDB;
        if ( m_RawDB->GetData().GetHeader().GetRefs().empty() ) {
            m_RawDB = null;
        }
        m_RefIndex = 0;
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessRefSeqEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBEnumerateRefSequences(bam_db.m_AADB->m_DB, &ptr) ) {
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError, "Cannot find first refseq", rc);
            }
            // no reference sequences found
        }
        else {
            // found first reference sequences
            m_AADBImpl = new SAADBImpl();
            m_AADBImpl->m_Iter.SetReferencedPointer(ptr);
            x_AllocBuffers();
        }
    }
}


void CBamRefSeqIterator::x_AllocBuffers(void)
{
    m_AADBImpl->m_RefSeqIdBuffer.reserve(32);
}


void CBamRefSeqIterator::x_InvalidateBuffers(void)
{
    m_AADBImpl->m_RefSeqIdBuffer.clear();
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamRefSeqIterator& iter)
{
    *this = iter;
}


CBamRefSeqIterator& CBamRefSeqIterator::operator=(const CBamRefSeqIterator& iter)
{
    if ( this != &iter ) {
        m_DB = iter.m_DB;
        m_AADBImpl = iter.m_AADBImpl;
        m_RawDB = iter.m_RawDB;
        m_RefIndex = iter.m_RefIndex;
        m_CachedRefSeq_id.Reset();
    }
    return *this;
}


void CBamRefSeqIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eNoData, "CBamRefSeqIterator is invalid");
    }
}


CBamRefSeqIterator& CBamRefSeqIterator::operator++(void)
{
    if ( m_AADBImpl ) {
        x_InvalidateBuffers();
        if ( rc_t rc = AlignAccessRefSeqEnumeratorNext(m_AADBImpl->m_Iter) ) {
            m_AADBImpl.Reset();
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError,
                            "Cannot find next refseq", rc);
            }
            // no more reference sequences
        }
    }
    else {
        if( ++m_RefIndex == m_RawDB->GetData().GetHeader().GetRefs().size() ) {
            // no more reference sequences
            m_RawDB.Reset();
        }
    }
    m_CachedRefSeq_id.Reset();
    return *this;
}


bool CBamRefSeqIterator::x_CheckRC(CBamString& buf,
                                   rc_t rc,
                                   size_t size,
                                   const char* msg) const
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 ) {
            // omit trailing zero char
            if ( buf[size-1] ) {
                ERR_POST("No zero at the end: " << string(buf.data(), size-1));
            }
            _ASSERT(buf[size-1] == '\0');
            buf.resize(size-1);
        }
        else {
            buf.clear();
        }
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW3(CBamException, eNoData,
                    "Cannot get value", rc, msg);
    }
}


void CBamRefSeqIterator::x_GetString(CBamString& buf,
                                     const char* msg, TGetString func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, buf.data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


CTempString CBamRefSeqIterator::GetRefSeqId(void) const
{
    if ( m_AADBImpl ) {
        x_GetString(m_AADBImpl->m_RefSeqIdBuffer, "RefSeqId",
                    AlignAccessRefSeqEnumeratorGetID);
        return m_AADBImpl->m_RefSeqIdBuffer;
    }
    else {
        return m_RawDB->GetData().GetHeader().GetRefName(m_RefIndex);
    }
}


CRef<CSeq_id> CBamRefSeqIterator::GetRefSeq_id(void) const
{
    if ( !m_CachedRefSeq_id ) {
        m_CachedRefSeq_id = m_DB->GetRefSeq_id(GetRefSeqId());
    }
    return m_CachedRefSeq_id;
}


TSeqPos CBamRefSeqIterator::GetLength(void) const
{
    if ( m_AADBImpl ) {
        uint64_t length;
        if ( rc_t rc = AlignAccessRefSeqEnumeratorGetLength(m_AADBImpl->m_Iter, &length) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "CBamRefSeqIterator::GetLength() cannot get length", rc);
        }
        if ( length >= kInvalidSeqPos ) {
            NCBI_THROW(CBamException, eOtherError,
                       "CBamRefSeqIterator::GetLength() length is too big");
        }
        return TSeqPos(length);
    }
    else {
        return m_RawDB->GetData().GetHeader().GetRefLength(m_RefIndex);
    }
}


/////////////////////////////////////////////////////////////////////////////

CBamAlignIterator::SRawImpl::SRawImpl(CObjectFor<CBamRawDb>& db)
    : m_RawDB(&db),
      m_Iter(db)
{
}


CBamAlignIterator::SRawImpl::SRawImpl(CObjectFor<CBamRawDb>& db,
                                      const string& ref_label,
                                      TSeqPos ref_pos,
                                      TSeqPos window,
                                      CBamAlignIterator::ESearchMode search_mode)
    : m_RawDB(&db),
      m_Iter(db, ref_label, ref_pos, window, CBamRawAlignIterator::ESearchMode(search_mode))
{
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
}


CBamAlignIterator::SRawImpl::SRawImpl(CObjectFor<CBamRawDb>& db,
                                      const string& ref_label,
                                      TSeqPos ref_pos,
                                      TSeqPos window,
                                      CBamIndex::EIndexLevel min_level,
                                      CBamIndex::EIndexLevel max_level,
                                      CBamAlignIterator::ESearchMode search_mode)
    : m_RawDB(&db),
      m_Iter(db, ref_label, ref_pos, window, min_level, max_level, CBamRawAlignIterator::ESearchMode(search_mode))
{
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
}


void CBamAlignIterator::SRawImpl::x_InvalidateBuffers()
{
    m_ShortSequence.clear();
    m_CIGAR.clear();
}


CBamAlignIterator::SAADBImpl::SAADBImpl(const CBamDb::SAADBImpl& db,
                                        AlignAccessAlignmentEnumerator* ptr)
    : m_DB(&db),
      m_Guard(db.m_Mutex)
{
    m_Iter.SetReferencedPointer(ptr);
    m_RefSeqId.reserve(32);
    m_ShortSeqId.reserve(32);
    m_ShortSeqAcc.reserve(32);
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
    m_Strand = eStrand_not_read;
}


void CBamAlignIterator::SAADBImpl::x_InvalidateBuffers()
{
    m_RefSeqId.clear();
    m_ShortSeqId.clear();
    m_ShortSeqAcc.clear();
    m_ShortSequence.clear();
    m_CIGAR.clear();
    m_Strand = eStrand_not_read;
}


TSeqPos CBamAlignIterator::SAADBImpl::GetRefSeqPos() const
{
    uint64_t pos = 0;
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetRefSeqPos(m_Iter, &pos) ) {
        if ( GetRCObject(rc) == RCObject(rcData) &&
             GetRCState(rc) == rcNotFound ) {
            return kInvalidSeqPos;
        }
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get RefSeqPos", rc);
    }
    return TSeqPos(pos);
}


CBamAlignIterator::CBamAlignIterator(void)
    : m_DB(0),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db)
    : m_DB(&bam_db),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawImpl = new SRawImpl(bam_db.m_RawDB.GetNCObject());
        if ( !m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessAlignmentEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBEnumerateAlignments(bam_db.m_AADB->m_DB, &ptr) ) {
            if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
                // error
                NCBI_THROW2(CBamException, eNoData, "Cannot find first alignment", rc);
            }
            // no alignments
        }
        else {
            // found first alignment
            m_AADBImpl = new SAADBImpl(*bam_db.m_AADB, ptr);
        }
    }
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db,
                                     const string& ref_id,
                                     TSeqPos ref_pos,
                                     TSeqPos window,
                                     ESearchMode search_mode)
    : m_DB(&bam_db),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawImpl = new SRawImpl(bam_db.m_RawDB.GetNCObject(), ref_id, ref_pos, window, search_mode);
        if ( !m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessAlignmentEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBWindowedAlignments(bam_db.m_AADB->m_DB, &ptr,
                                                       ref_id.c_str(), ref_pos, window) ) {
            if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
                // error
                NCBI_THROW2(CBamException, eNoData, "Cannot find first alignment", rc);
            }
            // no alignments
            return;
        }
        // found first alignment
        m_AADBImpl = new SAADBImpl(*bam_db.m_AADB, ptr);
        if ( search_mode == eSearchByStart ) {
            // skip alignments that start before the requested range
            while ( m_AADBImpl->GetRefSeqPos() < ref_pos ) {
                if ( rc_t rc = AlignAccessAlignmentEnumeratorNext(ptr) ) {
                    m_AADBImpl.Reset();
                    if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
                        // error
                        NCBI_THROW2(CBamException, eOtherError, "Cannot find first alignment", rc);
                    }
                    else {
                        // no matching alignment found
                        return;
                    }
                }
            }
        }
    }
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db,
                                     const string& ref_id,
                                     TSeqPos ref_pos,
                                     TSeqPos window,
                                     CBamIndex::EIndexLevel min_level,
                                     CBamIndex::EIndexLevel max_level,
                                     ESearchMode search_mode)
    : m_DB(&bam_db),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawImpl = new SRawImpl(bam_db.m_RawDB.GetNCObject(), ref_id, ref_pos, window, min_level, max_level, search_mode);
        if ( !m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
    }
    else {
        NCBI_THROW(CBamException, eInvalidArg, "BAM index levels are supported only in raw index mode");
    }
}


CBamAlignIterator::CBamAlignIterator(const CBamAlignIterator& iter)
{
    *this = iter;
}


CBamAlignIterator& CBamAlignIterator::operator=(const CBamAlignIterator& iter)
{
    if ( this != &iter ) {
        m_DB = iter.m_DB;
        m_AADBImpl = iter.m_AADBImpl;
        m_RawImpl = iter.m_RawImpl;
        m_SpotIdDetector = iter.m_SpotIdDetector;
        m_BamFlagsAvailability = iter.m_BamFlagsAvailability;
    }
    return *this;
}


void CBamAlignIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eNoData, "CBamAlignIterator is invalid");
    }
}


CBamAlignIterator& CBamAlignIterator::operator++(void)
{
    x_CheckValid();
    m_RefSeq_id.Reset();
    m_ShortSeq_id.Reset();
    if ( m_AADBImpl ) {
        if ( rc_t rc = AlignAccessAlignmentEnumeratorNext(m_AADBImpl->m_Iter) ) {
            m_AADBImpl.Reset();
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError, "Cannot find next alignment", rc);
            }
            // end of iteration, keep the error code
        }
        else {
            // next alignment
            m_AADBImpl->x_InvalidateBuffers();
        }
    }
    else {
        if ( !++m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
        else {
            m_RawImpl->x_InvalidateBuffers();
        }
    }
    return *this;
}


bool CBamAlignIterator::x_CheckRC(CBamString& buf,
                                  rc_t rc,
                                  size_t size,
                                  const char* msg) const
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 ) {
            // omit trailing zero char
            if ( buf[size-1] ) {
                ERR_POST("No zero at the end: " << string(buf.data(), size-1));
            }
            _ASSERT(buf[size-1] == '\0');
            buf.resize(size-1);
        }
        else {
            buf.clear();
        }
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW3(CBamException, eNoData,
                    "Cannot get value", rc, msg);
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf,
                                    const char* msg, TGetString func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, buf.data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf, uint64_t& pos,
                                    const char* msg, TGetString2 func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, &pos, buf.data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


CTempString CBamAlignIterator::GetRefSeqId(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_RawDB->GetData().GetHeader().GetRefName(m_RawImpl->m_Iter.GetRefSeqIndex());
    }
    else {
        x_GetString(m_AADBImpl->m_RefSeqId, "RefSeqId",
                    AlignAccessAlignmentEnumeratorGetRefSeqID);
        return m_AADBImpl->m_RefSeqId;
    }
}


TSeqPos CBamAlignIterator::GetRefSeqPos(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetRefSeqPos();
    }
    else {
        return m_AADBImpl->GetRefSeqPos();
    }
}


CTempString CBamAlignIterator::GetShortSeqId(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetShortSeqId();
    }
    else {
        x_GetString(m_AADBImpl->m_ShortSeqId, "ShortSeqId",
                    AlignAccessAlignmentEnumeratorGetShortSeqID);
        return m_AADBImpl->m_ShortSeqId;
    }
}


CTempString CBamAlignIterator::GetShortSeqAcc(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetShortSeqAcc();
    }
    else {
        x_GetString(m_AADBImpl->m_ShortSeqAcc, "ShortSeqAcc",
                    AlignAccessAlignmentEnumeratorGetShortSeqAccessionID);
        return m_AADBImpl->m_ShortSeqAcc;
    }
}


CTempString CBamAlignIterator::GetShortSequence(void) const
{
    if ( m_RawImpl ) {
        if ( m_RawImpl->m_ShortSequence.empty() ) {
            m_RawImpl->m_Iter.GetShortSequence(m_RawImpl->m_ShortSequence);
        }
        return m_RawImpl->m_ShortSequence;
    }
    else {
        if ( m_AADBImpl->m_ShortSequence.empty() ) {
            x_GetString(m_AADBImpl->m_ShortSequence, "ShortSequence",
                        AlignAccessAlignmentEnumeratorGetShortSequence);
        }
        return m_AADBImpl->m_ShortSequence;
    }
}


TSeqPos CBamAlignIterator::GetShortSequenceLength(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetShortSequenceLength();
    }
    else {
        return TSeqPos(GetShortSequence().size());
    }
}


inline void CBamAlignIterator::x_GetCIGAR(void) const
{
    x_GetString(m_AADBImpl->m_CIGAR, m_AADBImpl->m_CIGARPos, "CIGAR",
                AlignAccessAlignmentEnumeratorGetCIGAR);
}


TSeqPos CBamAlignIterator::GetCIGARPos(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARPos();
    }
    else {
        x_GetCIGAR();
        return TSeqPos(m_AADBImpl->m_CIGARPos);
    }
}


CTempString CBamAlignIterator::GetCIGAR(void) const
{
    if ( m_RawImpl ) {
        if ( m_RawImpl->m_CIGAR.empty() ) {
            m_RawImpl->m_Iter.GetCIGAR(m_RawImpl->m_CIGAR);
        }
        return m_RawImpl->m_CIGAR;
    }
    else {
        x_GetCIGAR();
        return m_AADBImpl->m_CIGAR;
    }
}


void CBamAlignIterator::GetRawCIGAR(vector<Uint4>& raw_cigar) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGAR(raw_cigar);
    }
    else {
        x_GetCIGAR();
        raw_cigar.clear();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            const char* types = "MIDNSHP=X";
            const char* ptr = strchr(types, type);
            unsigned op = ptr? unsigned(ptr-types): 15u;
            raw_cigar.push_back((len<<4)|(op));
        }
    }
}


TSeqPos CBamAlignIterator::GetCIGARRefSize(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARRefSize();
    }
    else {
        TSeqPos ref_size = 0;
        x_GetCIGAR();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                ref_size += len;
            }
            else if ( type == 'I' || type == 'S' ) {
                // insert
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
                ref_size += len;
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " << type << " in " << m_AADBImpl->m_CIGAR);
            }
            if ( len == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type << "0 in " << m_AADBImpl->m_CIGAR);
            }
        }
        return ref_size;
    }
}


TSeqPos CBamAlignIterator::GetCIGARShortSize(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARShortSize();
    }
    else {
        TSeqPos short_size = 0;
        x_GetCIGAR();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                short_size += len;
            }
            else if ( type == 'I' || type == 'S' ) {
                // insert
                short_size += len;
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " << type << " in " << m_AADBImpl->m_CIGAR);
            }
            if ( len == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type << "0 in " << m_AADBImpl->m_CIGAR);
            }
        }
        return short_size;
    }
}


pair< COpenRange<TSeqPos>, COpenRange<TSeqPos> >
CBamAlignIterator::GetCIGARAlignment(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARAlignment();
    }
    else {
        pair< COpenRange<TSeqPos>, COpenRange<TSeqPos> > ret;
        ret.first.SetFrom(GetRefSeqPos());
        x_GetCIGAR();
        ret.second.SetFrom(TSeqPos(m_AADBImpl->m_CIGARPos));
        TSeqPos ref_size = 0, short_size = 0;
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                ref_size += len;
                short_size += len;
            }
            else if ( type == 'I' || type == 'S' ) {
                // insert
                short_size += len;
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
                ref_size += len;
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " << type << " in " << m_AADBImpl->m_CIGAR);
            }
            if ( len == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type << "0 in " << m_AADBImpl->m_CIGAR);
            }
        }
        ret.first.SetLength(ref_size);
        ret.second.SetLength(short_size);
        return ret;
    }
}


CRef<CSeq_id> CBamAlignIterator::GetRefSeq_id(void) const
{
    if ( !m_RefSeq_id ) {
        m_RefSeq_id = m_DB->GetRefSeq_id(GetRefSeqId());
    }
    return m_RefSeq_id;
}


CRef<CSeq_id> CBamAlignIterator::GetShortSeq_id(const string& str) const
{
    return sx_GetShortSeq_id(str, GetIdMapper(), GetShortSequenceLength() == 0);
}


CRef<CSeq_id> CBamAlignIterator::GetShortSeq_id(void) const
{
    if ( !m_ShortSeq_id ) {
        string id = GetShortSeqId();
        bool paired = IsPaired(), is_1st = false, is_2nd = false;
        if ( paired ) {
            // regular way to get pairing info
            is_1st = IsFirstInPair();
            is_2nd = IsSecondInPair();
        }
        else {
            // more pairing info may be available via BAM file flags
            Uint2 flags;
            if ( TryGetFlags(flags) ) {
                // use flags to get pairing info faster
                paired = (flags & (BAMFlags_WasPaired |
                                   BAMFlags_IsMappedAsPair)) != 0;
                is_1st = (flags & BAMFlags_IsFirst) != 0;
                is_2nd = (flags & BAMFlags_IsSecond) != 0;
            }
        }
        if ( paired ) {
            if ( is_1st && !is_2nd ) {
                id += ".1";
            }
            else if ( is_2nd && !is_1st ) {
                id += ".2";
            }
            else {
                // conflict
                if ( ISpotIdDetector* detector = GetSpotIdDetector() ) {
                    detector->AddSpotId(id, this);
                }
                else {
                    id += ".?";
                }
            }
        }
        m_ShortSeq_id = GetShortSeq_id(id);
    }
    return m_ShortSeq_id;
}


void CBamAlignIterator::SetRefSeq_id(CRef<CSeq_id> seq_id)
{
    m_RefSeq_id = seq_id;
}


void CBamAlignIterator::SetShortSeq_id(CRef<CSeq_id> seq_id)
{
    m_ShortSeq_id = seq_id;
}


void CBamAlignIterator::x_GetStrand(void) const
{
    x_CheckValid();
    if ( m_AADBImpl->m_Strand != eStrand_not_read ) {
        return;
    }
    
    m_AADBImpl->m_Strand = eStrand_not_set;
    AlignmentStrandDirection dir;
    if ( AlignAccessAlignmentEnumeratorGetStrandDirection(m_AADBImpl->m_Iter, &dir) != 0 ) {
        return;
    }
    
    switch ( dir ) {
    case asd_Forward:
        m_AADBImpl->m_Strand = eNa_strand_plus;
        break;
    case asd_Reverse:
        m_AADBImpl->m_Strand = eNa_strand_minus;
        break;
    default:
        m_AADBImpl->m_Strand = eNa_strand_unknown;
        break;
    }
}


bool CBamAlignIterator::IsSetStrand(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsSetStrand();
    }
    else {
        x_GetStrand();
        return m_AADBImpl->m_Strand != eStrand_not_set;
    }
}


ENa_strand CBamAlignIterator::GetStrand(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetStrand();
    }
    else {
        if ( !IsSetStrand() ) {
            NCBI_THROW(CBamException, eNoData,
                       "Strand is not set");
        }
        return ENa_strand(m_AADBImpl->m_Strand);
    }
}


Uint1 CBamAlignIterator::GetMapQuality(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetMapQuality();
    }
    else {
        x_CheckValid();
        uint8_t q = 0;
        if ( rc_t rc = AlignAccessAlignmentEnumeratorGetMapQuality(m_AADBImpl->m_Iter, &q) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get MapQuality", rc);
        }
        return q;
    }
}


bool CBamAlignIterator::IsPaired(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsPaired();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc = AlignAccessAlignmentEnumeratorGetIsPaired(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsPaired flag", rc);
        }
        return f;
    }
}


bool CBamAlignIterator::IsFirstInPair(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsFirstInPair();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsFirstInPair(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsFirstInPair flag", rc);
        }
        return f;
    }
}


bool CBamAlignIterator::IsSecondInPair(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsSecondInPair();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsSecondInPair(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsSecondInPair flag", rc);
        }
        return f;
    }
}


CBamFileAlign::CBamFileAlign(const CBamAlignIterator& iter)
{
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetBAMAlignment(iter.m_AADBImpl->m_Iter, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get BAM file alignment", rc);
    }
}


Int4 CBamFileAlign::GetRefSeqIndex(void) const
{
    int32_t id;
    if ( rc_t rc = BAMAlignmentGetRefSeqId(*this, &id) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get BAM RefSeqIndex", rc);
    }
    return id;
}


Int4 CBamAlignIterator::GetRefSeqIndex(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetRefSeqIndex();
    }
    else {
        x_CheckValid();
        return CBamFileAlign(*this).GetRefSeqIndex();
    }
}


Uint2 CBamFileAlign::GetFlags(void) const
{
    uint16_t flags;
    if ( rc_t rc = BAMAlignmentGetFlags(*this, &flags) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get BAM flags", rc);
    }
    return flags;
}


bool CBamFileAlign::TryGetFlags(Uint2& flags) const
{
    return BAMAlignmentGetFlags(*this, &flags) == 0;
}


Uint2 CBamAlignIterator::GetFlags(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetFlags();
    }
    else {
        x_CheckValid();
        try {
            Uint2 flags = CBamFileAlign(*this).GetFlags();
            if ( m_BamFlagsAvailability != eBamFlags_Available ) {
                m_BamFlagsAvailability = eBamFlags_Available;
            }
            return flags;
        }
        catch ( CBamException& /* will be rethrown */ ) {
            if ( m_BamFlagsAvailability != eBamFlags_NotAvailable ) {
                m_BamFlagsAvailability = eBamFlags_NotAvailable;
            }
            throw;
        }
    }
}


bool CBamAlignIterator::TryGetFlags(Uint2& flags) const
{
    if ( m_RawImpl ) {
        flags = m_RawImpl->m_Iter.GetFlags();
        return true;
    }
    else {
        if ( !*this || m_BamFlagsAvailability == eBamFlags_NotAvailable ) {
            return false;
        }
        if ( !CBamFileAlign(*this).TryGetFlags(flags) ) {
            m_BamFlagsAvailability = eBamFlags_NotAvailable;
            return false;
        }
        if ( m_BamFlagsAvailability != eBamFlags_Available ) {
            m_BamFlagsAvailability = eBamFlags_Available;
        }
        return true;
    }
}


CRef<CBioseq> CBamAlignIterator::GetShortBioseq(void) const
{
    CTempString data = GetShortSequence();
    TSeqPos length = TSeqPos(data.size());
    if ( length == 0 ) {
        // no actual sequence
        return null;
    }
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetShortSeq_id());
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_na);
    inst.SetLength(length);
    string& iupac = inst.SetSeq_data().SetIupacna().Set();
    iupac.assign(data.data(), length);
    if ( GetStrand() == eNa_strand_minus ) {
        CSeqManip::ReverseComplement(iupac, CSeqUtil::e_Iupacna, 0, length);
    }
    return seq;
}


CRef<CSeq_align> CBamAlignIterator::GetMatchAlign(void) const
{
    if ( GetRefSeqPos() == kInvalidSeqPos ) {
        return null;
    }
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_diags);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetIds().push_back(GetRefSeq_id());
    denseg.SetIds().push_back(GetShortSeq_id());
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();

    int segcount = 0;
    if ( m_RawImpl ) {
        m_RawImpl->m_Iter.GetSegments(starts, lens);
        segcount = int(lens.size());
    }
    else {
        TSeqPos refpos = GetRefSeqPos();
        TSeqPos seqpos = GetCIGARPos();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos seglen;
        TSeqPos refstart = 0, seqstart = 0;
        while ( ptr != end ) {
            type = *ptr;
            for ( seglen = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    seglen = seglen*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                refstart = refpos;
                refpos += seglen;
                seqstart = seqpos;
                seqpos += seglen;
            }
            else if ( type == 'I' || type == 'S' ) {
                refstart = kInvalidSeqPos;
                seqstart = seqpos;
                seqpos += seglen;
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
                refstart = refpos;
                refpos += seglen;
                seqstart = kInvalidSeqPos;
            }
            else if ( type == 'P' ) {
                continue;
            }
            else {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " <<type<< " in " <<m_AADBImpl->m_CIGAR);
            }
            if ( seglen == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type <<
                               "0 in " << m_AADBImpl->m_CIGAR);
            }
            starts.push_back(refstart);
            starts.push_back(seqstart);
            lens.push_back(seglen);
            ++segcount;
        }
    }
    if ( GetStrand() == eNa_strand_minus ) {
        CDense_seg::TStrands& strands = denseg.SetStrands();
        strands.reserve(2*segcount);
        TSeqPos end = GetShortSequenceLength();
        for ( int i = 0; i < segcount; ++i ) {
            strands.push_back(eNa_strand_plus);
            strands.push_back(eNa_strand_minus);
            TSeqPos pos = starts[i*2+1];
            TSeqPos len = lens[i];
            if ( pos != kInvalidSeqPos ) {
                starts[i*2+1] = end - (pos + len);
            }
        }
    }

    denseg.SetNumseg(segcount);

    if ( s_GetCigarInAlignExt() ) {
        CRef<CUser_object> obj(new CUser_object);
        CRef<CObject_id> id;
        id = new CObject_id();
        id->SetStr("Tracebacks");
        obj->SetType(*id);
        
        CRef<CUser_field> field(new CUser_field());
        id = new CObject_id();
        id->SetStr("CIGAR");
        field->SetLabel(*id);
        field->SetData().SetStr(GetCIGAR());
        obj->SetData().push_back(field);

        align->SetExt().push_back(obj);
    }
    
    return align;
}


CRef<CSeq_annot>
CBamAlignIterator::x_GetSeq_annot(const string* annot_name) const
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign();
    if ( annot_name ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(*annot_name);
        annot->SetDesc().Set().push_back(desc);
    }
    return annot;
}


CRef<CSeq_entry>
CBamAlignIterator::x_GetMatchEntry(const string* annot_name) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    if ( CRef<CBioseq> seq = GetShortBioseq() ) {
        entry->SetSeq(*seq);
    }
    else {
        entry->SetSet().SetSeq_set();
    }
    if ( CRef<CSeq_align> align = GetMatchAlign() ) {
        CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
        entry->SetAnnot().push_back(annot);
        annot->SetData().SetAlign().push_back(align);
    }
    return entry;
}


/////////////////////////////////////////////////////////////////////////////
// CBamAlignIterator::ISpotIdDetector

CBamAlignIterator::ISpotIdDetector::~ISpotIdDetector(void)
{
}


END_SCOPE(objects)

#ifdef HAVE_NEW_PILEUP_COLLECTOR
BEGIN_NAMESPACE(NFast);

void fill_n_zeros_aligned16(int* dst, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst+0, zero);
        _mm_store_si128((__m128i*)dst+1, zero);
        _mm_store_si128((__m128i*)dst+2, zero);
        _mm_store_si128((__m128i*)dst+3, zero);
    }
}

/*
void fill_n_zeros(int* dst, size_t count)
{
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst+0, zero);
        _mm_store_si128((__m128i*)dst+1, zero);
        _mm_store_si128((__m128i*)dst+2, zero);
        _mm_store_si128((__m128i*)dst+3, zero);
    }
}
*/

void fill_n_zeros_aligned16(char* dst, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
}

/*
void fill_n_zeros(char* dst, size_t count)
{
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
}
*/

void copy_n_bytes_aligned16(const char* src, size_t count, int* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i mask = _mm_set_epi8(-128, -128, -128, 3,
                                -128, -128, -128, 2,
                                -128, -128, -128, 1,
                                -128, -128, -128, 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        uint32_t bb0 = ((const uint32_t*)src)[0];
        uint32_t bb1 = ((const uint32_t*)src)[1];
        uint32_t bb2 = ((const uint32_t*)src)[2];
        uint32_t bb3 = ((const uint32_t*)src)[3];
        __m128i ww0 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb0), mask);
        __m128i ww1 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb1), mask);
        __m128i ww2 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb2), mask);
        __m128i ww3 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb3), mask);
        _mm_store_si128((__m128i*)dst+0, ww0);
        _mm_store_si128((__m128i*)dst+1, ww1);
        _mm_store_si128((__m128i*)dst+2, ww2);
        _mm_store_si128((__m128i*)dst+3, ww3);
    }
}


void copy_n_aligned16(const int* src, size_t count, char* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i mask = _mm_set_epi8(-128, -128, -128, -128,
                                -128, -128, -128, -128,
                                -128, -128, -128, -128,
                                12, 8, 4, 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_shuffle_epi8(ww0, mask);
        ww1 = _mm_shuffle_epi8(ww1, mask);
        ww2 = _mm_shuffle_epi8(ww2, mask);
        ww3 = _mm_shuffle_epi8(ww3, mask);
        ww0 = _mm_or_si128(ww0, _mm_slli_si128(ww1, 4));
        ww2 = _mm_or_si128(ww2, _mm_slli_si128(ww3, 4));
        ww0 = _mm_or_si128(ww0, _mm_slli_si128(ww2, 8));
        _mm_store_si128((__m128i*)dst, ww0);
    }
}
    

void copy_n_aligned16(const int* src, size_t count, int* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        _mm_store_si128((__m128i*)dst+0, ww0);
        _mm_store_si128((__m128i*)dst+1, ww1);
        _mm_store_si128((__m128i*)dst+2, ww2);
        _mm_store_si128((__m128i*)dst+3, ww3);
    }
}

/*
void copy_n(const int* src, size_t count, char* dst)
{
    for ( ; count && (intptr_t(dst)%4); ++dst, --count, ++src ) {
        *dst = *src;
    }
    __m128i mask = _mm_set_epi8(128, 128, 128, 128,
                                128, 128, 128, 128,
                                128, 128, 128, 128,
                                12, 8, 4, 0);
    for ( ; count >= 4; dst += 4, count -= 4, src += 4 ) {
        __m128i ww = _mm_load_si128((const __m128i*)src);
        uint32_t bb = _mm_cvtsi128_si32(_mm_shuffle_epi8(ww, mask));
        *(uint32_t*)dst = bb;
    }
    for ( ; count; ++dst, --count, ++src ) {
        *dst = *src;
    }
}


void copy_n(const int* src, size_t count, int* dst)
{
    for ( ; count && (intptr_t(dst)%16); ++dst, --count, ++src ) {
        *dst = *src;
    }
    __m128i mask = _mm_set_epi8(128, 128, 128, 128,
                                128, 128, 128, 128,
                                128, 128, 128, 128,
                                12, 8, 4, 0);
    for ( ; count >= 4; dst += 4, count -= 4, src += 4 ) {
        __m128i ww = _mm_load_si128((const __m128i*)src);
        uint32_t bb = _mm_cvtsi128_si32(_mm_shuffle_epi8(ww, mask));
        *(uint32_t*)dst = bb;
    }
    for ( ; count; ++dst, --count, ++src ) {
        *dst = *src;
    }
}
*/

unsigned max_element_n_aligned16(const unsigned* src, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    __m128i max4 = _mm_setzero_si128();
    for ( auto src_end = src+count; src < src_end; src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_max_epu32(ww0, ww1);
        ww2 = _mm_max_epu32(ww2, ww3);
        ww0 = _mm_max_epu32(ww0, ww2);
        max4 = _mm_max_epu32(max4, ww0);
    }
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 8));
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 4));
    return _mm_cvtsi128_si32(max4);
}


END_NAMESPACE(NFast);
#endif // HAVE_NEW_PILEUP_COLLECTOR

END_NCBI_SCOPE
