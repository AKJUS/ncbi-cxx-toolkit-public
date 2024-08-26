#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Cache for loaded bioseq info
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/genbank/psg_loader.hpp>

#if defined(HAVE_PSG_LOADER)
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////

struct SPsgBioseqInfo
{
    SPsgBioseqInfo(const CSeq_id_Handle& request_id,
                   const CPSG_BioseqInfo& bioseq_info,
                   int lifespan);

    typedef underlying_type_t<CPSG_Request_Resolve::EIncludeInfo> TIncludedInfo;
    typedef vector<CSeq_id_Handle> TIds;

    CSeq_id_Handle request_id;
    atomic<TIncludedInfo> included_info;
    CSeq_inst::TMol molecule_type;
    Uint8 length;
    CPSG_BioseqInfo::TState state;
    CPSG_BioseqInfo::TState chain_state;
    TTaxId tax_id;
    int hash;
    TGi gi;
    CSeq_id_Handle canonical;
    TIds ids;
    string psg_blob_id;

    CDeadline deadline;

    TIncludedInfo Update(const CPSG_BioseqInfo& bioseq_info);
    bool IsDead() const;
    CBioseq_Handle::TBioseqStateFlags GetBioseqStateFlags() const;
    CBioseq_Handle::TBioseqStateFlags GetChainStateFlags() const;

    bool KnowsBlobId() const
    {
        return included_info & CPSG_Request_Resolve::fBlobId;
    }
    bool HasBlobId() const;
    const string& GetPSGBlobId() const
    {
        return psg_blob_id;
    }
    CConstRef<CPsgBlobId> GetDLBlobId() const;

private:
    SPsgBioseqInfo(const SPsgBioseqInfo&);
    SPsgBioseqInfo& operator=(const SPsgBioseqInfo&);
};


/////////////////////////////////////////////////////////////////////////////
// CPSGBioseqCache
/////////////////////////////////////////////////////////////////////////////

class CPSGBioseqCache
{
public:
    CPSGBioseqCache(int lifespan, size_t max_size)
        : m_Lifespan(lifespan), m_MaxSize(max_size) {}
    ~CPSGBioseqCache(void) {}

    shared_ptr<SPsgBioseqInfo> Get(const CSeq_id_Handle& idh);
    shared_ptr<SPsgBioseqInfo> Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TIdMap;
    typedef list<shared_ptr<SPsgBioseqInfo> > TInfoQueue;

    mutable CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TIdMap m_Ids;
    TInfoQueue m_Infos;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGCache_Base
/////////////////////////////////////////////////////////////////////////////

template<class TK, class TV>
class CPSGCache_Base
{
public:
    typedef TK TKey;
    typedef TV TValue;
    typedef CPSGCache_Base<TKey, TValue> TParent;

    CPSGCache_Base(int lifespan, size_t max_size, TValue def_val = TValue(nullptr))
        : m_Default(def_val),
          m_Lifespan(lifespan),
          m_MaxSize(max_size)
    {}

    TValue Find(const TKey& key) {
        CFastMutexGuard guard(m_Mutex);
        x_Expire();
        auto found = m_Values.find(key);
        return found != m_Values.end() ? found->second.value : m_Default;
    }
    
    void Add(const TKey& key, const TValue& value) {
        CFastMutexGuard guard(m_Mutex);
        auto iter = m_Values.lower_bound(key);
        if ( iter != m_Values.end() && key == iter->first ) {
            // erase old value
            x_Erase(iter++);
        }
        // insert
        iter = m_Values.insert(iter,
                               typename TValues::value_type(key, SNode(value, m_Lifespan)));
        iter->second.remove_list_iterator = m_RemoveList.insert(m_RemoveList.end(), iter);
        x_LimitSize();
    }

protected:
    // Map blob-id to blob info
    typedef TKey key_type;
    typedef TValue mapped_type;
    struct SNode;
    typedef map<key_type, SNode> TValues;
    typedef typename TValues::iterator TValueIter;
    typedef list<TValueIter> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveIter;
    struct SNode {
        SNode(const mapped_type& value, unsigned lifespan)
            : value(value),
              deadline(lifespan)
        {}
        mapped_type value;
        CDeadline deadline;
        TRemoveIter remove_list_iterator;
    };

