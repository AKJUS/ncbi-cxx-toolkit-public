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
 * Author: Eugene Vasilchenko
 *
 * File Description: SNP file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/data_loaders/snp/snploader.hpp>
#include <sra/data_loaders/snp/impl/snploader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <objects/dbsnp/primary_track/snpptis.hpp>

#include <algorithm>
#include <cmath>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   SNPLoader
NCBI_DEFINE_ERR_SUBCODE_X(16);

BEGIN_SCOPE(objects)

class CDataLoader;

NCBI_PARAM_DECL(int, SNP_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, SNP_LOADER, DEBUG, 0,
                  eParam_NoThread, SNP_LOADER_DEBUG);

enum {
    eDebug_open = 1,
    eDebug_open_time = 2,
    eDebug_load = 3,
    eDebug_load_time = 4,
    eDebug_resolve = 5,
    eDebug_data = 9
};

static int GetDebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, DEBUG)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(size_t, SNP_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, SNP_LOADER, GC_SIZE, 10,
                  eParam_NoThread, SNP_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, GC_SIZE)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(unsigned, SNP_LOADER, RETRY_COUNT);
NCBI_PARAM_DEF(unsigned, SNP_LOADER, RETRY_COUNT, 3);

static unsigned GetRetryCountParam(void)
{
    static unsigned value = NCBI_PARAM_TYPE(SNP_LOADER, RETRY_COUNT)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(unsigned, SNP_LOADER, FILE_REOPEN_TIME);
NCBI_PARAM_DEF(unsigned, SNP_LOADER, FILE_REOPEN_TIME, 60*60); // 1 hour


static unsigned GetFileReopenTimeParam(void)
{
    static unsigned value =
        NCBI_PARAM_TYPE(SNP_LOADER, FILE_REOPEN_TIME)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(unsigned, SNP_LOADER, FILE_RECHECK_TIME);
NCBI_PARAM_DEF(unsigned, SNP_LOADER, FILE_RECHECK_TIME, 5*60); // 5 minutes


static unsigned GetFileRecheckTimeParam(void)
{
    static unsigned value =
        NCBI_PARAM_TYPE(SNP_LOADER, FILE_RECHECK_TIME)::GetDefault();
    return value;
}


/*
NCBI_PARAM_DECL(string, SNP_LOADER, ANNOT_NAME);
NCBI_PARAM_DEF_EX(string, SNP_LOADER, ANNOT_NAME, "SNP",
                  eParam_NoThread, SNP_LOADER_ANNOT_NAME);

static string GetDefaultAnnotName(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, ANNOT_NAME)> s_Value;
    return s_Value->Get();
}
*/


NCBI_PARAM_DECL(bool, SNP_LOADER, SPLIT);
NCBI_PARAM_DEF_EX(bool, SNP_LOADER, SPLIT, true,
                  eParam_NoThread, SNP_LOADER_SPLIT);

static bool IsSplitEnabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, SPLIT)> s_Value;
    return s_Value->Get();
}


/////////////////////////////////////////////////////////////////////////////
// CSNPBlobId
/////////////////////////////////////////////////////////////////////////////

// Blob id:
// sat = 2001-2099 : SNP NA version 1 - 99
// or, for primary SNP track:
// sat = 3001-3099 : SNP NA version 1 - 99
// subsat : NA accession number
// or, for primary SNP graph track:
// NA accession number + kSNPSubSatGraph(=1000000000)
// satkey : SequenceIndex + 1000000*FilterIndex;
// satkey bits 24-30: 

const int kSNPSatBase = 2000;
const int kSNPSatPrimary = 3000;
const int kSNPSubSatGraph = 1000000000;
const int kNAVersionMin = 1;
const int kNAVersionMax = 99;
const int kSeqIndexCount = 1000000;
const int kFilterIndexCount = 2000;
const int kFilterIndexMaxLength = 4;

CSNPBlobId::CSNPBlobId(const CTempString& str)
{
    FromString(str);
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file,
                       const CSeq_id_Handle& seq_id,
                       size_t filter_index)
    : m_NAIndex(0),
      m_NAVersion(0),
      m_IsPrimaryTrack(false),
      m_IsPrimaryTrackGraph(false),
      m_SeqIndex(0),
      m_FilterIndex(Uint4(filter_index)),
      m_Accession(file.GetAccession()),
      m_SeqId(seq_id)
{
    // non-SatId
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file,
                       size_t seq_index,
                       size_t filter_index)
    : m_NAIndex(0),
      m_NAVersion(0),
      m_IsPrimaryTrack(false),
      m_IsPrimaryTrackGraph(false),
      m_SeqIndex(Uint4(seq_index)),
      m_FilterIndex(Uint4(filter_index))
{
    if ( file.IsValidNA() ) {
        SetSatNA(file.GetAccession());
    }
    else {
        // non-SatId
        m_Accession = file.GetAccession();
    }
    SetSeqAndFilterIndex(seq_index, filter_index);
}


CSNPBlobId::CSNPBlobId(const CSNPDbSeqIterator& seq,
                       size_t filter_index)
{
    SetSatNA(seq.GetDb().GetDbPath());
    SetSeqAndFilterIndex(seq.GetVDBSeqIndex(), filter_index);
}


CSNPBlobId::~CSNPBlobId(void)
{
}


bool CSNPBlobId::IsValidNAIndex(size_t na_index)
{
    return na_index > 0 && na_index < 1000000000;
}


bool CSNPBlobId::IsValidNAVersion(size_t na_version)
{
    return na_version >= kNAVersionMin && na_version <= kNAVersionMax;
}


bool CSNPBlobId::IsValidSeqIndex(size_t seq_index)
{
    return seq_index < kSeqIndexCount;
}


bool CSNPBlobId::IsValidFilterIndex(size_t filter_index)
{
    return filter_index < kFilterIndexCount;
}


void CSNPBlobId::SetNAIndex(size_t na_index)
{
    _ASSERT(IsValidNAIndex(na_index));
    m_NAIndex = Uint4(na_index);
}


bool CSNPBlobId::IsValidSubSat(void) const
{
    return IsValidNAIndex(GetNAIndex());
}


int CSNPBlobId::GetSatBase(void) const
{
    return IsPrimaryTrack()? kSNPSatPrimary: kSNPSatBase;
}


int CSNPBlobId::GetSubSatBase(void) const
{
    return IsPrimaryTrackGraph()? kSNPSubSatGraph: 0;
}


void CSNPBlobId::SetNAVersion(size_t na_version)
{
    _ASSERT(IsValidNAVersion(na_version));
    m_NAVersion = Uint2(na_version);
}


bool CSNPBlobId::IsSatId(void) const
{
    return m_NAIndex != 0;
}


Int4 CSNPBlobId::GetSat(void) const
{
    _ASSERT(IsValidNAVersion(GetNAVersion()));
    return Int4(GetSatBase() + GetNAVersion());
}


Int4 CSNPBlobId::GetSubSat(void) const
{
    _ASSERT(IsValidNAIndex(GetNAIndex()));
    return Int4(GetSubSatBase() + GetNAIndex());
}


Int4 CSNPBlobId::GetSatKey(void) const
{
    _ASSERT(IsValidSeqIndex(GetSeqIndex()));
    _ASSERT(IsValidFilterIndex(GetFilterIndex()));
    return Int4(GetSeqIndex() + GetFilterIndex()*kSeqIndexCount);
}


bool CSNPBlobId::IsValidSat(void) const
{
    return IsValidNAVersion(GetNAVersion());
}


pair<size_t, size_t> CSNPBlobId::ParseNA(CTempString acc)
{
    pair<size_t, size_t> ret(0, 0);
    // NA123456789.1
    if ( acc.size() < 13 || acc.size() > 15 ||
         acc[0] != 'N' || acc[1] != 'A' || acc[11] != '.' ) {
        return ret;
    }
    size_t na_index = NStr::StringToNumeric<size_t>(acc.substr(2, 9),
                                                    NStr::fConvErr_NoThrow);
    if ( !IsValidNAIndex(na_index) ) {
        return ret;
    }
    size_t na_version = NStr::StringToNumeric<size_t>(acc.substr(12),
                                                      NStr::fConvErr_NoThrow);
    if ( !IsValidNAVersion(na_version) ) {
        return ret;
    }
    ret.first = na_index;
    ret.second = na_version;
    return ret;
}


string CSNPBlobId::GetSatNA(void) const
{
    CNcbiOstrstream str;
    str << "NA" << setw(9) << setfill('0') << GetNAIndex()
        << '.' << GetNAVersion();
    return CNcbiOstrstreamToString(str);
}


void CSNPBlobId::SetSatNA(CTempString acc)
{
    pair<size_t, size_t> na = ParseNA(acc);
    SetNAIndex(na.first);
    SetNAVersion(na.second);
}


void CSNPBlobId::SetSeqAndFilterIndex(size_t seq_index,
                                      size_t filter_index)
{
    _ASSERT(IsValidSeqIndex(seq_index));
    _ASSERT(IsValidFilterIndex(filter_index));
    m_SeqIndex = Uint4(seq_index);
    m_FilterIndex = Uint4(filter_index);
}


bool CSNPBlobId::IsValidSatKey(void) const
{
    return IsValidSeqIndex(GetSeqIndex()) &&
        IsValidFilterIndex(GetFilterIndex());
}


CSeq_id_Handle CSNPBlobId::GetSeqId(void) const
{
    _ASSERT(!IsSatId());
    return m_SeqId;
}


string CSNPBlobId::GetAccession(void) const
{
    if ( m_Accession.empty() ) {
        return GetSatNA();
    }
    else {
        return m_Accession;
    }
}


void CSNPBlobId::SetPrimaryTrackFeat(void)
{
    _ASSERT(!IsPrimaryTrack());
    m_IsPrimaryTrack = true;
    m_IsPrimaryTrackGraph = false;
}


void CSNPBlobId::SetPrimaryTrackGraph(void)
{
    _ASSERT(!IsPrimaryTrack());
    m_IsPrimaryTrack = true;
    m_IsPrimaryTrackGraph = true;
}


static const char kFileEnd[] = "|||";
static const char kFilterPrefixChar = '#';


static
size_t sx_ExtractFilterIndex(string& s)
{
    size_t size = s.size();
    size_t pos = size;
    while ( pos && isdigit(s[pos-1]) ) {
        --pos;
    }
    size_t num_len = size - pos;
    if ( !num_len || num_len > kFilterIndexMaxLength ||
         !pos || s[pos] == '0' || s[pos-1] != kFilterPrefixChar ) {
        return 0;
    }
    size_t index = NStr::StringToNumeric<size_t>(s.substr(pos));
    if ( !CSNPBlobId::IsValidFilterIndex(index) ) {
        return 0;
    }
    // internally filter index is zero-based, but in accession it's one-based
    --index;
    // remove filter index from accession
    s.resize(pos - 1);
    return index;
}


static
string sx_AddFilterIndex(const string& s, size_t filter_index)
{
    CNcbiOstrstream str;
    str << s << kFilterPrefixChar << (filter_index+1);
    return CNcbiOstrstreamToString(str);
}


string CSNPBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    if ( IsSatId() ) {
        out << GetSat() << '.' << GetSubSat() << '.' << GetSatKey();
    }
    else {
        out << m_Accession;
        out << kFilterPrefixChar << (GetFilterIndex()+1);
        out << kFileEnd << m_SeqId;
    }
    return CNcbiOstrstreamToString(out);
}


bool CSNPBlobId::FromSatString(CTempString str)
{
    if ( str.empty() || !isdigit(Uchar(str[0])) ) {
        return false;
    }

    size_t dot1 = str.find('.');
    if ( dot1 == NPOS ) {
        return false;
    }
    size_t dot2 = str.find('.', dot1+1);
    if ( dot2 == NPOS ) {
        return false;
    }
    size_t sat = NStr::StringToNumeric<size_t>(str.substr(0, dot1),
                                               NStr::fConvErr_NoThrow);
    bool is_primary_track = sat >= kSNPSatPrimary;
    size_t na_version = sat - (is_primary_track? kSNPSatPrimary: kSNPSatBase);
    if ( !IsValidNAVersion(na_version) ) {
        return false;
    }
    size_t subsat = NStr::StringToNumeric<size_t>(str.substr(dot1+1, dot2-dot1-1),
                                                  NStr::fConvErr_NoThrow);
    bool is_primary_track_graph = is_primary_track && subsat >= kSNPSubSatGraph;
    size_t na_index = subsat - (is_primary_track_graph? kSNPSubSatGraph: 0);
    if ( !IsValidNAIndex(na_index) ) {
        return false;
    }
    
    size_t satkey = NStr::StringToNumeric<size_t>(str.substr(dot2+1),
                                                  NStr::fConvErr_NoThrow);
    size_t seq_index = satkey % kSeqIndexCount;
    size_t filter_index = satkey / kSeqIndexCount;
    if ( !IsValidSeqIndex(seq_index) || !IsValidFilterIndex(filter_index) ) {
        return false;
    }

    m_NAIndex = Uint4(na_index);
    m_NAVersion = Uint2(na_version);
    m_SeqIndex = Uint4(seq_index);
    m_FilterIndex = Uint4(filter_index);
    m_IsPrimaryTrack = is_primary_track;
    m_IsPrimaryTrackGraph = is_primary_track_graph;
    m_Accession.clear();
    m_SeqId.Reset();
    
    _ASSERT(IsSatId());
    return true;
}


void CSNPBlobId::FromString(CTempString str)
{
    if ( FromSatString(str) ) {
        return;
    }
    m_NAIndex = 0;
    m_NAVersion = 0;
    m_SeqIndex = 0;
    m_FilterIndex = 0;
    m_IsPrimaryTrack = false;
    m_IsPrimaryTrackGraph = false;
    m_Accession.clear();
    m_SeqId.Reset();
    _ASSERT(!IsSatId());

    SIZE_TYPE div = str.rfind(kFileEnd);
    if ( div == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CSNPBlobId: "<<str);
    }
    m_Accession = str.substr(0, div);
    m_SeqId = CSeq_id_Handle::GetHandle(str.substr(div+strlen(kFileEnd)));
    SetSeqAndFilterIndex(0, sx_ExtractFilterIndex(m_Accession));
}


bool CSNPBlobId::operator<(const CSNPBlobId& id2) const
{
    if ( m_NAIndex != id2.m_NAIndex ) {
        return m_NAIndex < id2.m_NAIndex;
    }
    if ( m_NAVersion != id2.m_NAVersion ) {
        return m_NAVersion < id2.m_NAVersion;
    }
    if ( m_SeqIndex != id2.m_SeqIndex ) {
        return m_SeqIndex < id2.m_SeqIndex;
    }
    if ( m_FilterIndex != id2.m_FilterIndex ) {
        return m_FilterIndex < id2.m_FilterIndex;
    }
    if ( m_IsPrimaryTrack != id2.m_IsPrimaryTrack ) {
        return m_IsPrimaryTrack < id2.m_IsPrimaryTrack;
    }
    if ( m_IsPrimaryTrackGraph != id2.m_IsPrimaryTrackGraph ) {
        return m_IsPrimaryTrackGraph < id2.m_IsPrimaryTrackGraph;
    }
    if ( m_Accession != id2.m_Accession ) {
        return m_Accession < id2.m_Accession;
    }
    return m_SeqId < id2.m_SeqId;
}


bool CSNPBlobId::operator==(const CSNPBlobId& id2) const
{
    return m_NAIndex == id2.m_NAIndex &&
           m_NAVersion == id2.m_NAVersion &&
           m_SeqIndex == id2.m_SeqIndex &&
           m_FilterIndex == id2.m_FilterIndex &&
           m_IsPrimaryTrack == id2.m_IsPrimaryTrack &&
           m_IsPrimaryTrackGraph == id2.m_IsPrimaryTrackGraph &&
           m_Accession == id2.m_Accession &&
           m_SeqId == id2.m_SeqId;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CSNPDataLoader_Impl::CSNPDataLoader_Impl(
    const CSNPDataLoader::SLoaderParams& params)
    : m_FileReopenTime(GetFileReopenTimeParam()),
      m_FileRecheckTime(GetFileRecheckTimeParam()),
      m_RetryCount(GetRetryCountParam()),
      m_FoundFiles(max(params.m_VDBFiles.size(), GetGCSize()),
                   m_FileReopenTime, m_FileRecheckTime)
{
    m_DirPath = params.m_DirPath;
    m_AnnotName = params.m_AnnotName;
    m_AddPTIS = params.m_AddPTIS;
    if ( m_AddPTIS ) {
        if ( CSnpPtisClient::IsEnabled() ) {
            m_PTISClient = CSnpPtisClient::CreateClient();
        }
        else {
            ERR_POST_ONCE("CSNPDataLoader: SNP primary track is disabled due to lack of GRPC support");
            m_AddPTIS = false;
        }
    }
    
    if ( params.m_VDBFiles.empty() ) {
        if ( !m_DirPath.empty() ) {
            m_DirPath.erase();
            AddFixedFile(params.m_DirPath);
        }
    }
    for ( auto& file : params.m_VDBFiles ) {
        AddFixedFile(file);
    }
}


CSNPDataLoader_Impl::~CSNPDataLoader_Impl(void)
{
}


template<class Call>
typename std::invoke_result<Call>::type
CSNPDataLoader_Impl::CallWithRetry(Call&& call,
                                   const char* name,
                                   unsigned retry_count)
{
    if ( retry_count == 0 ) {
        retry_count = m_RetryCount;
    }
    for ( unsigned t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CBlobStateException& ) {
            // no retry
            throw;
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<"CSNPDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<"CSNPDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<"CSNPDataLoader::"<<name<<"() try "<<t<<" exception");
        }
        if ( t >= 2 ) {
            //double wait_sec = m_WaitTime.GetTime(t-2);
            double wait_sec = 1;
            LOG_POST(Warning<<"CSNPDataLoader: waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
    }
    return call();
}


void CSNPDataLoader_Impl::AddFixedFile(const string& file)
{
    CallWithRetry(bind(&CSNPDataLoader_Impl::AddFixedFileOnce, this,
                       file),
                  "AddFixedFile");
}


void CSNPDataLoader_Impl::AddFixedFileOnce(const string& file)
{
    CRef<CSNPFileInfo> info(new CSNPFileInfo(*this, file));
    string key = info->GetBaseAnnotName();
    sx_ExtractFilterIndex(key);
    if ( !m_FixedFiles.insert(TFixedFiles::value_type(key, info->m_FileName)).second ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Duplicated fixed SNP NA: "<<
                       key);
    }
    CRef<SSNPFileInfoSlot> info_slot = m_FoundFiles.GetSlot(info->m_FileName);
    info_slot->UpdateExpiration(m_FoundFiles, info->m_FileName);
    info_slot->SetObject(info);
    info->InitializeDb(*this);
}


CRef<CSNPFileInfo> CSNPDataLoader_Impl::x_GetFileInfo(const string& file)
{
    CRef<SSNPFileInfoSlot> info_slot = m_FoundFiles.GetSlot(file);
    CRef<CSNPFileInfo> info; // return info
    CRef<CSNPFileInfo> delete_info; // delete stale file info after releasing mutex
    // now open or reopen the SNP file under individual guard
    SSNPFileInfoSlot::TSlotMutex::TWriteLockGuard guard(info_slot->GetSlotMutex());
    info = info_slot->GetObject<CSNPFileInfo>();
    if ( info && info_slot->IsExpired(m_FoundFiles, file) ) {
        if ( GetDebugLevel() >= 1 ) {
            LOG_POST_X(4, Info<<"CSNPDataLoader: "
                       "Reopening SNP file expired in cache: "<<file);
        }
        info_slot->ResetObject();
        delete_info.Swap(info);
    }
    if ( !info ) {
        // make sure the file is opened
        try {
            info_slot->UpdateExpiration(m_FoundFiles, file);
            info = new CSNPFileInfo(*this, file);
            info_slot->SetObject(info);
        }
        catch ( CSraException& exc ) {
            if ( exc.GetErrCode() == exc.eNotFoundDb ||
                 exc.GetErrCode() == exc.eProtectedDb ) {
                // no such SNP NA accession
                return null;
            }
            else {
                // problem in VDB or SNP reader
                throw;
            }
        }
    }
    info->InitializeDb(*this);
    return info;
}


CRef<CSNPFileInfo> CSNPDataLoader_Impl::GetFixedFile(const string& acc)
{
    TFixedFiles::iterator it = m_FixedFiles.find(acc);
    if ( it == m_FixedFiles.end() ) {
        return null;
    }
    return x_GetFileInfo(it->second);
}


CRef<CSNPFileInfo> CSNPDataLoader_Impl::FindFile(const string& acc)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic accessions
        return null;
    }
    return x_GetFileInfo(acc);
}


CRef<CSNPFileInfo>
CSNPDataLoader_Impl::GetFileInfo(const string& acc)
{
    if ( !m_FixedFiles.empty() ) {
        return GetFixedFile(acc);
    }
    else {
        return FindFile(acc);
    }
}


CRef<CSNPFileInfo>
CSNPDataLoader_Impl::GetFileInfo(const CSNPBlobId& blob_id)
{
    return GetFileInfo(blob_id.GetAccession());
}


CRef<CSNPSeqInfo>
CSNPDataLoader_Impl::GetSeqInfo(const CSNPBlobId& blob_id)
{
    CRef<CSNPSeqInfo> info = GetFileInfo(blob_id)->GetSeqInfo(blob_id);
    _ASSERT(*info->GetBlobId() == blob_id);
    return info;
}


CTSE_LoadLock CSNPDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                               const CSNPBlobId& blob_id)
{
    return CallWithRetry(bind(&CSNPDataLoader_Impl::GetBlobByIdOnce, this,
                              data_source, cref(blob_id)),
                         "GetBlobById");
}


CTSE_LoadLock CSNPDataLoader_Impl::GetBlobByIdOnce(CDataSource* data_source,
                                                   const CSNPBlobId& blob_id)
{
    CDataLoader::TBlobId loader_blob_id(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(loader_blob_id);
    if ( !load_lock.IsLoaded() ) {
        CStopWatch sw(CStopWatch::eStart);
        LoadBlob(blob_id, load_lock);
        load_lock.SetLoaded();
    }
    return load_lock;
}


CDataLoader::TTSE_LockSet
CSNPDataLoader_Impl::GetRecords(CDataSource* data_source,
                                 const CSeq_id_Handle& idh,
                                 CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    // SNPs are available by NA accession only, see GetOrphanAnnotRecords()
    return locks;
}


static string s_GetAccVer(const CSeq_id_Handle& id)
{
    if ( !id ) {
        return string();
    }
    if ( auto seq_id = id.GetSeqId() ) {
        if ( const CTextseq_id* text_id = seq_id->GetTextseq_Id() ) {
            if ( text_id->IsSetAccession() && !text_id->GetAccession().empty() &&
                 text_id->IsSetVersion() && text_id->GetVersion() > 0 ) {
                // fully qualified text id, no more information is necessary
                return text_id->GetAccession()+'.'+NStr::NumericToString(text_id->GetVersion());
            }
        }
    }
    return string();
}


CDataLoader::TTSE_LockSet
CSNPDataLoader_Impl::GetOrphanAnnotRecords(CDataSource* ds,
                                           const CSeq_id_Handle& id,
                                           const SAnnotSelector* sel,
                                           CDataLoader::TProcessedNAs* processed_nas)
{
    return CallWithRetry(bind(&CSNPDataLoader_Impl::GetOrphanAnnotRecordsOnce, this,
                              ds, cref(id), sel, processed_nas),
                         "GetOrphanAnnotRecords");
}


CDataLoader::TTSE_LockSet
CSNPDataLoader_Impl::GetOrphanAnnotRecordsOnce(CDataSource* ds,
                                               const CSeq_id_Handle& id,
                                               const SAnnotSelector* sel,
                                               CDataLoader::TProcessedNAs* processed_nas)
{
    CDataLoader::TTSE_LockSet locks;
    // implicitly load NA accessions
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        const SAnnotSelector::TNamedAnnotAccessions& accs =
            sel->GetNamedAnnotAccessions();
        if ( m_FixedFiles.empty() ) {
            auto accs_size = accs.size();
            if ( m_FoundFiles.get_size_limit() < accs_size ) {
                CMutexGuard guard(m_FoundFiles.GetCacheMutex());
                if ( m_FoundFiles.get_size_limit() < accs_size ) {
                    // increase VDB cache size
                    m_FoundFiles.set_size_limit(accs_size+GetGCSize());
                }
            }
        }
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it, accs ) {
            if ( m_AddPTIS && it->first == "SNP" ) {
                auto scale_limit = sel->GetSNPScaleLimit();
                if (scale_limit == CSeq_id::eSNPScaleLimit_Default) {
                    scale_limit = CSNPDataLoader::GetSNP_Scale_Limit();
                }
                if (!id.IsAllowedSNPScaleLimit(scale_limit)) continue;

                // default SNP track
                string acc_ver = s_GetAccVer(id);
                if ( !acc_ver.empty() ) {
                    // add default SNP track
                    if ( GetDebugLevel() >= eDebug_resolve ) {
                        LOG_POST_X(13, Info<<"CSNPDataLoader:PTIS: resolving "<<acc_ver);
                    }
                    string na_acc = m_PTISClient->GetPrimarySnpTrackForAccVer(acc_ver);
                    if ( GetDebugLevel() >= eDebug_resolve ) {
                        LOG_POST_X(13, Info<<"CSNPDataLoader:PTIS: "<<acc_ver<<" primary SNP track is "<<na_acc);
                    }
                    if ( !na_acc.empty() ) {
                        size_t filter_index = sx_ExtractFilterIndex(na_acc);
                        if ( CRef<CSNPFileInfo> info = GetFileInfo(na_acc) ) {
                            if ( CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id) ) {
                                seq->SetFilterIndex(filter_index);
                                {
                                    auto blob_id = seq->GetBlobId();
                                    blob_id->SetPrimaryTrackFeat();
                                    locks.insert(GetBlobById(ds, *blob_id));
                                }
                                {
                                    auto blob_id = seq->GetBlobId();
                                    blob_id->SetPrimaryTrackGraph();
                                    locks.insert(GetBlobById(ds, *blob_id));
                                }
                            }
                        }
                    }
                }
                CDataLoader::SetProcessedNA(it->first, processed_nas);
                continue;
            }
            string acc = it->first;
            size_t filter_index = sx_ExtractFilterIndex(acc);
            if ( filter_index == 0 && acc.size() == it->first.size() ) {
                // filter specification is required
                continue;
            }
            if ( CRef<CSNPFileInfo> info = GetFileInfo(acc) ) {
                CDataLoader::SetProcessedNA(it->first, processed_nas);
                if ( CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id) ) {
                    seq->SetFilterIndex(filter_index);
                    locks.insert(GetBlobById(ds, *seq->GetBlobId()));
                }
            }
        }
    }
    return locks;
}


void CSNPDataLoader_Impl::LoadBlob(const CSNPBlobId& blob_id,
                                   CTSE_LoadLock& load_lock)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= eDebug_load ) {
        LOG_POST_X(5, Info<<"CSNPDataLoader::LoadBlob("<<blob_id<<")");
        sw.Start();
    }
    GetSeqInfo(blob_id)->LoadAnnotBlob(load_lock);
    if ( GetDebugLevel() >= eDebug_load_time ) {
        LOG_POST_X(6, Info<<"CSNPDataLoader::LoadBlob("<<blob_id<<")"
                 " loaded in "<<sw.Elapsed());
    }
}


void CSNPDataLoader_Impl::GetChunk(const CSNPBlobId& blob_id,
                                   CTSE_Chunk_Info& chunk)
{
    CallWithRetry(bind(&CSNPDataLoader_Impl::GetChunkOnce, this,
                       cref(blob_id), ref(chunk)),
                  "GetChunk");
}


void CSNPDataLoader_Impl::GetChunkOnce(const CSNPBlobId& blob_id,
                                       CTSE_Chunk_Info& chunk_info)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= eDebug_load ) {
        LOG_POST_X(7, Info<<"CSNPDataLoader::"
                   "LoadChunk("<<blob_id<<", "<<chunk_info.GetChunkId()<<")");
        sw.Start();
    }
    GetSeqInfo(blob_id)->LoadAnnotChunk(chunk_info);
    if ( GetDebugLevel() >= eDebug_load_time ) {
        LOG_POST_X(8, Info<<"CSNPDataLoader::"
                   "LoadChunk("<<blob_id<<", "<<chunk_info.GetChunkId()<<")"
                   " loaded in "<<sw.Elapsed());
    }
}


CObjectManager::TPriority CSNPDataLoader_Impl::GetDefaultPriority(void) const
{
    CObjectManager::TPriority priority = CObjectManager::kPriority_Replace;
    if ( m_FixedFiles.empty() ) {
        // implicit loading data loader has lower priority by default
        priority += 1;
    }
    return priority;
}


CSNPDataLoader::TAnnotNames
CSNPDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    TAnnotNames names;
    names.push_back(m_AnnotName);
    return names;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPFileInfo
/////////////////////////////////////////////////////////////////////////////


CSNPFileInfo::CSNPFileInfo(CSNPDataLoader_Impl& impl,
                           const string& acc)
{
    x_Initialize(impl, acc);
}


void CSNPFileInfo::x_Initialize(CSNPDataLoader_Impl& impl,
                                const string& csra)
{
    m_FileName = csra;
    sx_ExtractFilterIndex(m_FileName);
    m_RemainingOpenRetries = impl.m_RetryCount;
    m_IsValidNA = CSNPBlobId::IsValidNA(m_FileName);
    m_Accession = m_FileName;
    if ( !m_IsValidNA ) {
        // remove directory part, if any
        SIZE_TYPE sep = m_Accession.find_last_of("/\\");
        if ( sep != NPOS ) {
            m_Accession.erase(0, sep+1);
        }
    }
    m_AnnotName = impl.m_AnnotName;
    if ( m_AnnotName.empty() ) {
        m_AnnotName = m_Accession;
    }
}


void CSNPFileInfo::InitializeDb(CSNPDataLoader_Impl& impl)
{
    if ( !m_SNPDb && m_RemainingOpenRetries > 0 ) {
        CStopWatch sw;
        if ( GetDebugLevel() >= eDebug_open ) {
            LOG_POST_X(1, Info <<
                       "CSNPDataLoader("<<m_FileName<<")");
            sw.Start();
        }
        m_SNPDb = CSNPDb(impl.m_Mgr, m_FileName);
        if ( GetDebugLevel() >= eDebug_open_time ) {
            LOG_POST_X(2, Info <<
                       "CSNPDataLoader("<<m_FileName<<")"
                       " opened VDB in "<<sw.Elapsed());
        }
    }
}


string CSNPFileInfo::GetSNPAnnotName(size_t filter_index) const
{
    return sx_AddFilterIndex(GetBaseAnnotName(), filter_index);
}


void CSNPFileInfo::GetPossibleAnnotNames(TAnnotNames& names) const
{
    for ( CSNPDbTrackIterator it(m_SNPDb); it; ++it ) {
        names.push_back(CAnnotName(GetSNPAnnotName(it.GetVDBTrackIndex())));
    }
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(const CSeq_id_Handle& seq_id)
{
    CRef<CSNPSeqInfo> ret;
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_id);
    if ( seq_it ) {
        ret = new CSNPSeqInfo(this, seq_it);
    }
    return ret;
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(size_t seq_index)
{
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_index);
    _ASSERT(seq_it);
    CRef<CSNPSeqInfo> ret(new CSNPSeqInfo(this, seq_it));
    return ret;
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(const CSNPBlobId& blob_id)
{
    CRef<CSNPSeqInfo> ret;
    if ( blob_id.IsSatId() ) {
        ret = GetSeqInfo(blob_id.GetSeqIndex());
    }
    else {
        ret = GetSeqInfo(blob_id.GetSeqId());
    }
    if ( ret ) {
        ret->SetFromBlobId(blob_id);
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPSeqInfo
/////////////////////////////////////////////////////////////////////////////


CSNPSeqInfo::CSNPSeqInfo(CSNPFileInfo* file,
                         const CSNPDbSeqIterator& it)
    : m_File(file),
      m_SeqIndex(it.GetVDBSeqIndex()),
      m_FilterIndex(0),
      m_IsPrimaryTrack(false),
      m_IsPrimaryTrackGraph(false)
{
    if ( !file->IsValidNA() ) {
        m_SeqId = it.GetSeqIdHandle();
    }
}


CRef<CSNPBlobId> CSNPSeqInfo::GetBlobId(void) const
{
    CRef<CSNPBlobId> blob_id;
    _ASSERT(m_File);
    if ( !m_SeqId ) {
        blob_id = new CSNPBlobId(*m_File, m_SeqIndex, m_FilterIndex);
    }
    else {
        blob_id = new CSNPBlobId(*m_File, m_SeqId, m_FilterIndex);
    }
    if ( m_IsPrimaryTrack ) {
        if ( m_IsPrimaryTrackGraph ) {
            blob_id->SetPrimaryTrackGraph();
        }
        else {
            blob_id->SetPrimaryTrackFeat();
        }
    }
    return blob_id;
}


void CSNPSeqInfo::SetFilterIndex(size_t filter_index)
{
    if ( !CSNPBlobId::IsValidFilterIndex(filter_index) ) {
        filter_index = 0;
    }
    m_FilterIndex = filter_index;
}


void CSNPSeqInfo::SetFromBlobId(const CSNPBlobId& blob_id)
{
    SetFilterIndex(blob_id.GetFilterIndex());
    m_IsPrimaryTrack = blob_id.IsPrimaryTrack();
    m_IsPrimaryTrackGraph = blob_id.IsPrimaryTrackGraph();
}


CSNPDbSeqIterator
CSNPSeqInfo::GetSeqIterator(void) const
{
    CSNPDbSeqIterator it;
    if ( !m_SeqId ) {
        it = CSNPDbSeqIterator(*m_File, m_SeqIndex);
    }
    else {
        it = CSNPDbSeqIterator(*m_File, m_SeqId);
    }
    if ( m_FilterIndex ) {
        it.SetTrack(CSNPDbTrackIterator(*m_File, m_FilterIndex));
    }
    return it;
}


string CSNPSeqInfo::GetAnnotName(void) const
{
    // primary SNP track features have hard-coded name from EADB
    if ( m_IsPrimaryTrack ) {
        return "SNP";
    }
    else {
        return m_File->GetSNPAnnotName(m_FilterIndex);
    }
}


void CSNPSeqInfo::LoadAnnotBlob(CTSE_LoadLock& load_lock)
{
    CSNPDbSeqIterator it = GetSeqIterator();
    string base_name = GetAnnotName();
    CSNPDbSeqIterator::TFlags flags = CSNPDbSeqIterator::fDefaultFlags;
    if ( m_IsPrimaryTrack ) {
        // primary track has overvew graph in a separate TSE
        if ( m_IsPrimaryTrackGraph ) {
            flags |= CSNPDbSeqIterator::fNoSNPFeat;
        }
        else {
            flags |= CSNPDbSeqIterator::fOnlySNPFeat;
        }
    }
    if ( IsSplitEnabled() ) {
        auto split = it.GetSplitInfoAndVersion(base_name, flags);
        load_lock->GetSplitInfo().SetSplitVersion(split.second);
        if ( GetDebugLevel() >= eDebug_data ) {
            LOG_POST_X(6, Info<<"CSNPDataLoader::LoadAnnotBlob("<<*GetBlobId()<<"):"
                       << " SV=" << split.second
                       << " " << MSerial_AsnText << *split.first);
        }
        CSplitParser::Attach(*load_lock, *split.first);
    }
    else {
        auto entry = it.GetEntry(base_name, flags);
        if ( GetDebugLevel() >= eDebug_data ) {
            LOG_POST_X(6, Info<<"CSNPDataLoader::LoadAnnotBlob("<<*GetBlobId()<<"): "
                       << MSerial_AsnText << *entry);
        }
        load_lock->SetSeq_entry(*entry);
    }
}


void CSNPSeqInfo::LoadAnnotChunk(CTSE_Chunk_Info& chunk_info)
{
    int chunk_id = chunk_info.GetChunkId();
    string base_name = GetAnnotName();
    CSNPDbSeqIterator it = GetSeqIterator();
    auto split_version = chunk_info.GetSplitInfo().GetSplitVersion();
    auto chunk = it.GetChunkForVersion(base_name, chunk_id, split_version);
    if ( GetDebugLevel() >= eDebug_data ) {
        LOG_POST_X(6, Info<<"CSNPDataLoader::LoadAnnotChunk("<<*GetBlobId()<<", "<<chunk_id<<"):"
                   << " SV=" << split_version
                   << " " << MSerial_AsnText << *chunk);
    }
    CSplitParser::Load(chunk_info, *chunk);
    chunk_info.SetLoaded();
}


END_SCOPE(objects)
END_NCBI_SCOPE
