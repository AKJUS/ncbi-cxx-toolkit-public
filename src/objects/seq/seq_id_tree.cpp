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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
*/

#include <ncbi_pch.hpp>
#include <objects/misc/error_codes.hpp>
#include <corelib/ncbi_param.hpp>
#include "seq_id_tree.hpp"
#include <objects/seq/seq_id_mapper.hpp>
#include <common/ncbi_sanitizers.h>


#define NCBI_USE_ERRCODE_X   Objects_SeqIdMap


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//#define NCBI_SLOW_ATOMIC_SWAP
#ifdef NCBI_SLOW_ATOMIC_SWAP
DEFINE_STATIC_FAST_MUTEX(sx_GetSeqIdMutex);
#endif

////////////////////////////////////////////////////////////////////
//
//  CSeq_id_***_Tree::
//
//    Seq-id sub-type specific trees
//

CSeq_id_Which_Tree::CSeq_id_Which_Tree(CSeq_id_Mapper* mapper)
    : m_Mapper(mapper)
{
    _ASSERT(mapper);
}


CSeq_id_Which_Tree::~CSeq_id_Which_Tree(void)
{
}


bool CSeq_id_Which_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return false; // Assume no matches by default
}


void CSeq_id_Which_Tree::FindMatch(const CSeq_id_Handle& id,
                                   TSeq_id_MatchList& id_list) const
{
    id_list.insert(id); // only exact match by default
}


bool CSeq_id_Which_Tree::Match(const CSeq_id_Handle& h1,
                               const CSeq_id_Handle& h2) const
{
    if ( h1 == h2 ) {
        return true;
    }
    if ( HaveMatch(h1) ) {
        TSeq_id_MatchList id_list;
        FindMatch(h1, id_list);
        return id_list.find(h2) != id_list.end();
    }
    return false;
}


bool CSeq_id_Which_Tree::IsBetterVersion(const CSeq_id_Handle& /*h1*/,
                                         const CSeq_id_Handle& /*h2*/) const
{
    return false; // No id version by default
}


inline
CSeq_id_Info* CSeq_id_Which_Tree::CreateInfo(CSeq_id::E_Choice type)
{
    return new CSeq_id_Info(type, m_Mapper);
}


bool CSeq_id_Which_Tree::HaveReverseMatch(const CSeq_id_Handle& ) const
{
    return false; // Assume no reverse matches by default
}


void CSeq_id_Which_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                          TSeq_id_MatchList& id_list)
{
    id_list.insert(id);
    return;
}


static inline void s_AssignObject_id(CObject_id& new_id,
                                     const CObject_id& old_id)
{
    if ( old_id.IsStr() ) {
        new_id.SetStr(old_id.GetStr());
    }
    else {
        new_id.SetId(old_id.GetId());
    }
}


static inline void s_AssignDbtag(CDbtag& new_id,
                                 const CDbtag& old_id)
{
    new_id.SetDb(old_id.GetDb());
    s_AssignObject_id(new_id.SetTag(), old_id.GetTag());
}


static inline void s_AssignTextseq_id(CTextseq_id& new_tid,
                                      const CTextseq_id& old_tid)
{
    if (old_tid.IsSetAccession()) {
        new_tid.SetAccession(old_tid.GetAccession());
    }
    if (old_tid.IsSetVersion()) {
        new_tid.SetVersion(old_tid.GetVersion());
    }
    if (old_tid.IsSetName()) {
        new_tid.SetName(old_tid.GetName());
    }
    if (old_tid.IsSetRelease()) {
        new_tid.SetRelease(old_tid.GetRelease());
    }
}


static inline void s_AssignSeq_id(CSeq_id& new_id,
                                  const CSeq_id& old_id)
{
    switch (old_id.Which()) {
    case CSeq_id::e_Gi:
        new_id.SetGi(old_id.GetGi());
        break;

    case CSeq_id::e_Local:
        s_AssignObject_id(new_id.SetLocal(), old_id.GetLocal());
        break;

    case CSeq_id::e_General:
        s_AssignDbtag(new_id.SetGeneral(), old_id.GetGeneral());
        break;

    case CSeq_id::e_Other:
        s_AssignTextseq_id(new_id.SetOther(), old_id.GetOther());
        break;

    case CSeq_id::e_Genbank:
        s_AssignTextseq_id(new_id.SetGenbank(), old_id.GetGenbank());
        break;

    case CSeq_id::e_Embl:
        s_AssignTextseq_id(new_id.SetEmbl(), old_id.GetEmbl());
        break;

    case CSeq_id::e_Ddbj:
        s_AssignTextseq_id(new_id.SetDdbj(), old_id.GetDdbj());
        break;

    case CSeq_id::e_Gpipe:
        s_AssignTextseq_id(new_id.SetGpipe(), old_id.GetGpipe());
        break;

    case CSeq_id::e_Named_annot_track:
        s_AssignTextseq_id(new_id.SetNamed_annot_track(), old_id.GetNamed_annot_track());
        break;

    default:
        new_id.Assign(old_id);
        break;
    }
}


CSeq_id_Info* CSeq_id_Which_Tree::CreateInfo(const CSeq_id& id)
{
    CRef<CSeq_id> id_ref(new CSeq_id);
    s_AssignSeq_id(*id_ref, id);
    return new CSeq_id_Info(id_ref, m_Mapper);
}


void CSeq_id_Which_Tree::DropInfo(const CSeq_id_Info* info)
{
    TWriteLockGuard guard(m_TreeLock);
    if ( info->IsLocked() ) {
        _ASSERT(info->m_Seq_id_Type.load(memory_order_relaxed) != CSeq_id::e_not_set);
        return;
    }
    if ( info->m_Seq_id_Type.load(memory_order_acquire) == CSeq_id::e_not_set ) {
        _ASSERT(!info->IsLocked());
        return;
    }
    x_Unindex(info);
    _ASSERT(!info->IsLocked());
    _ASSERT(info->m_Seq_id_Type.load(memory_order_relaxed) != CSeq_id::e_not_set);
    const_cast<CSeq_id_Info*>(info)->m_Seq_id_Type.store(CSeq_id::e_not_set, memory_order_release);
}


CSeq_id_Handle CSeq_id_Which_Tree::GetGiHandle(TGi /*gi*/)
{
    NCBI_THROW(CSeq_id_MapperException, eTypeError, "Invalid seq-id type");
}


void CSeq_id_Which_Tree::Initialize(CSeq_id_Mapper* mapper,
                                    vector<CRef<CSeq_id_Which_Tree> >& v)
{
    NCBI_LSAN_DISABLE_GUARD;

    v.resize(CSeq_id::e_MaxChoice);
    v[CSeq_id::e_not_set].Reset(new CSeq_id_not_set_Tree(mapper));
    v[CSeq_id::e_Local].Reset(new CSeq_id_Local_Tree(mapper));
    v[CSeq_id::e_Gibbsq].Reset(new CSeq_id_Gibbsq_Tree(mapper));
    v[CSeq_id::e_Gibbmt].Reset(new CSeq_id_Gibbmt_Tree(mapper));
    v[CSeq_id::e_Giim].Reset(new CSeq_id_Giim_Tree(mapper));
    // These three types share the same accessions space
    CRef<CSeq_id_Which_Tree> gb(new CSeq_id_GB_Tree(mapper));
    v[CSeq_id::e_Genbank] = gb;
    v[CSeq_id::e_Embl] = gb;
    v[CSeq_id::e_Ddbj] = gb;
    v[CSeq_id::e_Pir].Reset(new CSeq_id_Pir_Tree(mapper));
    v[CSeq_id::e_Swissprot].Reset(new CSeq_id_Swissprot_Tree(mapper));
    v[CSeq_id::e_Patent].Reset(new CSeq_id_Patent_Tree(mapper));
    v[CSeq_id::e_Other].Reset(new CSeq_id_Other_Tree(mapper));
    v[CSeq_id::e_General].Reset(new CSeq_id_General_Tree(mapper));
    v[CSeq_id::e_Gi].Reset(new CSeq_id_Gi_Tree(mapper));
    // see above    v[CSeq_id::e_Ddbj] = gb;
    v[CSeq_id::e_Prf].Reset(new CSeq_id_Prf_Tree(mapper));
    v[CSeq_id::e_Pdb].Reset(new CSeq_id_PDB_Tree(mapper));
    v[CSeq_id::e_Tpg].Reset(new CSeq_id_Tpg_Tree(mapper));
    v[CSeq_id::e_Tpe].Reset(new CSeq_id_Tpe_Tree(mapper));
    v[CSeq_id::e_Tpd].Reset(new CSeq_id_Tpd_Tree(mapper));
    v[CSeq_id::e_Gpipe].Reset(new CSeq_id_Gpipe_Tree(mapper));
    v[CSeq_id::e_Named_annot_track].Reset(new CSeq_id_Named_annot_track_Tree(mapper));
}


static const size_t kMallocOverhead = 2*sizeof(void*);

static size_t sx_StringMemory(const string& s)
{
    size_t size = s.capacity();
    if ( size ) {
        if ( size + sizeof(void*) > sizeof(string) ) {
            // ref-counted
            size += sizeof(void*) + kMallocOverhead;
        }
    }
    return size;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_not_set_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_not_set_Tree::CSeq_id_not_set_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_not_set_Tree::~CSeq_id_not_set_Tree(void)
{
}


bool CSeq_id_not_set_Tree::Empty(void) const
{
    return true;
}


inline
bool CSeq_id_not_set_Tree::x_Check(const CSeq_id& id) const
{
    return id.Which() == CSeq_id::e_not_set;
}


void CSeq_id_not_set_Tree::DropInfo(const CSeq_id_Info* /*info*/)
{
}


void CSeq_id_not_set_Tree::x_Unindex(const CSeq_id_Info* /*info*/)
{
}


CSeq_id_Handle CSeq_id_not_set_Tree::FindInfo(const CSeq_id& /*id*/) const
{
    return null;
}


CSeq_id_Handle CSeq_id_not_set_Tree::FindOrCreate(const CSeq_id& /*id*/)
{
    return null;
}


void CSeq_id_not_set_Tree::FindMatch(const CSeq_id_Handle& /*id*/,
                                     TSeq_id_MatchList& /*id_list*/) const
{
    ERR_POST_X(3, Warning << "CSeq_id_Mapper::GetMatchingHandles() -- "
               "uninitialized seq-id");
}


void CSeq_id_not_set_Tree::FindMatchStr(const string& /*sid*/,
                                        TSeq_id_MatchList& /*id_list*/) const
{
}


void CSeq_id_not_set_Tree::FindReverseMatch(const CSeq_id_Handle& /*id*/,
                                            TSeq_id_MatchList& /*id_list*/)
{
    ERR_POST_X(4, Warning << "CSeq_id_Mapper::GetReverseMatchingHandles() -- "
               "uninitialized seq-id");
}


size_t CSeq_id_not_set_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
        out << "virtual, no memory" << endl;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_int_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_int_Tree::CSeq_id_int_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_int_Tree::~CSeq_id_int_Tree(void)
{
}


bool CSeq_id_int_Tree::Empty(void) const
{
    return m_IntMap.empty();
}


CSeq_id_Handle CSeq_id_int_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT(x_Check(id));
    TPacked value = x_Get(id);

    TReadLockGuard guard(m_TreeLock);
    TIntMap::const_iterator it = m_IntMap.find(value);
    if (it != m_IntMap.end()) {
        return CSeq_id_Handle(it->second);
    }
    return null;
}


CSeq_id_Handle CSeq_id_int_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    TPacked value = x_Get(id);

    TWriteLockGuard guard(m_TreeLock);
    pair<TIntMap::iterator, bool> ins =
        m_IntMap.insert(TIntMap::value_type(value, nullptr));
    if ( ins.second ) {
        ins.first->second = CreateInfo(id);
    }
    return CSeq_id_Handle(ins.first->second);
}


void CSeq_id_int_Tree::x_Unindex(const CSeq_id_Info* info)
{
    _ASSERT(x_Check(*info->GetSeqId()));
    TPacked value = x_Get(*info->GetSeqId());

    _VERIFY(m_IntMap.erase(value));
}


void CSeq_id_int_Tree::FindMatchStr(const string& sid,
                                    TSeq_id_MatchList& id_list) const
{
    TPacked value;
    try {
        value = NStr::StringToNumeric<TPacked>(sid);
    }
    catch (const CStringException& /*ignored*/) {
        // Not an integer value
        return;
    }
    TReadLockGuard guard(m_TreeLock);
    TIntMap::const_iterator it = m_IntMap.find(value);
    if (it != m_IntMap.end()) {
        id_list.insert(CSeq_id_Handle(it->second));
    }
}


size_t CSeq_id_int_Tree::Dump(CNcbiOstream& out,
                              CSeq_id::E_Choice type,
                              int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = m_IntMap.size(), elem_size = 0, extra_size = 0;
    if ( count ) {
        elem_size = sizeof(int)+sizeof(void*); // map value
        elem_size += sizeof(int)+3*sizeof(void*); // red/black tree overhead
        elem_size += sizeof(CSeq_id_Info); //
        elem_size += sizeof(CSeq_id); //
        // malloc overhead:
        // map value, CSeq_id_Info, CSeq_id
        elem_size += 3*kMallocOverhead;
    }
    size_t bytes = count*elem_size+extra_size;
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TIntMap, it, m_IntMap ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gibbsq_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gibbsq_Tree::CSeq_id_Gibbsq_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_int_Tree(mapper)
{
}


bool CSeq_id_Gibbsq_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGibbsq();
}


CSeq_id_Gibbsq_Tree::TPacked CSeq_id_Gibbsq_Tree::x_Get(const CSeq_id& id) const
{
    return INT_ID_FROM(CSeq_id::TGibbsq, id.GetGibbsq());
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gibbmt_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gibbmt_Tree::CSeq_id_Gibbmt_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_int_Tree(mapper)
{
}


bool CSeq_id_Gibbmt_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGibbmt();
}


CSeq_id_Gibbmt_Tree::TPacked CSeq_id_Gibbmt_Tree::x_Get(const CSeq_id& id) const
{
    return INT_ID_FROM(CSeq_id::TGibbmt, id.GetGibbmt());
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gi_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Gi_Info::CSeq_id_Gi_Info(CSeq_id_Mapper* mapper)
    : CSeq_id_Info(CSeq_id::e_Gi, mapper)
{
}


CConstRef<CSeq_id> CSeq_id_Gi_Info::GetPackedSeqId(TPacked gi, TVariant /*variant*/) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_Gi_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<TThis*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id = ret;
#else
    const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    const_cast<CSeq_id&>(*ret).SetGi(GI_FROM(TPacked, gi));
    return ret;
}


CSeq_id_Gi_Tree::CSeq_id_Gi_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper),
      m_ZeroInfo(0),
      m_SharedInfo(0)
{
}


CSeq_id_Gi_Tree::~CSeq_id_Gi_Tree(void)
{
}


bool CSeq_id_Gi_Tree::Empty(void) const
{
    return true;
}


inline
bool CSeq_id_Gi_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGi();
}


inline
TGi CSeq_id_Gi_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGi();
}


void CSeq_id_Gi_Tree::x_Unindex(const CSeq_id_Info* info)
{
    if ( info == m_SharedInfo ) {
        m_SharedInfo = 0;
    }
    else if ( info == m_ZeroInfo ) {
        m_ZeroInfo = 0;
    }
}


CSeq_id_Handle CSeq_id_Gi_Tree::GetGiHandle(TGi gi)
{
    if ( gi != ZERO_GI ) {
        TWriteLockGuard guard(m_TreeLock);
        if ( !m_SharedInfo ) {
            m_SharedInfo = new CSeq_id_Gi_Info(m_Mapper);
        }
        return CSeq_id_Handle(m_SharedInfo, GI_TO(TPacked, gi));
    }
    else {
        TWriteLockGuard guard(m_TreeLock);
        if ( !m_ZeroInfo ) {
            CRef<CSeq_id> zero_id(new CSeq_id);
            zero_id->SetGi(ZERO_GI);
            m_ZeroInfo = CreateInfo(*zero_id);
        }
        return CSeq_id_Handle(m_ZeroInfo);
    }
}


CSeq_id_Handle CSeq_id_Gi_Tree::FindInfo(const CSeq_id& id) const
{
    CSeq_id_Handle ret;
    _ASSERT(x_Check(id));
    TPacked gi = GI_TO(TPacked, x_Get(id));
    TReadLockGuard guard(m_TreeLock);
    if ( gi ) {
        if ( m_SharedInfo ) {
            ret = CSeq_id_Handle(m_SharedInfo, gi);
        }
    }
    else if ( m_ZeroInfo ) {
        ret = CSeq_id_Handle(m_ZeroInfo);
    }
    return ret;
}


CSeq_id_Handle CSeq_id_Gi_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    return GetGiHandle(x_Get(id));
}


void CSeq_id_Gi_Tree::FindMatchStr(const string& sid,
                                   TSeq_id_MatchList& id_list) const
{
    TPacked gi;
    try {
        gi = NStr::StringToNumeric<TPacked>(sid);
    }
    catch (const CStringException& /*ignored*/) {
        // Not an integer value
        return;
    }
    if (gi) {
        id_list.insert(CSeq_id_Handle(m_SharedInfo, gi));
    }
    else if ( m_ZeroInfo ) {
        id_list.insert(CSeq_id_Handle(m_ZeroInfo));
    }
}


size_t CSeq_id_Gi_Tree::Dump(CNcbiOstream& out,
                             CSeq_id::E_Choice type,
                             int details) const
{
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
        out << "virtual, small constant memory";
        out << endl;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Textseq_Tree
/////////////////////////////////////////////////////////////////////////////


NCBI_PARAM_DECL(bool, OBJECTS, PACK_TEXTID);
NCBI_PARAM_DEF_EX(bool, OBJECTS, PACK_TEXTID, true,
                  eParam_NoThread, OBJECTS_PACK_TEXTID);
static inline bool s_PackTextidEnabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJECTS, PACK_TEXTID)> value;
    return value->Get();
}

NCBI_PARAM_DECL(bool, OBJECTS, PACK_GENERAL);
NCBI_PARAM_DEF_EX(bool, OBJECTS, PACK_GENERAL, true,
                  eParam_NoThread, OBJECTS_PACK_GENERAL);
static inline bool s_PackGeneralEnabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJECTS, PACK_GENERAL)> value;
    return value->Get();
}

static inline
void s_RestoreNumber(string& str, size_t pos, size_t len, TIntId number)
{
    char* start = &str[pos];
    char* ptr = start + len;
    while ( number ) {
        *--ptr = (char)('0' + number % 10);
        number /= 10;
    }
    while ( ptr > start ) {
        *--ptr = '0';
    }
}

static inline
TIntId s_ParseNumber(const string& str, size_t pos, size_t len)
{
    TIntId number = 0;
    for ( size_t i = pos; i < pos+len; ++i ) {
        number = number * 10 + (str[i]-'0');
    }
    return number;
}


static inline
CSeq_id_Handle::TVariant s_RestoreCaseVariant(string& str, size_t len,
                                              CSeq_id_Handle::TVariant variant)
{
    for ( size_t i = 0; variant && i != len; ++i ) {
        int c = Uint1(str[i]);
        if ( isalpha(c) ) {
            if ( variant & 1 ) {
                // flip case
                if ( islower(c) ) {
                    c = toupper(c);
                }
                else {
                    c = tolower(c);
                }
                str[i] = c;
            }
            variant >>= 1;
        }
    }
    return variant;
}


static inline
CSeq_id_Handle::TVariant s_RestoreCaseVariant(string& str, CSeq_id_Handle::TVariant variant)
{
    return s_RestoreCaseVariant(str, str.size(), variant);
}


static inline
pair<CSeq_id_Handle::TVariant, CSeq_id_Handle::TVariant>
s_ParseCaseVariant(CTempString ref, const char* str,
                   CSeq_id_Handle::TVariant bit)
{
    CSeq_id_Handle::TVariant variant = 0;
    for ( size_t i = 0; bit && i != ref.size(); ++i ) {
        int cr = Uint1(ref[i]);
        if ( !isalpha(cr) ) {
            continue;
        }
        int cs = Uint1(str[i]);
        if ( cs != cr ) {
            _ASSERT((isupper(cs) && tolower(cs) == cr) ||
                    (islower(cs) && toupper(cs) == cr));
            variant |= bit;
        }
        bit <<= 1;
    }
    return make_pair(variant, bit);
}


static inline
pair<CSeq_id_Handle::TVariant, CSeq_id_Handle::TVariant>
s_ParseCaseVariant(CTempString ref, const string& str,
                   CSeq_id_Handle::TVariant bit = 1)
{
    _ASSERT(ref.size() <= str.size());
    return s_ParseCaseVariant(ref, str.data(), bit);
}


static inline
void s_RestoreNumberAndCaseVariant(string& str, size_t pos, size_t len, TIntId number,
                                   CSeq_id_Handle::TVariant variant)
{
    s_RestoreNumber(str, pos, len, number);
    if ( variant ) {
        s_RestoreCaseVariant(str, pos, variant);
    }
}


CSeq_id_Textseq_Info::CSeq_id_Textseq_Info(CSeq_id::E_Choice type,
                                           CSeq_id_Mapper* mapper,
                                           const TKey& key)
    : CSeq_id_Info(type, mapper),
      m_Key(key)
{
}


CSeq_id_Textseq_Info::~CSeq_id_Textseq_Info(void)
{
}


CSeq_id_Textseq_Info::TKey
CSeq_id_Textseq_Info::ParseAcc(const string& acc,
                               const TVersion* ver)
{
    TKey key;
    size_t len = acc.size(), prefix_len = len, most_significant = NPOS;
    while ( prefix_len ) {
        char c = acc[--prefix_len];
        if ( c >= '1' && c <= '9' ) {
            most_significant = prefix_len;
        }
        else if ( c != '0' ) {
            ++prefix_len;
            break;
        }
    }
    if ( most_significant == NPOS ) {
        return key;
    }
    size_t acc_digits = len - prefix_len, real_digits = len - most_significant;
    if ( acc_digits < 2 || acc_digits > 12 ||
         real_digits > 9 || acc_digits*2 < prefix_len ) {
        return key;
    }
    if ( prefix_len <= 4 ) {
        // good
    }
    else if ( prefix_len == 3 ) {
        if ( (acc[0] != 'N' && acc[0] != 'Y') ||
             (acc[1] != 'P' && acc[1] != 'C') ||
             (acc[2] != '_') ) {
            return key;
        }
    }
    else {
        return key;
    }
    if ( acc_digits > 6 && real_digits < acc_digits ) {
        acc_digits = max(size_t(6), real_digits);
        prefix_len = len - acc_digits;
    }
    if ( prefix_len > key.kMaxPrefixLen ) {
        return key;
    }
    key.m_PrefixLen = Uint1(prefix_len);
    memcpy(key.m_PrefixBuf, acc.data(), prefix_len);
    unsigned hash = 0;
    for ( size_t i = 0; i < 3 && i < prefix_len; ++i ) {
        hash = (hash << 8) | toupper(key.m_PrefixBuf[i] & 0xff);
    }
    hash = (hash << 8) | unsigned(acc_digits << 1);
    key.m_Hash = hash;
    if ( ver ) {
        key.SetVersion(*ver);
    }
    return key;
}


void CSeq_id_Textseq_Info::RestoreAccession(string& acc, TPacked param, TVariant variant) const
{
    acc = GetAccPrefix();
    acc.resize(acc.size() + GetAccDigits(), '0');
    s_RestoreNumberAndCaseVariant(acc, GetAccPrefix().size(), GetAccDigits(), param, variant);
}


void CSeq_id_Textseq_Info::Restore(CTextseq_id& id, TPacked param, TVariant variant) const
{
    if ( !id.IsSetAccession() ) {
        id.SetAccession(GetAccPrefix());
        string& acc = id.SetAccession();
        acc.resize(acc.size() + GetAccDigits(), '0');
        if ( IsSetVersion() ) {
            id.SetVersion(GetVersion());
        }
    }
    s_RestoreNumberAndCaseVariant(id.SetAccession(),
                                  GetAccPrefix().size(), GetAccDigits(), param, variant);
}


inline
CSeq_id_Textseq_Info::TPacked
CSeq_id_Textseq_Info::Pack(const TKey& key, const string& acc)
{
    return s_ParseNumber(acc, key.GetPrefixLen(), key.GetAccDigits());
}


inline
CSeq_id_Textseq_Info::TPacked
CSeq_id_Textseq_Info::Pack(const TKey& key, const CTextseq_id& tid)
{
    return Pack(key, tid.GetAccession());
}


inline
CSeq_id_Info::TVariant
CSeq_id_Textseq_Info::ParseCaseVariant(const CSeq_id_Info* info, const string& acc)
{
    return s_ParseCaseVariant(info->GetSeqId()->GetTextseq_Id()->GetAccession(), acc).first;
}


inline
CSeq_id_Info::TVariant
CSeq_id_Textseq_Info::TKey::ParseCaseVariant(const string& acc) const
{
    return s_ParseCaseVariant(GetAccPrefix(), acc).first;
}


CConstRef<CSeq_id> CSeq_id_Textseq_Info::GetPackedSeqId(TPacked param, TVariant variant) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_Textseq_Info TThis;
    if ( variant ) {
        // all non-initial case variants need fresh Seq-id to start with
        ret = new CSeq_id;
    }
    else {
        // otherwise try to use shared Seq-id if it's not referenced anywhere else
#if defined NCBI_SLOW_ATOMIC_SWAP
        CFastMutexGuard guard(sx_GetSeqIdMutex);
        ret = m_Seq_id;
        const_cast<TThis*>(this)->m_Seq_id.Reset();
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id = ret;
#else
        const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    }
    // split accession number and version
    const_cast<CSeq_id&>(*ret).Select(GetType(), eDoNotResetVariant);
    Restore(*const_cast<CTextseq_id*>(ret->GetTextseq_Id()), param, variant);
    return ret;
}


int CSeq_id_Textseq_Info::CompareOrdered(const CSeq_id_Info& other, const CSeq_id_Handle& h_this, const CSeq_id_Handle& h_other) const
{
    if ((h_this.IsPacked() || h_this.IsSetVariant()) && (h_other.IsPacked() || h_other.IsSetVariant())) {
        auto pother = dynamic_cast<const CSeq_id_Textseq_Info*>(&other);
        if (pother) {
            string this_acc, other_acc;
            // NOTE: Comparison should ignore case, so use 0 variant.
            RestoreAccession(this_acc, h_this.GetPacked(), 0);
            pother->RestoreAccession(other_acc, h_other.GetPacked(), 0);
            if (int adiff = PNocase().Compare(this_acc, other_acc)) {
                return adiff;
            }
            if (int vdiff = IsSetVersion() - pother->IsSetVersion()) {
                return vdiff;
            }
            if ( IsSetVersion() ) {
                _ASSERT(pother->IsSetVersion());
                return GetVersion() - pother->GetVersion();
            }
        }
    }
    return CSeq_id_Info::CompareOrdered(other, h_this, h_other);
}


CSeq_id_Textseq_PlainInfo::CSeq_id_Textseq_PlainInfo(const CConstRef<CSeq_id>& seq_id,
                                                     CSeq_id_Mapper* mapper)
    : CSeq_id_Info(seq_id, mapper)
{
}


inline
CSeq_id_Info::TVariant
CSeq_id_Textseq_PlainInfo::ParseCaseVariant(const string& acc) const
{
    return s_ParseCaseVariant(m_Seq_id->GetTextseq_Id()->GetAccession(), acc).first;
}


inline
CSeq_id_Info::TVariant
CSeq_id_Textseq_PlainInfo::ParseCaseVariant(const CTextseq_id& id) const
{
    if ( !id.IsSetAccession() ) {
        return 0;
    }
    return s_ParseCaseVariant(m_Seq_id->GetTextseq_Id()->GetAccession(), id.GetAccession()).first;
}


CConstRef<CSeq_id> CSeq_id_Textseq_PlainInfo::GetPackedSeqId(TPacked packed, TVariant variant) const
{
    _ASSERT(!packed);
    _ASSERT(variant);
    CRef<CSeq_id> ret(new CSeq_id);
    s_AssignSeq_id(*ret, *m_Seq_id);
    s_RestoreCaseVariant(const_cast<CTextseq_id*>(ret->GetTextseq_Id())->SetAccession(), variant);
    return ret;
}


CSeq_id_Textseq_Tree::CSeq_id_Textseq_Tree(CSeq_id_Mapper* mapper,
                                           CSeq_id::E_Choice type)
    : CSeq_id_Which_Tree(mapper),
      m_Type(type)
{
}


CSeq_id_Textseq_Tree::~CSeq_id_Textseq_Tree(void)
{
}


bool CSeq_id_Textseq_Tree::x_Check(const CSeq_id::E_Choice& type) const
{
    return type == m_Type;
}


bool CSeq_id_Textseq_Tree::x_Check(const CSeq_id& id) const
{
    return x_Check(id.Which());
}


bool CSeq_id_Textseq_Tree::Empty(void) const
{
    return m_ByName.empty() && m_ByAcc.empty() && m_PackedMap.empty();
}


bool CSeq_id_Textseq_Tree::x_Equals(const CTextseq_id& id1,
                                    const CTextseq_id& id2)
{
    if ( id1.IsSetAccession() != id2.IsSetAccession() ) {
        return false;
    }
    if ( id1.IsSetName() != id2.IsSetName() ) {
        return false;
    }
    if ( id1.IsSetVersion() != id2.IsSetVersion() ) {
        return false;
    }
    if ( id1.IsSetRelease() != id2.IsSetRelease() ) {
        return false;
    }
    if ( id1.IsSetAccession() &&
         !NStr::EqualNocase(id1.GetAccession(), id2.GetAccession()) ) {
        return false;
    }
    if ( id1.IsSetName() &&
         !NStr::EqualNocase(id1.GetName(), id2.GetName()) ) {
        return false;
    }
    if ( id1.IsSetVersion() &&
         id1.GetVersion() != id2.GetVersion() ) {
        return false;
    }
    if ( id1.IsSetRelease() &&
         id1.GetRelease() != id2.GetRelease() ) {
        return false;
    }
    return true;
}


CSeq_id_Textseq_PlainInfo*
CSeq_id_Textseq_Tree::x_FindStrInfo(const TStringMap& str_map,
                                    const string& str,
                                    CSeq_id::E_Choice type,
                                    const CTextseq_id& tid) const
{
    for ( TStringMapCI vit = str_map.find(str);
          vit != str_map.end() && NStr::EqualNocase(vit->first, str);
          ++vit ) {
        CConstRef<CSeq_id> id = vit->second->GetSeqId();
        if ( id->Which() == type && x_Equals(tid, x_Get(*id)) ) {
            return vit->second;
        }
    }
    return 0;
}


inline
CSeq_id_Textseq_PlainInfo*
CSeq_id_Textseq_Tree::x_FindStrInfo(CSeq_id::E_Choice type,
                                    const CTextseq_id& tid) const
{
    if ( tid.IsSetAccession() ) {
        return x_FindStrInfo(m_ByAcc, tid.GetAccession(), type, tid);
    }
    else if ( tid.IsSetName() ) {
        return x_FindStrInfo(m_ByName, tid.GetName(), type, tid);
    }
    else {
        return 0;
    }
}


CSeq_id_Handle CSeq_id_Textseq_Tree::FindInfo(const CSeq_id& id) const
{
    // Note: if a record is found by accession, no name is checked
    // even if it is also set.
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    // Can not compare if no accession given
    if ( s_PackTextidEnabled() &&
         tid.IsSetAccession() && !tid.IsSetName() && !tid.IsSetRelease() ) {
        const string& acc = tid.GetAccession();
        TPackedKey key = CSeq_id_Textseq_Info::ParseAcc(acc, tid);
        if ( key ) {
            TPacked packed = CSeq_id_Textseq_Info::Pack(key, tid);
            TReadLockGuard guard(m_TreeLock);
            TPackedMap_CI it = m_PackedMap.find(key);
            if ( it == m_PackedMap.end() ) {
                return null;
            }
            return CSeq_id_Handle(it->second, packed, it->first.ParseCaseVariant(acc));
        }
    }
    TReadLockGuard guard(m_TreeLock);
    CSeq_id_Textseq_PlainInfo* info = x_FindStrInfo(id.Which(), tid);
    CSeq_id_Handle::TVariant variant = info? info->ParseCaseVariant(tid): 0;
    return CSeq_id_Handle(info, 0, variant);
}

CSeq_id_Handle CSeq_id_Textseq_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    if ( s_PackTextidEnabled() &&
         tid.IsSetAccession() && !tid.IsSetName() && !tid.IsSetRelease() ) {
        const string& acc = tid.GetAccession();
        TPackedKey key = CSeq_id_Textseq_Info::ParseAcc(acc, tid);
        if ( key ) {
            TPacked packed = CSeq_id_Textseq_Info::Pack(key, tid);
            CSeq_id_Handle::TVariant variant = 0;
            TWriteLockGuard guard(m_TreeLock);
            TPackedMap_I it = m_PackedMap.lower_bound(key);
            if ( it == m_PackedMap.end() ||
                 m_PackedMap.key_comp()(key, it->first) ) {
                CConstRef<CSeq_id_Textseq_Info> info
                    (new CSeq_id_Textseq_Info(id.Which(), m_Mapper, key));
                it = m_PackedMap.insert(it, TPackedMapValue(key, info));
            }
            else {
                variant = it->first.ParseCaseVariant(acc);
            }
            return CSeq_id_Handle(it->second, packed, variant);
        }
    }
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Textseq_PlainInfo* info = x_FindStrInfo(id.Which(), tid);
    CSeq_id_Handle::TVariant variant = 0;
    if ( !info ) {
        CRef<CSeq_id> ref_id(new CSeq_id);
        s_AssignSeq_id(*ref_id, id);
        info = new CSeq_id_Textseq_PlainInfo(ref_id, m_Mapper);
        if ( tid.IsSetAccession() ) {
            m_ByAcc.insert(TStringMapValue(tid.GetAccession(), info));
        }
        if ( tid.IsSetName() ) {
            m_ByName.insert(TStringMapValue(tid.GetName(), info));
        }
    }
    else {
        variant = info->ParseCaseVariant(tid);
    }
    return CSeq_id_Handle(info, 0, variant);
}


void CSeq_id_Textseq_Tree::x_Erase(TStringMap& str_map,
                                   const string& key,
                                   const CSeq_id_Info* info)
{
    for ( TStringMap::iterator it = str_map.find(key);
          it != str_map.end() && NStr::EqualNocase(it->first, key);
          ++it ) {
        if ( it->second == info ) {
            str_map.erase(it);
            return;
        }
    }
}


void CSeq_id_Textseq_Tree::x_Unindex(const CSeq_id_Info* info)
{
    if ( !m_PackedMap.empty() ) {
        const CSeq_id_Textseq_Info* sinfo =
            dynamic_cast<const CSeq_id_Textseq_Info*>(info);
        if ( sinfo ) {
            m_PackedMap.erase(sinfo->GetKey());
            return;
        }
    }
    CConstRef<CSeq_id> tid_id = info->GetSeqId();
    _ASSERT(x_Check(*tid_id));
    const CTextseq_id& tid = x_Get(*tid_id);
    if ( tid.IsSetAccession() ) {
        x_Erase(m_ByAcc, tid.GetAccession(), info);
    }
    if ( tid.IsSetName() ) {
        x_Erase(m_ByName, tid.GetName(), info);
    }
}


static inline
bool x_IsDefaultSwissprotRelease(const string& release)
{
    return release == "reviewed"  ||  release == "unreviewed";
}


void CSeq_id_Textseq_Tree::x_FindMatchByAcc(TSeq_id_MatchList& id_list,
                                            const string& acc,
                                            const TVersion* ver) const
{
    if ( !m_PackedMap.empty() ) {
        if ( TPackedKey key = CSeq_id_Textseq_Info::ParseAcc(acc, ver) ) {
            if ( key.IsSetVersion() ) {
                // only same version
                TPackedMap_CI it = m_PackedMap.find(key);
                if ( it != m_PackedMap.end() ) {
                    TPacked packed = CSeq_id_Textseq_Info::Pack(key, acc);
                    id_list.insert(CSeq_id_Handle(it->second, packed));
                }
            }
            else {
                // all versions
                TPacked packed = 0;
                for ( TPackedMap_CI it = m_PackedMap.lower_bound(key);
                      it != m_PackedMap.end() && it->first.SameHashNoVer(key);
                      ++it ) {
                    if ( it->first.EqualAcc(key) ) {
                        if ( packed == 0 ) {
                            packed = CSeq_id_Textseq_Info::Pack(key, acc);
                        }
                        _ASSERT(packed==CSeq_id_Textseq_Info::Pack(key, acc));
                        id_list.insert(CSeq_id_Handle(it->second, packed));
                    }
                }
            }
        }
    }

    for ( TStringMapCI vit = m_ByAcc.find(acc);
          vit != m_ByAcc.end() && NStr::EqualNocase(vit->first, acc);
          ++vit ) {
        if ( ver ) {
            CConstRef<CSeq_id> tst_id = vit->second->GetSeqId();
            const CTextseq_id& tst = x_Get(*tst_id);
            // acc.ver should match
            if ( !tst.IsSetVersion() || tst.GetVersion() != *ver ) {
                continue;
            }
        }
        id_list.insert(CSeq_id_Handle(vit->second));
    }
}


void
CSeq_id_Textseq_Tree::x_FindRevMatchByAccPacked(TSeq_id_MatchList& id_list,
                                                const string& acc,
                                                const TVersion* ver) const
{
    if ( !m_PackedMap.empty() ) {
        if ( TPackedKey key = CSeq_id_Textseq_Info::ParseAcc(acc, ver) ) {
            TPackedMap_CI it = m_PackedMap.find(key);
            if ( it != m_PackedMap.end() ) {
                TPacked packed = CSeq_id_Textseq_Info::Pack(key, acc);
                id_list.insert(CSeq_id_Handle(it->second, packed));
            }
            if ( key.IsSetVersion() ) {
                // no version too
                key.ResetVersion();
                TPackedMap_CI itm = m_PackedMap.find(key);
                if ( itm != m_PackedMap.end() ) {
                    TPacked packed = CSeq_id_Textseq_Info::Pack(key, acc);
                    id_list.insert(CSeq_id_Handle(itm->second, packed));
                }
            }
        }
    }
}


void
CSeq_id_Textseq_Tree::x_FindRevMatchByAccNonPacked(TSeq_id_MatchList& id_list,
                                                   const string& acc,
                                                   const TVersion* ver) const
{
    for ( TStringMapCI vit = m_ByAcc.find(acc);
          vit != m_ByAcc.end() && NStr::EqualNocase(vit->first, acc);
          ++vit ) {
        CConstRef<CSeq_id> tst_id = vit->second->GetSeqId();
        const CTextseq_id& tst = x_Get(*tst_id);
        if ( tst.IsSetVersion() &&
             (!ver || tst.GetVersion() != *ver) ) {
            continue;
        }
        id_list.insert(CSeq_id_Handle(vit->second));
    }
}


inline
void
CSeq_id_Textseq_Tree::x_FindRevMatchByAcc(TSeq_id_MatchList& id_list,
                                          const string& acc,
                                          const TVersion* ver) const
{
    x_FindRevMatchByAccPacked(id_list, acc, ver);
    x_FindRevMatchByAccNonPacked(id_list, acc, ver);
}


void CSeq_id_Textseq_Tree::x_FindMatchByName(TSeq_id_MatchList& id_list,
                                             const string& name,
                                             const CTextseq_id* tid,
                                             EAllowFields allow_fields) const
{
    for ( TStringMapCI vit = m_ByName.find(name);
          vit != m_ByName.end() && NStr::EqualNocase(vit->first, name);
          ++vit ) {
        if ( tid || allow_fields != eAnyFields ) {
            CConstRef<CSeq_id> tst_id = vit->second->GetSeqId();
            const CTextseq_id& tst = x_Get(*tst_id);
            if ( tid ) {
                // name.rel should match
                if ( tst.IsSetAccession() && tid->IsSetAccession() ) {
                    // both accessions are set.
                    // if they are the same - match will be found by accession,
                    // otherwise accessions are different and there is no match.
                    continue;
                }
                if ( tid->IsSetRelease() ) {
                    if ( tst.IsSetRelease()  ||
                         !(m_Type == CSeq_id::e_Swissprot &&
                           x_IsDefaultSwissprotRelease(tid->GetRelease())) ) {
                        if ( !tst.IsSetRelease() ||
                             tst.GetRelease() != tid->GetRelease() ) {
                            continue;
                        }
                    }
                }
            }
            if ( allow_fields == eOnlyName ) {
                // only name field is allowed
                if ( tst.IsSetAccession() || tst.IsSetVersion() || tst.IsSetRelease() ) {
                    continue;
                }
            }
        }
        id_list.insert(CSeq_id_Handle(vit->second));
    }
}


void CSeq_id_Textseq_Tree::x_FindRevMatchByName(TSeq_id_MatchList& /*id_list*/,
                                                const string&      /*name*/,
                                                const CTextseq_id* /*tid*/) const
{
    /*
    for ( TStringMapCI vit = m_ByName.find(name);
          vit != m_ByName.end() && NStr::EqualNocase(vit->first, name);
          ++vit ) {
        if ( tid ) {
            CConstRef<CSeq_id> tst_id = vit->second->GetSeqId();
            const CTextseq_id& tst = x_Get(*tst_id);
            // name.rel should match
            if ( tst.IsSetAccession() && tid->IsSetAccession() ) {
                // both accessions are set.
                // if they are the same - match will be found by accession,
                // otherwise accessions are different and there is no match.
                continue;
            }
            if ( tid->IsSetRelease() ) {
                if ( tst.IsSetRelease()  ||
                     !(m_Type == CSeq_id::e_Swissprot &&
                       x_IsDefaultSwissprotRelease(tid->GetRelease())) ) {
                    if ( !tst.IsSetRelease() ||
                         tst.GetRelease() != tid->GetRelease() ) {
                        continue;
                    }
                }
            }
        }
        id_list.insert(CSeq_id_Handle(vit->second));
    }
    */
}


bool CSeq_id_Textseq_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindMatch(const CSeq_id_Handle& id,
                                     TSeq_id_MatchList& id_list) const
{
    auto type = id.Which();
    bool mine = x_Check(type);
    if ( mine ) {
        id_list.insert(id);
    }
    TReadLockGuard guard(m_TreeLock);
    if ( id.IsPacked() ) {
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( !m_ByAcc.empty() ) {
            // potentially whole search
            TStringMapCI it = m_ByAcc.lower_bound(info->GetAccPrefix());
            if ( it != m_ByAcc.end() && info->GoodPrefix(it->first) ) {
                // have similar accessions
                CTextseq_id tid;
                info->Restore(tid, id.GetPacked(), id.GetVariant());
                x_FindMatchByAcc(id_list, tid.GetAccession(), &tid);
                // x_FindMatchByAcc will search packed accessions too
                return;
            }
        }
        // only packed search -> no need to decode
        if ( !mine ) { // weak matching
            TPackedMap_CI iter = m_PackedMap.find(info->GetKey());
            if ( iter != m_PackedMap.end() ) {
                id_list.insert(CSeq_id_Handle(iter->second, id.GetPacked(), id.GetVariant()));
            }
        }
        if ( !info->IsSetVersion() ) {
            // add all known versions
            const TPackedKey& key = info->GetKey();
            for ( TPackedMap_CI it = m_PackedMap.lower_bound(key);
                  it != m_PackedMap.end() && it->first.SameHashNoVer(key);
                  ++it ) {
                if ( it->first.EqualAcc(key) ) {
                    id_list.insert(CSeq_id_Handle(it->second, id.GetPacked(), id.GetVariant()));
                }
            }
        }
        // special case for PIR and PRF ids - matching accesstion to name
        // the id should have only accession and no other fields (version, name, release)
        if ( (type == CSeq_id::e_Pir || type == CSeq_id::e_Prf) &&
             !info->IsSetVersion() && // packed id doesn't have name or release already
             !m_ByName.empty() ) {
            string acc;
            info->RestoreAccession(acc, id.GetPacked(), 0); // case doesn't matter
            x_FindMatchByName(id_list, acc, nullptr, eOnlyName);
        }
    }
    else {
        CConstRef<CSeq_id> tid_id = id.GetSeqId();
        const CTextseq_id* tid = tid_id->GetTextseq_Id();
        _ASSERT(tid);
        if ( tid->IsSetAccession() ) {
            x_FindMatchByAcc(id_list, tid->GetAccession(), tid);
        }
        if ( tid->IsSetName() ) {
            x_FindMatchByName(id_list, tid->GetName(), tid);
        }
        // special case for PIR and PRF ids - matching accesstion to name
        // the id should have only accession and no other fields (version, name, release)
        if ( (type == CSeq_id::e_Pir || type == CSeq_id::e_Prf) &&
             tid->IsSetAccession() &&
             !tid->IsSetVersion() && !tid->IsSetName() && !tid->IsSetRelease() &&
             !m_ByName.empty() ) {
            x_FindMatchByName(id_list, tid->GetAccession(), nullptr, eOnlyName);
        }
    }
}


void CSeq_id_Textseq_Tree::FindMatchStr(const string& sid,
                                        TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    // ignore '.' in the search string - cut it out
    SIZE_TYPE dot = sid.find('.');
    if ( dot != NPOS ) {
        string acc = sid.substr(0, dot);
        x_FindMatchByAcc(id_list, acc);
        x_FindMatchByName(id_list, acc);
    }
    else {
        x_FindMatchByAcc(id_list, sid);
        x_FindMatchByName(id_list, sid);
    }
}


bool CSeq_id_Textseq_Tree::Match(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const
{
    return CSeq_id_Which_Tree::Match(h1, h2);
}


inline
bool CSeq_id_Textseq_Tree::x_GetVersion(int& version,
                                        const CSeq_id_Handle& id) const
{
    if ( id.IsPacked() ) {
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( !info->IsSetVersion() ) {
            version = 0;
            return false;
        }
        version = info->GetVersion();
        return true;
    }
    else {
        CConstRef<CSeq_id> id1 = id.GetSeqId();
        const CTextseq_id* tid1 = id1->GetTextseq_Id();
        if ( !tid1->IsSetVersion() ) {
            version = 0;
            return false;
        }
        version = tid1->GetVersion();
        return true;
    }
}


bool CSeq_id_Textseq_Tree::IsBetterVersion(const CSeq_id_Handle& h1,
                                           const CSeq_id_Handle& h2) const
{
    // Compare versions. If only one of the two ids has version,
    // consider it as better.
    int version1, version2;
    return x_GetVersion(version1, h1) &&
        (!x_GetVersion(version2, h2) || version1 > version2);
}


bool CSeq_id_Textseq_Tree::HaveReverseMatch(const CSeq_id_Handle&) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                            TSeq_id_MatchList& id_list)
{
    bool mine = x_Check(id.Which());
    if ( mine ) {
        id_list.insert(id);
    }
    if ( id.IsPacked() ) {
        TReadLockGuard guard(m_TreeLock);
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( !mine ) { // weak matching
            TPackedMap_CI iter = m_PackedMap.find(info->GetKey());
            if ( iter != m_PackedMap.end() ) {
                id_list.insert(CSeq_id_Handle(iter->second, id.GetPacked(), id.GetVariant()));
            }
        }
        if ( info->IsSetVersion() ) {
            TPackedKey key = info->GetKey();
            key.ResetVersion();
            TPackedMap_CI it = m_PackedMap.find(key);
            if ( it != m_PackedMap.end() ) {
                id_list.insert(CSeq_id_Handle(it->second, id.GetPacked(), id.GetVariant()));
            }
        }
        if ( !m_ByAcc.empty() ) {
            // look for non-packed variants that may have set name or revision
            string acc;
            info->RestoreAccession(acc, id.GetPacked(), id.GetVariant());
            x_FindRevMatchByAccNonPacked
                (id_list, acc, info->IsSetVersion()? &info->GetVersion(): 0);
        }
        return;
    }

    CConstRef<CSeq_id> orig_id = id.GetSeqId();
    const CTextseq_id& orig_tid = x_Get(*orig_id);

    if ( true || !mine ) { // this code should be enough
        TReadLockGuard guard(m_TreeLock);
        // search only existing accessions
        if ( orig_tid.IsSetAccession() ) {
            x_FindRevMatchByAcc(id_list, orig_tid.GetAccession(), &orig_tid);
        }
        if ( orig_tid.IsSetName() ) {
            x_FindRevMatchByName(id_list, orig_tid.GetName(), &orig_tid);
        }
        return;
    }
}


size_t CSeq_id_Textseq_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{
        size_t size = m_ByAcc.size() + m_ByName.size();
        size_t elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(string)+sizeof(void*); // map value
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CTextseq_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CTextseq_id
            elem_size += 4*kMallocOverhead;
            ITERATE ( TStringMap, it, m_ByAcc ) {
                CConstRef<CSeq_id> id_id = it->second->GetSeqId();
                const CTextseq_id& id = *id_id->GetTextseq_Id();
                extra_size += sx_StringMemory(id.GetAccession());
                if ( id.IsSetName() ) {
                    extra_size += sx_StringMemory(id.GetName());
                }
                if ( id.IsSetRelease() ) {
                    extra_size += sx_StringMemory(id.GetRelease());
                }
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<size << " handles, "<<bytes<<" bytes"<<endl;
        }
    }}
    {{
        size_t size = m_PackedMap.size(), elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(TPackedKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Textseq_Info); //
            // malloc overhead:
            // map value, CSeq_id_Textseq_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedMap, it, m_PackedMap ) {
                //extra_size += sx_StringMemory(it->first.m_Prefix);
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " " <<size << " packed handles, "<<bytes<<" bytes"<<endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TStringMap, it, m_ByAcc ) {
            CConstRef<CSeq_id> id = it->second->GetSeqId();
            out << "  " << id->AsFastaString() << endl;
        }
        ITERATE ( TPackedMap, it, m_PackedMap ) {
            out << "  packed prefix "
                << it->first.GetAccPrefix()<<"."<<it->first.m_Version << endl;
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_GB_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_GB_Tree::CSeq_id_GB_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_not_set)
{
}


bool CSeq_id_GB_Tree::x_Check(const CSeq_id::E_Choice& type) const
{
    return
        type == CSeq_id::e_Genbank  ||
        type == CSeq_id::e_Embl  ||
        type == CSeq_id::e_Ddbj;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Pir_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Pir_Tree::CSeq_id_Pir_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Pir)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Swissprot_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Swissprot_Tree::CSeq_id_Swissprot_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Swissprot)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Prf_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Prf_Tree::CSeq_id_Prf_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Prf)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpg_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpg_Tree::CSeq_id_Tpg_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Tpg)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpe_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpe_Tree::CSeq_id_Tpe_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Tpe)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpd_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpd_Tree::CSeq_id_Tpd_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Tpd)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gpipe_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gpipe_Tree::CSeq_id_Gpipe_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Gpipe)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Named_annot_track_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Named_annot_track_Tree::CSeq_id_Named_annot_track_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Named_annot_track)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Other_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Other_Tree::CSeq_id_Other_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper, CSeq_id::e_Other)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Local_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Local_Tree::CSeq_id_Local_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Local_Tree::~CSeq_id_Local_Tree(void)
{
}


bool CSeq_id_Local_Tree::Empty(void) const
{
    return m_ByStr.empty() && m_ById.empty();
}


static inline bool sx_AllDigits(const string& s)
{
    ITERATE ( string, i, s ) {
        if ( !isdigit(Uint1(*i)) ) {
            return false;
        }
    }
    return true;
}


static bool sx_ParseLocalStrId(const string& str, CObject_id::TId& id)
{
    if ( str.empty() ) {
        return false;
    }
    char first_char = str[0];
    if ( first_char != '-' && (first_char < '0' || first_char > '9') ) {
        // not a number
        return false;
    }
    CObject_id::TId value = NStr::StringToNumeric<CObject_id::TId>(str, NStr::fConvErr_NoThrow);
    if ( !value ) {
        if ( errno ) {
            // not convertible to integer
            return false;
        }
        // converted to 0
        if ( str.size() != 1 ) {
            // leading zeroes are not allowed
            return false;
        }
        // valid zero as a string
        id = 0;
        return true;
    }
    else if ( value > 0 ) {
        // non-zero positive value
        if ( first_char == '0' || first_char == '+' ) {
            // redundant '+' or leading zeroes are not allowed
            return false;
        }
        // valid positive as a string
        id = value;
        return true;
    }
    else {
        // non-zero negative value
        if ( first_char != '-' || str[1] == '0' ) {
            // leading zeroes are not allowed
            return false;
        }
        // valid negative as a string
        id = value;
        return true;
    }
}


CSeq_id_Local_Info::CSeq_id_Local_Info(const CObject_id& oid, CSeq_id_Mapper* mapper)
    : CSeq_id_Info(CSeq_id::e_Local, mapper),
      m_IsId(oid.IsId())
{
    CRef<CSeq_id> seq_id(new CSeq_id);
    CObject_id& oid2 = seq_id->SetLocal();
    if ( IsId() ) {
        m_HasMatchingId = true;
        m_MatchingId = oid.GetId();
        oid2.SetId(oid.GetId());
    }
    else {
        m_HasMatchingId = sx_ParseLocalStrId(oid.GetStr(), m_MatchingId);
        oid2.SetStr(oid.GetStr());
    }
    m_Seq_id = std::move(seq_id);
}


CSeq_id_Local_Info::~CSeq_id_Local_Info()
{
}


inline CSeq_id_Handle::TVariant
CSeq_id_Local_Info::ParseCaseVariant(const string& str) const
{
    return s_ParseCaseVariant(m_Seq_id->GetLocal().GetStr(), str).first;
}


inline CSeq_id_Handle::TVariant
CSeq_id_Local_Info::ParseCaseVariant(const CObject_id& oid) const
{
    if ( !oid.IsStr() ) {
        return 0;
    }
    return ParseCaseVariant(oid.GetStr());
}


CConstRef<CSeq_id> CSeq_id_Local_Info::GetPackedSeqId(TPacked packed, TVariant variant) const
{
    if ( !variant ) {
        return m_Seq_id;
    }
    CRef<CSeq_id> ret(new CSeq_id);
    const CObject_id& src = m_Seq_id->GetLocal();
    CObject_id& oid = ret->SetLocal();
    if ( IsId() ) {
        oid.SetId(src.GetId());
    }
    else {
        string& str = oid.SetStr();
        str = src.GetStr();
        s_RestoreCaseVariant(str, variant);
    }
    return ret;
}


CSeq_id_Local_Info* CSeq_id_Local_Tree::x_FindStrInfo(const string& str) const
{
    TByStr::const_iterator it = m_ByStr.find(str);
    if ( it != m_ByStr.end() ) {
        return it->second;
    }
    // Not found
    return 0;
}


CSeq_id_Local_Info* CSeq_id_Local_Tree::x_FindIdInfo(CObject_id::TId id) const
{
    TById::const_iterator it = m_ById.find(id);
    if ( it != m_ById.end() ) {
        return it->second;
    }
    // Not found
    return 0;
}


CSeq_id_Local_Info* CSeq_id_Local_Tree::x_FindInfo(const CObject_id& oid) const
{
    if ( oid.IsStr() ) {
        return x_FindStrInfo(oid.GetStr());
    }
    else {
        return x_FindIdInfo(oid.GetId());
    }
}


CSeq_id_Handle CSeq_id_Local_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsLocal() );
    const CObject_id& oid = id.GetLocal();
    TReadLockGuard guard(m_TreeLock);
    CSeq_id_Local_Info* info = x_FindInfo(oid);
    CSeq_id_Handle::TVariant variant = info? info->ParseCaseVariant(oid): 0;
    return CSeq_id_Handle(info, 0, variant);
}


CSeq_id_Handle CSeq_id_Local_Tree::FindOrCreate(const CSeq_id& id)
{
    const CObject_id& oid = id.GetLocal();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Local_Info*& info = oid.IsStr()? m_ByStr[oid.GetStr()]: m_ById[oid.GetId()];
    CSeq_id_Handle::TVariant variant = 0;
    if ( !info ) {
        info = new CSeq_id_Local_Info(oid, m_Mapper);
    }
    else {
        variant = info->ParseCaseVariant(oid);
    }
    return CSeq_id_Handle(info, 0, variant);
}


void CSeq_id_Local_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT(id->IsLocal());
    const CObject_id& oid = id->GetLocal();

    if ( oid.IsStr() ) {
        _VERIFY(m_ByStr.erase(oid.GetStr()));
    }
    else if ( oid.IsId() ) {
        _VERIFY(m_ById.erase(oid.GetId()));
    }
}


bool CSeq_id_Local_Tree::HaveMatch(const CSeq_id_Handle& id) const
{
    // match id <-> str(number)
    const CSeq_id_Local_Info* sinfo =
        static_cast<const CSeq_id_Local_Info*>(id.x_GetInfo());
    return sinfo->IsId() || sinfo->HasMatchingId();
}


void CSeq_id_Local_Tree::FindMatch(const CSeq_id_Handle& id,
                                   TSeq_id_MatchList& id_list) const
{
    id_list.insert(id);
    // match id <-> str(number)
    const CSeq_id_Local_Info* sinfo =
        static_cast<const CSeq_id_Local_Info*>(id.x_GetInfo());
    TReadLockGuard guard(m_TreeLock);
    if ( sinfo->IsId() ) {
        // id -> str
        if ( CSeq_id_Info* id2 = x_FindStrInfo(NStr::NumericToString(sinfo->GetMatchingId())) ) {
            id_list.insert(CSeq_id_Handle(id2));
        }
    }
    else if ( sinfo->HasMatchingId() ) {
        // str -> id
        if ( CSeq_id_Info* id2 = x_FindIdInfo(sinfo->GetMatchingId()) ) {
            id_list.insert(CSeq_id_Handle(id2));
        }
    }
}


void CSeq_id_Local_Tree::FindMatchStr(const string& str,
                                      TSeq_id_MatchList& id_list) const
{
    CObject_id::TId id;
    bool has_matching_id = sx_ParseLocalStrId(str, id);
    TReadLockGuard guard(m_TreeLock);
    // In any case search in strings
    if ( CSeq_id_Info* id2 = x_FindStrInfo(str) ) {
        id_list.insert(CSeq_id_Handle(id2));
    }
    // search possible int match
    if ( has_matching_id ) {
        if ( CSeq_id_Info* id2 = x_FindIdInfo(id) ) {
            id_list.insert(CSeq_id_Handle(id2));
        }
    }
}


size_t CSeq_id_Local_Tree::Dump(CNcbiOstream& out,
                                CSeq_id::E_Choice type,
                                int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{
        size_t size = m_ByStr.size(), elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(string)+sizeof(void*); // map value
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CObject_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CObject_id
            elem_size += 4*kMallocOverhead;
            ITERATE ( TByStr, it, m_ByStr ) {
                extra_size += sx_StringMemory(it->first);
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " " <<size << " str handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{
        size_t size = m_ById.size(), elem_size = 0;
        if ( size ) {
            elem_size = sizeof(int)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CObject_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CObject_id
            elem_size += 4*kMallocOverhead;
        }
        size_t bytes = size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<size << " int handles, "<<bytes<<" bytes" << endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TByStr, it, m_ByStr ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
        ITERATE ( TById, it, m_ById ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Id_Info
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Id_Info::CSeq_id_General_Id_Info(CSeq_id_Mapper* mapper,
                                                 const TKey& key)
    : CSeq_id_Info(CSeq_id::e_General, mapper),
      m_Key(key)
{
}


CSeq_id_General_Id_Info::~CSeq_id_General_Id_Info(void)
{
}


inline
CSeq_id_General_Id_Info::TPacked
CSeq_id_General_Id_Info::Pack(const TKey& /*key*/, const CDbtag& dbtag)
{
    TPacked id = dbtag.GetTag().GetId();
    if ( id <= 0 ) {
        if ( id == numeric_limits<TPacked>::min() ) {
            // to avoid overflow into positive values
            return 0;
        }
        --id;
    }
    return id;
}


void CSeq_id_General_Id_Info::Restore(CDbtag& dbtag, TPacked param, TVariant variant) const
{
    if ( !dbtag.IsSetDb() ) {
        dbtag.SetDb(GetDbtag());
    }
    if ( param < 0 ) {
        ++param;
    }
    dbtag.SetTag().SetId(CObject_id::TId(param));
    s_RestoreCaseVariant(dbtag.SetDb(), variant);
}


CConstRef<CSeq_id> CSeq_id_General_Id_Info::GetPackedSeqId(TPacked param, TVariant variant) const
{
    CConstRef<CSeq_id> ret;
    if ( variant ) {
        // all non-initial case variants need fresh Seq-id to start with
        ret = new CSeq_id;
    }
    else {
        // otherwise try to use shared Seq-id if it's not referenced anywhere else
        typedef CSeq_id_General_Id_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
        CFastMutexGuard guard(sx_GetSeqIdMutex);
        ret = m_Seq_id;
        const_cast<TThis*>(this)->m_Seq_id.Reset();
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id = ret;
#else
        const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    }
    Restore(const_cast<CSeq_id&>(*ret).SetGeneral(), param, variant);
    return ret;
}


int CSeq_id_General_Id_Info::CompareOrdered(const CSeq_id_Info& other, const CSeq_id_Handle& h_this, const CSeq_id_Handle& h_other) const
{
    if ((h_this.IsPacked() || h_this.IsSetVariant()) && (h_other.IsPacked() || h_other.IsSetVariant())) {
        auto pother = dynamic_cast<const CSeq_id_General_Id_Info*>(&other);
        if (pother) {
            if ( int cmp = NStr::CompareNocase(GetDbtag(), pother->GetDbtag()) ) {
                return cmp;
            }
            auto id_this = h_this.GetPacked();
            auto id_other = h_other.GetPacked();
            return id_this < id_other? -1: id_this > id_other;
        }
    }
    return CSeq_id_Info::CompareOrdered(other, h_this, h_other);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Str_Info
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Str_Info::CSeq_id_General_Str_Info(CSeq_id_Mapper* mapper,
                                                 const TKey& key)
    : CSeq_id_Info(CSeq_id::e_General, mapper),
      m_Key(key)
{
}


CSeq_id_General_Str_Info::~CSeq_id_General_Str_Info(void)
{
}


inline
CSeq_id_Handle::TVariant CSeq_id_General_Str_Info::TKey::ParseCaseVariant(const CDbtag& dbtag) const
{
    auto t1 = s_ParseCaseVariant(m_Db, dbtag.GetDb());
    const char* str = dbtag.GetTag().GetStr().data();
    auto t2 = s_ParseCaseVariant(m_StrPrefix, str, t1.second);
    auto t3 = s_ParseCaseVariant(m_StrSuffix, str+m_StrPrefix.size()+GetStrDigits(), t2.second);
    return t1.first | t2.first | t3.first;
}


CSeq_id_General_Str_Info::TKey
CSeq_id_General_Str_Info::Parse(const CDbtag& dbtag)
{
    TKey key;
    key.m_Key = 0;
    const string& str = dbtag.GetTag().GetStr();
    size_t len = str.size(), prefix_len = len, str_digits = 0;
    // find longest digit substring
    size_t cur_digits = 0, total_digits = 0;
    for ( ssize_t i = len; i >= 0; ) {
        char c = --i < 0? 0: str[i];
        if ( c >= '0' && c <= '9' ) {
            ++total_digits;
            ++cur_digits;
        }
        else {
            if ( !str_digits || cur_digits > str_digits+2 ) {
                str_digits = cur_digits;
                prefix_len = i+1;
            }
            cur_digits = 0;
        }
    }
    if ( str_digits > 9 ) {
        prefix_len += str_digits - 9;
        total_digits += str_digits - 9;
        str_digits = 9;
    }
    if ( str_digits*3 < total_digits*2 ) {
        // too many other digits
        return key;
    }
    key.m_Db = dbtag.GetDb();
    if ( prefix_len > 0 ) {
        key.m_StrPrefix = str.substr(0, prefix_len);
    }
    if ( prefix_len + str_digits < str.size() ) {
        key.m_StrSuffix = str.substr(prefix_len+str_digits);
    }
    TPacked hash = 1;
    if ( 1 ) {
        ITERATE(string, i, key.m_Db) {
            hash = hash*17 + toupper(Uint1(*i));
        }
        ITERATE ( string, i, key.m_StrPrefix ) {
            hash = hash*17 + toupper(Uint1(*i));
        }
        ITERATE(string, i, key.m_StrSuffix) {
            hash = hash*17 + toupper(Uint1(*i));
        }
    }
    else {
        for ( size_t i = 0; i < 3 && i < prefix_len; ++i ) {
            hash = (hash << 8) | toupper(key.m_StrPrefix[prefix_len-1-i] & 0xff);
        }
    }
    key.m_Key = (hash << 8) | TPacked(str_digits);
    return key;
}


inline
CSeq_id_General_Str_Info::TPacked
CSeq_id_General_Str_Info::Pack(const TKey& key,
                               const CDbtag& dbtag)
{
    TPacked id = s_ParseNumber(dbtag.GetTag().GetStr(),
                               key.m_StrPrefix.size(),
                               key.GetStrDigits());
    if ( id <= 0 ) {
        --id;
    }
    return id;
}


void CSeq_id_General_Str_Info::Restore(CDbtag& dbtag, TPacked param, TVariant variant) const
{
    if ( !dbtag.IsSetDb() ) {
        dbtag.SetDb(GetDbtag());
    }
    CObject_id& obj_id = dbtag.SetTag();
    if ( !obj_id.IsStr() ) {
        obj_id.SetStr(GetStrPrefix());
        string& str = obj_id.SetStr();
        str.resize(str.size() + GetStrDigits(), '0');
        if ( !GetStrSuffix().empty() ) {
            str += GetStrSuffix();
        }
    }
    if ( param < 0 ) {
        ++param;
    }
    s_RestoreNumber(obj_id.SetStr(), GetStrPrefix().size(), GetStrDigits(), param);
    variant = s_RestoreCaseVariant(dbtag.SetDb(), variant);
    s_RestoreCaseVariant(obj_id.SetStr(), variant);
}


CConstRef<CSeq_id> CSeq_id_General_Str_Info::GetPackedSeqId(TPacked param, TVariant variant) const
{
    CConstRef<CSeq_id> ret;
    if ( variant ) {
        // all non-initial case variants need fresh Seq-id to start with
        ret = new CSeq_id;
    }
    else {
        // otherwise try to use shared Seq-id if it's not referenced anywhere else
        typedef CSeq_id_General_Str_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
        CFastMutexGuard guard(sx_GetSeqIdMutex);
        ret = m_Seq_id;
        const_cast<TThis*>(this)->m_Seq_id.Reset();
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id = ret;
#else
        const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
        if ( !ret || !ret->ReferencedOnlyOnce() ) {
            ret.Reset(new CSeq_id);
        }
        const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    }
    Restore(const_cast<CSeq_id&>(*ret).SetGeneral(), param, variant);
    return ret;
}


string CSeq_id_General_Str_Info::x_GetStr(TPacked param) const
{
    string str = GetStrPrefix();
    str.resize(str.size() + GetStrDigits(), '0');
    if ( !GetStrSuffix().empty() ) {
        str += GetStrSuffix();
    }
    if (param < 0) ++param;
    s_RestoreNumber(str, GetStrPrefix().size(), GetStrDigits(), param);
    return str;
}


int CSeq_id_General_Str_Info::CompareOrdered(const CSeq_id_Info& other, const CSeq_id_Handle& h_this, const CSeq_id_Handle& h_other) const
{
    /*
    we cannot simply compare strings - large integers in string form must be compared numerically
    if ((h_this.IsPacked() || h_this.IsSetVariant()) && (h_other.IsPacked() || h_other.IsSetVariant())) {
        auto pother = dynamic_cast<const CSeq_id_General_Str_Info*>(&other);
        if (pother) {
            if ( int cmp = NStr::CompareNocase(GetDbtag(), pother->GetDbtag()) ) {
                return cmp;
            }
            auto str_this = x_GetStr(h_this.GetPacked());
            auto str_other = pother->x_GetStr(h_other.GetPacked());
            E_Choice type_this = x_GetIdType(str_this);
            E_Choice type_other = x_GetIdType(str_other);
            if ( int diff = type_this - type_other ) {
                return diff;
            }
            switch ( type_this ) {
            case e_Id:
                return value < value2? -1: (value > value2);
            case e_Str:
                return PNocase().Compare(str_this, str_other);
            default:
                return 0;
            }
        }
    }
    */
    return CSeq_id_Info::CompareOrdered(other, h_this, h_other);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_PlainInfo
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_PlainInfo::CSeq_id_General_PlainInfo(const CDbtag& dbid, CSeq_id_Mapper* mapper)
    : CSeq_id_Info(CSeq_id::e_General, mapper)
{
    CRef<CSeq_id> seq_id(new CSeq_id);
    s_AssignDbtag(seq_id->SetGeneral(), dbid);
    m_Seq_id = std::move(seq_id);
}


inline
CSeq_id_Handle::TVariant CSeq_id_General_PlainInfo::ParseCaseVariant(const CDbtag& dbtag) const
{
    const CDbtag& src = m_Seq_id->GetGeneral();
    if ( dbtag.GetTag().IsId() ) {
        return s_ParseCaseVariant(src.GetDb(), dbtag.GetDb()).first;
    }
    else {
        auto t1 = s_ParseCaseVariant(src.GetDb(), dbtag.GetDb());
        auto t2 = s_ParseCaseVariant(src.GetTag().GetStr(), dbtag.GetTag().GetStr(), t1.second);
        return t1.first | t2.first;
    }
}


CConstRef<CSeq_id> CSeq_id_General_PlainInfo::GetPackedSeqId(TPacked packed, TVariant variant) const
{
    if ( !variant ) {
        return m_Seq_id;
    }
    CRef<CSeq_id> id(new CSeq_id);
    CDbtag& dbtag = id->SetGeneral();
    s_AssignDbtag(dbtag, m_Seq_id->GetGeneral());
    if ( dbtag.GetTag().IsId() ) {
        s_RestoreCaseVariant(dbtag.SetDb(), variant);
    }
    else {
        variant = s_RestoreCaseVariant(dbtag.SetDb(), variant);
        s_RestoreCaseVariant(dbtag.SetTag().SetStr(), variant);
    }
    return id;
}
    
/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Tree::CSeq_id_General_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_General_Tree::~CSeq_id_General_Tree(void)
{
}


bool CSeq_id_General_Tree::Empty(void) const
{
    return m_DbMap.empty() && m_PackedIdMap.empty() && m_PackedStrMap.empty();
}


CSeq_id_General_PlainInfo* CSeq_id_General_Tree::x_FindInfo(const CDbtag& dbid) const
{
    TDbMap::const_iterator db = m_DbMap.find(dbid.GetDb());
    if (db == m_DbMap.end())
        return 0;
    const STagMap& tm = db->second;
    const CObject_id& oid = dbid.GetTag();
    if ( oid.IsStr() ) {
        STagMap::TByStr::const_iterator it = tm.m_ByStr.find(oid.GetStr());
        if (it != tm.m_ByStr.end()) {
            return it->second;
        }
    }
    else if ( oid.IsId() ) {
        STagMap::TById::const_iterator it = tm.m_ById.find(oid.GetId());
        if (it != tm.m_ById.end()) {
            return it->second;
        }
    }
    // Not found
    return 0;
}


static const size_t kMinGeneralStrDigits = 3;


CSeq_id_Handle CSeq_id_General_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsGeneral() );
    const CDbtag& dbid = id.GetGeneral();
    if ( s_PackGeneralEnabled() ) {
        switch ( dbid.GetTag().Which() ) {
        case CObject_id::e_Str:
        {
            TPackedStrKey key = CSeq_id_General_Str_Info::Parse(dbid);
            if ( key.GetStrDigits() < kMinGeneralStrDigits ) {
                break;
            }
            TPacked packed = CSeq_id_General_Str_Info::Pack(key, dbid);
            TReadLockGuard guard(m_TreeLock);
            TPackedStrMap::const_iterator it = m_PackedStrMap.find(key);
            if ( it != m_PackedStrMap.end() ) {
                return CSeq_id_Handle(it->second, packed, it->first.ParseCaseVariant(dbid));
            }
            return null;
        }
        case CObject_id::e_Id:
        {
            const string& key = dbid.GetDb();
            TPacked packed = CSeq_id_General_Id_Info::Pack(key, dbid);
            if ( packed == 0 ) {
                break;
            }
            TReadLockGuard guard(m_TreeLock);
            TPackedIdMap::const_iterator it = m_PackedIdMap.find(key);
            if ( it != m_PackedIdMap.end() ) {
                return CSeq_id_Handle(it->second, packed, s_ParseCaseVariant(it->first, dbid.GetDb()).first);
            }
            return null;
        }
        default:
            return null;
        }
    }
    TReadLockGuard guard(m_TreeLock);
    CSeq_id_General_PlainInfo* info = x_FindInfo(dbid);
    CSeq_id_Handle::TVariant variant = info? info->ParseCaseVariant(dbid): 0;
    return CSeq_id_Handle(info, 0, variant);
}


CSeq_id_Handle CSeq_id_General_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsGeneral() );
    const CDbtag& dbid = id.GetGeneral();
    if ( s_PackGeneralEnabled() ) {
        switch ( dbid.GetTag().Which() ) {
        case CObject_id::e_Str:
        {
            TPackedStrKey key = CSeq_id_General_Str_Info::Parse(dbid);
            if ( key.GetStrDigits() < kMinGeneralStrDigits ) {
                break;
            }
            TPacked packed = CSeq_id_General_Str_Info::Pack(key, dbid);
            TWriteLockGuard guard(m_TreeLock);
            TPackedStrMap::iterator it = m_PackedStrMap.find(key);
            if ( it == m_PackedStrMap.end() ) {
                CConstRef<CSeq_id_General_Str_Info> info
                    (new CSeq_id_General_Str_Info(m_Mapper, key));
                m_PackedStrMap.insert(TPackedStrMap::value_type(key, info));
                // newly created ids have case variant bits all zeros
                return CSeq_id_Handle(info, packed, 0);
            }
            else {
                // determine case variant
                CSeq_id_Handle::TVariant variant = it->first.ParseCaseVariant(dbid);
                return CSeq_id_Handle(it->second, packed, variant);
            }
        }
        case CObject_id::e_Id:
        {
            const string& key = dbid.GetDb();
            TPacked packed = CSeq_id_General_Id_Info::Pack(key, dbid);
            TWriteLockGuard guard(m_TreeLock);
            TPackedIdMap::iterator it = m_PackedIdMap.lower_bound(key);
            CSeq_id_Handle::TVariant variant = 0;
            if ( it == m_PackedIdMap.end() ||
                 !NStr::EqualNocase(it->first, key) ) {
                CConstRef<CSeq_id_General_Id_Info> info
                    (new CSeq_id_General_Id_Info(m_Mapper, key));
                it = m_PackedIdMap.insert
                    (it, TPackedIdMap::value_type(key, info));
            }
            else {
                variant = s_ParseCaseVariant(it->first, dbid.GetDb()).first;
            }
            return CSeq_id_Handle(it->second, packed, variant);
        }
        default:
            break;
        }
    }
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_General_PlainInfo* info = x_FindInfo(dbid);
    CSeq_id_Handle::TVariant variant = 0;
    if ( !info ) {
        info = new CSeq_id_General_PlainInfo(dbid, m_Mapper);
        STagMap& tm = m_DbMap[dbid.GetDb()];
        const CObject_id& oid = dbid.GetTag();
        if ( oid.IsStr() ) {
            //LOG_POST("CSeq_id_General_Tree::CreateStr("<<oid.GetStr()<<")");
            _VERIFY(tm.m_ByStr.insert
                    (STagMap::TByStr::value_type(oid.GetStr(), info)).second);
        }
        else if ( oid.IsId() ) {
            //LOG_POST("CSeq_id_General_Tree::CreateStr("<<oid.GetId()<<")");
            _VERIFY(tm.m_ById.insert(STagMap::TById::value_type(oid.GetId(),
                                                                info)).second);
        }
        else {
            NCBI_THROW(CSeq_id_MapperException, eEmptyError,
                       "Can not create index for an empty db-tag");
        }
    }
    else {
        variant = info->ParseCaseVariant(dbid);
    }
    return CSeq_id_Handle(info, 0, variant);
}


void CSeq_id_General_Tree::x_Unindex(const CSeq_id_Info* info)
{
    if ( !m_PackedStrMap.empty() ) {
        const CSeq_id_General_Str_Info* sinfo =
            dynamic_cast<const CSeq_id_General_Str_Info*>(info);
        if ( sinfo ) {
            m_PackedStrMap.erase(sinfo->GetKey());
            return;
        }
    }
    if ( !m_PackedIdMap.empty() ) {
        const CSeq_id_General_Id_Info* sinfo =
            dynamic_cast<const CSeq_id_General_Id_Info*>(info);
        if ( sinfo ) {
            m_PackedIdMap.erase(sinfo->GetKey());
            return;
        }
    }

    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsGeneral() );
    const CDbtag& dbid = id->GetGeneral();

    TDbMap::iterator db_it = m_DbMap.find(dbid.GetDb());
    _ASSERT(db_it != m_DbMap.end());
    STagMap& tm = db_it->second;
    const CObject_id& oid = dbid.GetTag();
    if ( oid.IsStr() ) {
        _VERIFY(tm.m_ByStr.erase(oid.GetStr()));
    }
    else if ( oid.IsId() ) {
        _VERIFY(tm.m_ById.erase(oid.GetId()));
    }
    if (tm.m_ByStr.empty()  &&  tm.m_ById.empty())
        m_DbMap.erase(db_it);
}


bool CSeq_id_General_Tree::HaveMatch(const CSeq_id_Handle& id) const
{
    // match id <-> str(number)
    TReadLockGuard guard(m_TreeLock);
    if ( !m_PackedStrMap.empty() ) {
        const CSeq_id_General_Str_Info* sinfo =
            dynamic_cast<const CSeq_id_General_Str_Info*>(id.x_GetInfo());
        if ( sinfo ) {
            // string with non-digital prefix or suffix
            // cannot be converted to numeric id
            if ( !sinfo->GetStrSuffix().empty() ||
                 !sx_AllDigits(sinfo->GetStrPrefix()) ) {
                return false;
            }
        }
    }
    return true;
}


void CSeq_id_General_Tree::FindMatch(const CSeq_id_Handle& id,
                                     TSeq_id_MatchList& id_list) const
{
    id_list.insert(id);
    // match id <-> str(number)
    {{
        TReadLockGuard guard(m_TreeLock);
        if ( !m_PackedStrMap.empty() ) {
            const CSeq_id_General_Str_Info* sinfo =
                dynamic_cast<const CSeq_id_General_Str_Info*>(id.x_GetInfo());
            if ( sinfo ) {
                // string with non-digital prefix or suffix
                // cannot be converted to numeric id
                if ( !sinfo->GetStrSuffix().empty() ||
                     !sx_AllDigits(sinfo->GetStrPrefix()) ) {
                    return;
                }
            }
        }
    }}
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CDbtag& dbtag = seq_id->GetGeneral();
    const CObject_id& obj_id = dbtag.GetTag();
    if ( obj_id.IsId() ) {
        int n = obj_id.GetId();
        if ( n >= 0 ) {
            CSeq_id seq_id2;
            CDbtag& dbtag2 = seq_id2.SetGeneral();
            dbtag2.SetDb(dbtag.GetDb());
            dbtag2.SetTag().SetStr(NStr::IntToString(n));
            CSeq_id_Handle id2 = FindInfo(seq_id2);
            if ( id2 ) {
                id_list.insert(id2);
            }
        }
    }
    else {
        const string& s = obj_id.GetStr();
        int n = NStr::StringToNonNegativeInt(s);
        if ( n >= 0 && NStr::IntToString(n) == s ) {
            CSeq_id seq_id2;
            CDbtag& dbtag2 = seq_id2.SetGeneral();
            dbtag2.SetDb(dbtag.GetDb());
            dbtag2.SetTag().SetId(n);
            CSeq_id_Handle id2 = FindInfo(seq_id2);
            if ( id2 ) {
                id_list.insert(id2);
            }
        }
    }
}


void CSeq_id_General_Tree::FindMatchStr(const string& sid,
                                        TSeq_id_MatchList& id_list) const
{
    TPacked value;
    bool ok;
    try {
        value = NStr::StringToNumeric<TPacked>(sid);
        ok = true;
    }
    catch (const CStringException&) {
        // Not an integer value
        value = -1;
        ok = false;
    }
    TReadLockGuard guard(m_TreeLock);
    ITERATE(TDbMap, db_it, m_DbMap) {
        // In any case search in strings
        STagMap::TByStr::const_iterator str_it =
            db_it->second.m_ByStr.find(sid);
        if (str_it != db_it->second.m_ByStr.end()) {
            id_list.insert(CSeq_id_Handle(str_it->second));
        }
        if ( ok ) {
            STagMap::TById::const_iterator int_it =
                db_it->second.m_ById.find(value);
            if (int_it != db_it->second.m_ById.end()) {
                id_list.insert(CSeq_id_Handle(int_it->second));
            }
        }
    }
}


size_t CSeq_id_General_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{ // m_DbMap
        size_t count = 0, bytes = 0;
        ITERATE ( TDbMap, it, m_DbMap ) {
            bytes += sizeof(string)+sizeof(STagMap); // map value
            bytes += sizeof(int)+3*sizeof(void*); // red/black tree
            // malloc overhead:
            // map value
            bytes += 1*kMallocOverhead;
            bytes += sx_StringMemory(it->first);
            ITERATE ( STagMap::TById, it2, it->second.m_ById ) {
                count += 1;
                bytes += sizeof(it2->first)+sizeof(it2->second); // map
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CObject_id);
                bytes += 4*kMallocOverhead;
            }
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                count += 1;
                bytes += sizeof(it2->first)+sizeof(it2->second); // map
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CObject_id);
                bytes += 4*kMallocOverhead;
                bytes += sx_StringMemory(it2->first);
            }
        }
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{ // m_PackedIdMap
        size_t count = m_PackedIdMap.size(), elem_size = 0, extra_size = 0;
        if ( count ) {
            elem_size = sizeof(TPackedIdKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_General_Id_Info); //
            // malloc overhead:
            // map value, CSeq_id_General_Id_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedIdMap, it, m_PackedIdMap ) {
                extra_size += sx_StringMemory(it->first);
            }
        }
        size_t bytes = extra_size + count*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " packed int handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{ // m_PackedStrMap
        size_t count = m_PackedStrMap.size(), elem_size = 0, extra_size = 0;
        if ( count ) {
            elem_size = sizeof(TPackedIdKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_General_Str_Info); //
            // malloc overhead:
            // map value, CSeq_id_General_Id_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedStrMap, it, m_PackedStrMap ) {
                extra_size += sx_StringMemory(it->first.m_Db);
                extra_size += sx_StringMemory(it->first.m_StrPrefix);
                extra_size += sx_StringMemory(it->first.m_StrSuffix);
            }
        }
        size_t bytes = extra_size + count*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " packed str handles, "<<bytes<<" bytes" << endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TDbMap, it, m_DbMap ) {
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                out << "  "<<it2->second->GetSeqId()->AsFastaString() << endl;
            }
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                out << "  "<<it2->second->GetSeqId()->AsFastaString() << endl;
            }
        }
        ITERATE ( TPackedIdMap, it, m_PackedIdMap ) {
            out << "  packed int "<<it->first << endl;
        }
        ITERATE ( TPackedStrMap, it, m_PackedStrMap ) {
            out << "  packed str "<<it->first.m_Key<<"/"<<it->first.m_Db<<"/"
                <<it->first.m_StrPrefix<<"/"<<it->first.m_StrSuffix << endl;
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Giim_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Giim_Tree::CSeq_id_Giim_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Giim_Tree::~CSeq_id_Giim_Tree(void)
{
}


bool CSeq_id_Giim_Tree::Empty(void) const
{
    return m_IdMap.empty();
}


CSeq_id_Info* CSeq_id_Giim_Tree::x_FindInfo(const CGiimport_id& gid) const
{
    TIdMap::const_iterator id_it = m_IdMap.find(gid.GetId());
    if (id_it == m_IdMap.end())
        return 0;
    ITERATE (TGiimList, dbr_it, id_it->second) {
        CConstRef<CSeq_id> id = (*dbr_it)->GetSeqId();
        const CGiimport_id& gid2 = id->GetGiim();
        // Both Db and Release must be equal
        if ( !gid.Equals(gid2) ) {
            return *dbr_it;
        }
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_Giim_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsGiim() );
    const CGiimport_id& gid = id.GetGiim();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(gid));
}


CSeq_id_Handle CSeq_id_Giim_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsGiim() );
    const CGiimport_id& gid = id.GetGiim();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(gid);
    if ( !info ) {
        info = CreateInfo(id);
        m_IdMap[gid.GetId()].push_back(info);
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Giim_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsGiim() );
    const CGiimport_id& gid = id->GetGiim();

    TIdMap::iterator id_it = m_IdMap.find(gid.GetId());
    _ASSERT(id_it != m_IdMap.end());
    TGiimList& giims = id_it->second;
    NON_CONST_ITERATE(TGiimList, dbr_it, giims) {
        if (*dbr_it == info) {
            giims.erase(dbr_it);
            break;
        }
    }
    if ( giims.empty() )
        m_IdMap.erase(id_it);
}


void CSeq_id_Giim_Tree::FindMatchStr(const string& sid,
                                     TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    try {
        TPacked value = NStr::StringToNumeric<TPacked>(sid);
        TIdMap::const_iterator it = m_IdMap.find(value);
        if (it == m_IdMap.end())
            return;
        ITERATE(TGiimList, git, it->second) {
            id_list.insert(CSeq_id_Handle(*git));
        }
    }
    catch (CStringException&) {
        // Not an integer value
        return;
    }
}


size_t CSeq_id_Giim_Tree::Dump(CNcbiOstream& out,
                               CSeq_id::E_Choice type,
                               int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TIdMap, it, m_IdMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 2*kMallocOverhead;
        size_t size2 = it->second.size();
        count += size2;
        bytes += it->second.capacity()*sizeof(void*);
        bytes += size2*sizeof(CSeq_id_Info);
        bytes += size2*sizeof(CSeq_id);
        bytes += size2*sizeof(CGiimport_id);
        ITERATE ( TGiimList, it2, it->second ) {
            const CGiimport_id& id = (*it2)->GetSeqId()->GetGiim();
            if ( id.IsSetDb() ) {
                bytes += sx_StringMemory(id.GetDb());
            }
            if ( id.IsSetRelease() ) {
                bytes += sx_StringMemory(id.GetRelease());
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TIdMap, it, m_IdMap ) {
            ITERATE ( TGiimList, it2, it->second ) {
                out << "  "<<(*it2)->GetSeqId()->AsFastaString() << endl;
            }
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Patent_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Patent_Tree::CSeq_id_Patent_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Patent_Tree::~CSeq_id_Patent_Tree(void)
{
}


bool CSeq_id_Patent_Tree::Empty(void) const
{
    return m_CountryMap.empty();
}


CSeq_id_Info* CSeq_id_Patent_Tree::x_FindInfo(const CPatent_seq_id& pid) const
{
    const CId_pat& cit = pid.GetCit();
    TByCountry::const_iterator cntry_it = m_CountryMap.find(cit.GetCountry());
    if (cntry_it == m_CountryMap.end())
        return 0;

    const string* number;
    const SPat_idMap::TByNumber* by_number;
    if ( cit.GetId().IsNumber() ) {
        number = &cit.GetId().GetNumber();
        by_number = &cntry_it->second.m_ByNumber;
    }
    else if ( cit.GetId().IsApp_number() ) {
        number = &cit.GetId().GetApp_number();
        by_number = &cntry_it->second.m_ByApp_number;
    }
    else {
        return 0;
    }

    SPat_idMap::TByNumber::const_iterator num_it = by_number->find(*number);
    if (num_it == by_number->end())
        return 0;
    SPat_idMap::TBySeqid::const_iterator seqid_it =
        num_it->second.find(pid.GetSeqid());
    if (seqid_it != num_it->second.end()) {
        return seqid_it->second;
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_Patent_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(pid));
}

CSeq_id_Handle CSeq_id_Patent_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(pid);
    if ( !info ) {
        const CId_pat& cit = pid.GetCit();
        SPat_idMap& country = m_CountryMap[cit.GetCountry()];
        if ( cit.GetId().IsNumber() ) {
            SPat_idMap::TBySeqid& num =
                country.m_ByNumber[cit.GetId().GetNumber()];
            _ASSERT(num.find(pid.GetSeqid()) == num.end());
            info = CreateInfo(id);
            num[pid.GetSeqid()] = info;
        }
        else if ( cit.GetId().IsApp_number() ) {
            SPat_idMap::TBySeqid& app = country.m_ByApp_number[
                cit.GetId().GetApp_number()];
            _ASSERT(app.find(pid.GetSeqid()) == app.end());
            info = CreateInfo(id);
            app[pid.GetSeqid()] = info;
        }
        else {
            // Can not index empty patent number
            NCBI_THROW(CSeq_id_MapperException, eEmptyError,
                       "Cannot index empty patent number");
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Patent_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsPatent() );
    const CPatent_seq_id& pid = id->GetPatent();

    TByCountry::iterator country_it =
        m_CountryMap.find(pid.GetCit().GetCountry());
    _ASSERT(country_it != m_CountryMap.end());
    SPat_idMap& pats = country_it->second;
    if ( pid.GetCit().GetId().IsNumber() ) {
        SPat_idMap::TByNumber::iterator num_it =
            pats.m_ByNumber.find(pid.GetCit().GetId().GetNumber());
        _ASSERT(num_it != pats.m_ByNumber.end());
        SPat_idMap::TBySeqid::iterator seqid_it =
            num_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != num_it->second.end());
        _ASSERT(seqid_it->second == info);
        num_it->second.erase(seqid_it);
        if ( num_it->second.empty() )
            pats.m_ByNumber.erase(num_it);
    }
    else if ( pid.GetCit().GetId().IsApp_number() ) {
        SPat_idMap::TByNumber::iterator app_it =
            pats.m_ByApp_number.find(pid.GetCit().GetId().GetApp_number());
        _ASSERT( app_it != pats.m_ByApp_number.end() );
        SPat_idMap::TBySeqid::iterator seqid_it =
            app_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != app_it->second.end());
        _ASSERT(seqid_it->second == info);
        app_it->second.erase(seqid_it);
        if ( app_it->second.empty() )
            pats.m_ByApp_number.erase(app_it);
    }
    if (country_it->second.m_ByNumber.empty()  &&
        country_it->second.m_ByApp_number.empty())
        m_CountryMap.erase(country_it);
}


void CSeq_id_Patent_Tree::FindMatchStr(const string& sid,
                                       TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    ITERATE (TByCountry, cit, m_CountryMap) {
        SPat_idMap::TByNumber::const_iterator nit =
            cit->second.m_ByNumber.find(sid);
        if (nit != cit->second.m_ByNumber.end()) {
            ITERATE(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.insert(CSeq_id_Handle(iit->second));
            }
        }
        SPat_idMap::TByNumber::const_iterator ait =
            cit->second.m_ByApp_number.find(sid);
        if (ait != cit->second.m_ByApp_number.end()) {
            ITERATE(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.insert(CSeq_id_Handle(iit->second));
            }
        }
    }
}


size_t CSeq_id_Patent_Tree::Dump(CNcbiOstream& out,
                                 CSeq_id::E_Choice type,
                                 int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TByCountry, it, m_CountryMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 1*kMallocOverhead;
        bytes += sx_StringMemory(it->first);
        ITERATE ( SPat_idMap::TByNumber, it2, it->second.m_ByNumber ) {
            bytes += sizeof(it2->first) + sizeof(it2->second);
            bytes += sizeof(int)+3*sizeof(void*); // red/black tree
            // malloc overhead:
            // map value, vector
            bytes += 1*kMallocOverhead;
            bytes += sx_StringMemory(it2->first);
            ITERATE ( SPat_idMap::TBySeqid, it3, it2->second ) {
                count += 1;
                bytes += sizeof(it2->first) + sizeof(it2->second);
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CPatent_seq_id);
                bytes += sizeof(CId_pat);
                // malloc overhead:
                // map value, 
                bytes += 5*kMallocOverhead;
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TByCountry, it, m_CountryMap ) {
            ITERATE ( SPat_idMap::TByNumber, it2, it->second.m_ByNumber ) {
                ITERATE ( SPat_idMap::TBySeqid, it3, it2->second ) {
                    out << "  "<<it3->second->GetSeqId()->AsFastaString() << endl;
                }
            }
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_PDB_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_PDB_Tree::CSeq_id_PDB_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_PDB_Tree::~CSeq_id_PDB_Tree(void)
{
}


bool CSeq_id_PDB_Tree::Empty(void) const
{
    return m_MolMap.empty();
}


CSeq_id_PDB_Info::CSeq_id_PDB_Info(const CConstRef<CSeq_id>& seq_id, CSeq_id_Mapper* mapper)
    : CSeq_id_Info(seq_id, mapper)
{
}


CConstRef<CSeq_id> CSeq_id_PDB_Info::GetPackedSeqId(TPacked /*packed*/, TVariant variant) const
{
    if ( !variant ) {
        // the seq-id is already normalized
        return m_Seq_id;
    }
    CRef<CSeq_id> ret(new CSeq_id);
    s_AssignSeq_id(*ret, *m_Seq_id);
    CPDB_seq_id& pdb_id = ret->SetPdb();
    // update chain/chain-id set
    if ( variant & (TVariant(1)<<kNoChainOffset) ) {
        pdb_id.ResetChain();
    }
    if ( variant & (TVariant(1)<<kNoChain_idOffset) ) {
        pdb_id.ResetChain_id();
    }
    variant &= ~((TVariant(1)<<kNoChainOffset) |
                 (TVariant(1)<<kNoChain_idOffset));
    const TVariant kMolLowerCaseMask =
        (TVariant(1) << (kMolLowerCaseOffset+kMolLowerCaseBits)) -
        (TVariant(1) << (kMolLowerCaseOffset));
    if ( variant & kMolLowerCaseMask ) {
        string& mol = pdb_id.SetMol().Set();
        for ( int i = 0; i < kMolLowerCaseBits; ++i ) {
            if ( variant & (TVariant(1) << (kMolLowerCaseOffset+i)) ) {
                mol[i+1] = tolower(mol[i+1]);
            }
        }
        variant &= ~kMolLowerCaseMask;
    }
    if ( variant ) {
        // add date
        CDate_std& date = pdb_id.SetRel().SetStd();
        int year = (variant >> kYearOffset) & ((1 << kYearBits)-1);
        int month = (variant >> kMonthOffset) & ((1 << kMonthBits)-1);
        int day = (variant >> kDayOffset) & ((1 << kDayBits)-1);
        int hour = (variant >> kHourOffset) & ((1 << kHourBits)-1);
        int minute = (variant >> kMinuteOffset) & ((1 << kMinuteBits)-1);
        int second = (variant >> kSecondOffset) & ((1 << kSecondBits)-1);
        date.SetYear(year);
        if ( month != 0 ) {
            date.SetMonth(month);
        }
        if ( day != 0 ) {
            date.SetDay(day);
        }
        if ( hour != (1 << kHourBits)-1 ) {
            date.SetHour(hour);
        }
        if ( minute != (1 << kMinuteBits)-1 ) {
            date.SetMinute(minute);
        }
        if ( second != (1 << kSecondBits)-1 ) {
            date.SetSecond(second);
        }
    }
    return ret;
}


static inline int x_GetUnset0(int bits)
{
    // (1<<bits)-1 is reserved for unset field
    return (1<<bits)-1;
}


static inline bool x_GetUnset1(int /*bits*/)
{
    // 0 is reserved for unset field
    return 0;
}


static inline bool x_InRange0(int value, int bits)
{
    // (1<<bits)-1 is reserved for unset field
    return value >= 0 && value <= (1<<bits)-2;
}


static inline bool x_InRange1(int value, int bits)
{
    // 0 is reserved for unset field
    return value >= 1 && value <= (1<<bits)-1;
}


CSeq_id_Info::TVariant CSeq_id_PDB_Info::x_NormalizeDate(const CDate_std& date_std)
{
    if ( x_InRange1(date_std.GetYear(), kYearBits) &&
         (!date_std.IsSetMonth() || x_InRange1(date_std.GetMonth(), kMonthBits)) &&
         (!date_std.IsSetDay() || x_InRange1(date_std.GetDay(), kDayBits)) &&
         !date_std.IsSetSeason() &&
         (!date_std.IsSetHour() || x_InRange0(date_std.GetHour(), kHourBits)) &&
         (!date_std.IsSetMinute() || x_InRange0(date_std.GetMinute(), kMinuteBits)) &&
         (!date_std.IsSetSecond() || x_InRange0(date_std.GetSecond(), kSecondBits)) ) {
        int year = date_std.GetYear();
        int month = date_std.IsSetMonth()? date_std.GetMonth(): x_GetUnset1(kMonthBits);
        int day = date_std.IsSetDay()? date_std.GetDay(): x_GetUnset1(kDayBits);
        int hour = date_std.IsSetHour()? date_std.GetHour(): x_GetUnset0(kHourBits);
        int minute = date_std.IsSetMinute()? date_std.GetMinute(): x_GetUnset0(kMinuteBits);
        int second = date_std.IsSetSecond()? date_std.GetSecond(): x_GetUnset0(kSecondBits);
        return
            (TVariant(year) << kYearOffset) |
            (TVariant(month) << kMonthOffset) |
            (TVariant(day) << kDayOffset) |
            (TVariant(hour) << kHourOffset) |
            (TVariant(minute) << kMinuteOffset) |
            (TVariant(second) << kSecondOffset);
    }
    return 0;
}


pair<CConstRef<CSeq_id>, CSeq_id_Info::TVariant> CSeq_id_PDB_Info::Normalize(const CSeq_id& seq_id)
{
    pair<CConstRef<CSeq_id>, TVariant> ret(Ref(&seq_id), 0);
    const CPDB_seq_id& pdb_id = seq_id.GetPdb();
    // try to normalize date
    if ( pdb_id.IsSetRel() ) {
        const CDate& date = pdb_id.GetRel();
        if ( date.IsStd() ) {
            ret.second = x_NormalizeDate(date.GetStd());
        }
        if ( !ret.second ) {
            // non-normalizable date, use PDB id as is
            return ret;
        }
    }
    // normalize chain
    bool normal_has_chain =
        pdb_id.IsSetChain() || (pdb_id.IsSetChain_id() && pdb_id.GetChain_id().size() == 1);
    bool normal_has_chain_id =
        pdb_id.IsSetChain_id() || pdb_id.IsSetChain();
    bool need_upcase = !NStr::IsUpper(pdb_id.GetMol().Get());
    if ( ret.second ||
         need_upcase ||
         pdb_id.IsSetChain() != normal_has_chain ||
         pdb_id.IsSetChain_id() != normal_has_chain_id ) {
        // create normalized PDB id
        CRef<CSeq_id> new_seq_id(new CSeq_id());
        CPDB_seq_id& new_pdb_id = new_seq_id->SetPdb();
        new_pdb_id.SetMol(pdb_id.GetMol());
        if ( need_upcase ) {
            string& mol = new_pdb_id.SetMol().Set();
            for ( int i = 0; i < kMolLowerCaseBits && size_t(i+1) < mol.size(); ++i ) {
                char c = mol[i+1];
                if ( islower(c) ) {
                    mol[i+1] = toupper(c);
                    ret.second |= TVariant(1) << (kMolLowerCaseOffset + i);
                }
            }
        }
        if ( normal_has_chain_id ) {
            if ( pdb_id.IsSetChain_id() ) {
                new_pdb_id.SetChain_id(pdb_id.GetChain_id());
            }
            else {
                new_pdb_id.SetChain_id(string(1, char(pdb_id.GetChain())));
            }
        }
        if ( normal_has_chain ) {
            new_pdb_id.SetChain(new_pdb_id.GetChain_id()[0]);
        }
        ret.second |=
            (TVariant(!pdb_id.IsSetChain()) << kNoChainOffset) |
            (TVariant(!pdb_id.IsSetChain_id()) << kNoChain_idOffset);
        ret.first = new_seq_id;
    }
    return ret;
}


inline string CSeq_id_PDB_Tree::x_IdToStrKey(const CPDB_seq_id& id) const
{
// this is an attempt to follow the undocumented rules of PDB
// ("documented" as code written elsewhere)
    string skey = id.GetMol().Get();
    if (id.IsSetChain_id()) {
        skey += '_';
        skey += id.GetChain_id();
    }
    else if (id.IsSetChain()) {
        skey += '_';
        skey += char(id.GetChain());
    }
    return skey;
}


CSeq_id_Handle CSeq_id_PDB_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsPdb() );
    pair<CConstRef<CSeq_id>, CSeq_id_Info::TVariant> normalized_id = CSeq_id_PDB_Info::Normalize(id);
    const CPDB_seq_id& pid = normalized_id.first->GetPdb();
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if ( mol_it != m_MolMap.end() ) {
        ITERATE( TSubMolList, it, mol_it->second ) {
            if ( pid.Equals((*it)->GetSeqId()->GetPdb()) ) {
                return CSeq_id_Handle(*it, 0, normalized_id.second);
            }
        }
    }
    return CSeq_id_Handle();
}


CSeq_id_Handle CSeq_id_PDB_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsPdb() );
    pair<CConstRef<CSeq_id>, CSeq_id_Info::TVariant> normalized_id = CSeq_id_PDB_Info::Normalize(id);
    const CPDB_seq_id& pid = normalized_id.first->GetPdb();
    TWriteLockGuard guard(m_TreeLock);
    TSubMolList& sub = m_MolMap[x_IdToStrKey(pid)];
    ITERATE ( TSubMolList, it, sub ) {
        if ( pid.Equals((*it)->GetSeqId()->GetPdb()) ) {
            return CSeq_id_Handle(*it, 0, normalized_id.second);
        }
    }
    if ( normalized_id.first == &id ) {
        // create a copy
        CRef<CSeq_id> id_ref(new CSeq_id);
        s_AssignSeq_id(*id_ref, id);
        normalized_id.first = id_ref;
    }
    CSeq_id_Info* info = new CSeq_id_PDB_Info(normalized_id.first, m_Mapper);
    sub.push_back(info);
    return CSeq_id_Handle(info, 0, normalized_id.second);
}


void CSeq_id_PDB_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsPdb() );
    const CPDB_seq_id& pid = id->GetPdb();

    TMolMap::iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    _ASSERT(mol_it != m_MolMap.end());
    NON_CONST_ITERATE(TSubMolList, it, mol_it->second) {
        if (*it == info) {
            //_ASSERT(pid.Equals((*it)->GetSeqId()->GetPdb()));
            mol_it->second.erase(it);
            break;
        }
    }
    if ( mol_it->second.empty() )
        m_MolMap.erase(mol_it);
}


bool CSeq_id_PDB_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_PDB_Tree::FindMatch(const CSeq_id_Handle& id,
                                 TSeq_id_MatchList& id_list) const
{
    //_ASSERT(id && id == FindInfo(id.GetSeqId()));
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CPDB_seq_id& pid = seq_id->GetPdb();
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if (mol_it == m_MolMap.end())
        return;
    ITERATE(TSubMolList, it, mol_it->second) {
        const CPDB_seq_id& pid2 = (*it)->GetSeqId()->GetPdb();
        // Ignore date if not set in id
        if ( pid.IsSetRel() ) {
            if ( !pid2.IsSetRel()  ||
                !pid.GetRel().Equals(pid2.GetRel()) )
                continue;
        }
        id_list.insert(CSeq_id_Handle(*it));
    }
}


void CSeq_id_PDB_Tree::FindMatchStr(const string& sid,
                                    TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mit = m_MolMap.find(sid);
    if (mit == m_MolMap.end())
        return;
    ITERATE(TSubMolList, sub_it, mit->second) {
        id_list.insert(CSeq_id_Handle(*sub_it));
    }
}


bool CSeq_id_PDB_Tree::HaveReverseMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_PDB_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                        TSeq_id_MatchList& id_list)
{
    //_ASSERT(id && id == FindInfo(id.GetSeqId()));
    id_list.insert(id);
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CPDB_seq_id& pid = seq_id->GetPdb();
    if ( !pid.IsSetRel() )
        return;
    // find id without release date
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if (mol_it == m_MolMap.end())
        return;
    ITERATE(TSubMolList, it, mol_it->second) {
        const CPDB_seq_id& pid2 = (*it)->GetSeqId()->GetPdb();
        // Ignore date if set in id
        if ( pid2.IsSetRel() )
            continue;
        id_list.insert(CSeq_id_Handle(*it));
    }
}


size_t CSeq_id_PDB_Tree::Dump(CNcbiOstream& out,
                              CSeq_id::E_Choice type,
                              int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TMolMap, it, m_MolMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 2*kMallocOverhead;
        bytes += sx_StringMemory(it->first);
        size_t size2 = it->second.size();
        count += size2;
        bytes += it->second.capacity()*sizeof(void*);
        bytes += size2*sizeof(CSeq_id_Info);
        bytes += size2*sizeof(CSeq_id);
        bytes += size2*sizeof(CPDB_seq_id);
        ITERATE ( TSubMolList, it2, it->second ) {
            const CPDB_seq_id& id = (*it2)->GetSeqId()->GetPdb();
            if ( id.IsSetRel() ) {
                bytes += sizeof(CDate);
                bytes += kMallocOverhead;
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TMolMap, it, m_MolMap ) {
            ITERATE ( TSubMolList, it2, it->second ) {
                out << "  "<<(*it2)->GetSeqId()->AsFastaString() << endl;
            }
        }
    }
    return total_bytes;
}


const char* CSeq_id_MapperException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eTypeError:   return "eTypeError";
    case eSymbolError: return "eSymbolError";
    case eEmptyError:  return "eEmptyError";
    case eOtherError:  return "eOtherError";
    default:           return CException::GetErrCodeString();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