    void x_Erase(TValueIter iter) {
        m_RemoveList.erase(iter->second.remove_list_iterator);
        m_Values.erase(iter);
    }
    void x_Expire() {
        while ( !m_RemoveList.empty() &&
                m_RemoveList.front()->second.deadline.IsExpired() ) {
            x_PopFront();
        }
    }
    void x_LimitSize() {
        while ( m_Values.size() > m_MaxSize ) {
            x_PopFront();
        }
    }
    void x_PopFront() {
        _ASSERT(!m_RemoveList.empty());
        _ASSERT(m_RemoveList.front() != m_Values.end());
        _ASSERT(m_RemoveList.front()->second.remove_list_iterator == m_RemoveList.begin());
        m_Values.erase(m_RemoveList.front());
        m_RemoveList.pop_front();
    }

    TValue m_Default;
    CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TValues m_Values;
    TRemoveList m_RemoveList;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGCDDInfoCache
/////////////////////////////////////////////////////////////////////////////

class CPSGCDDInfoCache : public CPSGCache_Base<string, bool>
{
public:
    CPSGCDDInfoCache(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, false) {}
};


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////

struct SPsgBlobInfo
{
    explicit SPsgBlobInfo(const CPSG_BlobInfo& blob_info);
    explicit SPsgBlobInfo(const CTSE_Info& tse);

    string blob_id_main;
    string id2_info;
    CBioseq_Handle::TBioseqStateFlags blob_state_flags;
    Int8 last_modified;

    int GetBlobVersion() const { return int(last_modified/60000); /* ms to minutes */ }

    bool IsSplit() const { return !id2_info.empty(); }

private:
    SPsgBlobInfo(const SPsgBlobInfo&);
    SPsgBlobInfo& operator=(const SPsgBlobInfo&);
};


/////////////////////////////////////////////////////////////////////////////
// CPSGBlobMap
/////////////////////////////////////////////////////////////////////////////

class CPSGBlobMap : public CPSGCache_Base<string, shared_ptr<SPsgBlobInfo>>
{
public:
    CPSGBlobMap(int lifespan, size_t max_size)
        : TParent(lifespan, max_size) {}

    void DropBlob(const CPsgBlobId& blob_id) {
        //ERR_POST("DropBlob("<<blob_id.ToPsgId()<<")");
        CFastMutexGuard guard(m_Mutex);
        auto iter = m_Values.find(blob_id.ToPsgId());
        if ( iter != m_Values.end() ) {
            x_Erase(iter);
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
// CPSGIpgTaxIdMap
/////////////////////////////////////////////////////////////////////////////

class CPSGIpgTaxIdMap : public CPSGCache_Base<CSeq_id_Handle, TTaxId>
{
public:
    CPSGIpgTaxIdMap(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, INVALID_TAX_ID) {}
};


/////////////////////////////////////////////////////////////////////////////
// CPSGAnnotCache
/////////////////////////////////////////////////////////////////////////////

struct SPsgAnnotInfo
{
    typedef CDataLoader::TIds TIds;
    typedef list<shared_ptr<CPSG_NamedAnnotInfo>> TInfos;

    SPsgAnnotInfo(const string& _name,
                  const TIds& _ids,
                  const TInfos& _infos,
                  int lifespan);

    string name;
    TIds ids;
    TInfos infos;
    CDeadline deadline;

private:
    SPsgAnnotInfo(const SPsgAnnotInfo&);
    SPsgAnnotInfo& operator=(const SPsgAnnotInfo&);
};


class CPSGAnnotCache
{
public:
    CPSGAnnotCache(int lifespan, size_t max_size)
        : m_Lifespan(lifespan), m_MaxSize(max_size) {}
    ~CPSGAnnotCache(void) {}

    typedef CDataLoader::TIds TIds;

    shared_ptr<SPsgAnnotInfo> Get(const string& name, const CSeq_id_Handle& idh);
    shared_ptr<SPsgAnnotInfo> Add(const SPsgAnnotInfo::TInfos& infos,
                                  const string& name,
                                  const TIds& ids);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgAnnotInfo>> TIdMap;
    typedef map<string, TIdMap> TNameMap;
    typedef list<shared_ptr<SPsgAnnotInfo>> TInfoQueue;

    mutable CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TNameMap m_NameMap;
    TInfoQueue m_Infos;
};


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif // OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP
