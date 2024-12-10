#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSORS__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSORS__HPP

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
 * File Description: PSG reply processors
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processor.hpp>
#include <objtools/data_loaders/genbank/impl/psg_blob_processor.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cdd.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;

class CObjectIStream;

BEGIN_NAMESPACE(objects);

class CTSE_Lock;
class CDataSource;
class CTSE_Chunk_Info;
class CPsgBlobId;
class CID2S_Chunk;

BEGIN_NAMESPACE(psgl);


struct SPsgBioseqInfo;
struct SPsgBlobInfo;
struct SCDDIds;
class CPSGCaches;


/////////////////////////////////////////////////////////////////////////////
// CPSGL_BioseqInfo_Processor
//
// Possible states after completion:
//   0. Error:
//        check:
//          GetStatus() != CThreadPool_Task::eCompleted { eFailed or eCanceled }
//          or m_BioseqInfoStatus == { EPSG_Status::eError or EPSG_Status::eCanceled }
//        operation failed for some reason, retry may succeed
//   otherwise
//          GetStatus() == CThreadPool_Task::eCompleted:
//   1. Unresolved: PSG doesn't know about the requested seq id
//        check:
//          m_BioseqInfoStatus == EPSG_Status::eNotFound
//        result:
//          m_BioseqInfoResult == nullptr
//   2. Resolved: normal sequence
//        check:
//          m_BioseqInfoStatus == EPSG_Status::eSuccess
//        result:
//          m_BioseqInfoResult != nullptr
//   3. Resolved but forbidden: sequence exists but client doesn't have permission to get it
//        check:
//          m_BioseqInfoStatus == EPSG_Status::eForbidden
//        result:
//          m_BioseqInfoResult != nullptr
/////////////////////////////////////////////////////////////////////////////

class CPSGL_BioseqInfo_Processor : public CPSGL_Processor
{
public:
    explicit
    CPSGL_BioseqInfo_Processor(const CSeq_id_Handle& seq_id,
                               CPSGCaches* caches = nullptr);
    ~CPSGL_BioseqInfo_Processor() override;

    const CSeq_id_Handle& GetSeq_id() const
    {
        return m_Seq_id;
    }

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    // arguments
    CSeq_id_Handle m_Seq_id;

    // processing data
    CFastMutex m_BioseqInfoMutex;
    EPSG_Status m_BioseqInfoStatus;
    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;
    
    // cache pointers
    CPSGCaches* m_Caches = nullptr;

    // result
    shared_ptr<SPsgBioseqInfo> m_BioseqInfoResult;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_BlobInfo_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_BlobInfo_Processor : public CPSGL_Processor
{
public:
    explicit
    CPSGL_BlobInfo_Processor(const CSeq_id_Handle& seq_id,
                             CPSGCaches* caches = nullptr);
    explicit
    CPSGL_BlobInfo_Processor(const string& blob_id,
                             CPSGCaches* caches = nullptr);
    explicit
    CPSGL_BlobInfo_Processor(const CSeq_id_Handle& seq_id,
                             const string& blob_id,
                             CPSGCaches* caches = nullptr);
    ~CPSGL_BlobInfo_Processor() override;

    const CSeq_id_Handle& GetSeq_id() const
    {
        return m_Seq_id;
    }
    const string& GetBlob_id() const
    {
        return m_Blob_id;
    }

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    // arguments
    CSeq_id_Handle m_Seq_id;
    string m_Blob_id;

    // processing data
    CFastMutex m_BlobInfoMutex;
    EPSG_Status m_BlobInfoStatus;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;

    // cache pointers
    CPSGCaches* m_Caches = nullptr;

    // result
    shared_ptr<SPsgBlobInfo> m_BlobInfoResult;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Info_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_Info_Processor : public CPSGL_Processor
{
public:
    explicit
    CPSGL_Info_Processor(const CSeq_id_Handle& seq_id,
                         CPSGCaches* caches = nullptr);
    explicit
    CPSGL_Info_Processor(const CSeq_id_Handle& seq_id,
                         const string& blob_id,
                         CPSGCaches* caches = nullptr);
    ~CPSGL_Info_Processor() override;

    const CSeq_id_Handle& GetSeq_id() const
    {
        return m_Seq_id;
    }
    const string& GetBlob_id() const
    {
        return m_Blob_id;
    }

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    // arguments
    CSeq_id_Handle m_Seq_id;
    string m_Blob_id;

    // processing data
    CFastMutex m_InfoMutex;
    EPSG_Status m_BioseqInfoStatus;
    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;
    EPSG_Status m_BlobInfoStatus;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;

    // cache pointers
    CPSGCaches* m_Caches = nullptr;

    // result
    shared_ptr<SPsgBioseqInfo> m_BioseqInfoResult;
    shared_ptr<SPsgBlobInfo> m_BlobInfoResult;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_IpgTaxId_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_IpgTaxId_Processor : public CPSGL_Processor
{
public:
    explicit
    CPSGL_IpgTaxId_Processor(const CSeq_id_Handle& seq_id,
                             bool is_WP_acc,
                             CPSGCaches* caches = nullptr);
    ~CPSGL_IpgTaxId_Processor() override;

    const CSeq_id_Handle& GetSeq_id() const
    {
        return m_Seq_id;
    }

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    // arguments
    CSeq_id_Handle m_Seq_id;
    bool m_IsWPAcc = false;

    // processing data
    CFastMutex m_IpgTaxIdMutex;
    EPSG_Status m_IpgTaxIdStatus;
    
    // cache pointers
    CPSGCaches* m_Caches = nullptr;

    // result
    TTaxId m_TaxId = INVALID_TAX_ID;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_CDDAnnot_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_CDDAnnot_Processor : public CPSGL_Processor
{
public:
    typedef vector<CSeq_id_Handle> TSeqIdSet;
    
    explicit
    CPSGL_CDDAnnot_Processor(const SCDDIds& cdd_ids,
                             const TSeqIdSet& id_set,
                             CDataSource* data_source,
                             CPSGCaches* caches = nullptr);
    ~CPSGL_CDDAnnot_Processor() override;

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    // arguments
    SCDDIds m_CDDIds;
    TSeqIdSet m_SeqIdSet;

    // processing data
    shared_ptr<CPSG_NamedAnnotInfo> m_AnnotInfo;
    shared_ptr<CPSG_NamedAnnotStatus> m_AnnotStatus;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;

    // cache pointers
    CDataSource* m_DataSource = nullptr; // OM data source to get TSE locks from
    CPSGCaches* m_Caches = nullptr;

    // result
    CTSE_Lock m_TSE_Lock;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Get_Processor
//
// Possible states after completion:
//   1. Error:
//        check: m_BioseqInfoStatus == { EPSG_Status::eError or EPSG_Status::eCanceled }
//        processing status != CThreadPool_Task::eCompleted, normally eFailed or eCanceled
//        - operation failed for some reason, retry may succeed
//   3. Unresolved:
//        check: m_BioseqInfoStatus == EPSG_Status::eNotFound
//        also HasBlob_id() == false
//        - PSG doesn't know about the requested seq id
//   2. Loaded:
//        check: m_BioseqInfoStatus == EPSG_Status::eSuccess && GetTSE_Lock() != null
//        - GetTSE_Lock() returns TSE with the requested sequence
//   3. Resolved but not loaded:
//        check: m_BioseqInfoStatus == EPSG_Status::eSuccess && GetTSE_Lock() == null
//        also HasBlob_id() == true and GotForbidden() == false
//        - the TSE can be loaded via blob id GetDLBlobId()
//   4. Resolved but forbidden:
//        m_BioseqInfoStatus == EPSG_Status::eForbidden
//        GotForbidden() == true - the sequence is known but client is not allowed to load it,
//        e.g. withdrawn or restricted, actual blob state is GetForbiddenBlobState()
/////////////////////////////////////////////////////////////////////////////

class CPSGL_Get_Processor : public CPSGL_Blob_Processor
{
public:
    explicit
    CPSGL_Get_Processor(const CSeq_id_Handle& seq_id,
                        CDataSource* data_source,
                        CPSGCaches* caches = nullptr,
                        bool add_wgs_master = false);
    ~CPSGL_Get_Processor() override;

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    const CSeq_id_Handle& GetSeq_id() const
    {
        return m_Seq_id;
    }

    typedef shared_ptr<CPSG_BioseqInfo> TBioseqInfo;
    EPSG_Status GetBioseqInfoStatus() const
    {
        return m_BioseqInfoStatus;
    }
    const TBioseqInfo& GetBioseqInfo() const
    {
        return m_BioseqInfo;
    }
    const shared_ptr<SPsgBioseqInfo>& GetBioseqInfoResult() const
    {
        return m_BioseqInfoResult;
    }

    bool HasBlob_id() const;
    const string& GetPSGBlobId() const;
    CConstRef<CPsgBlobId> GetDLBlobId() const;
    const CTSE_Lock& GetTSE_Lock() const
    {
        return m_TSE_Lock;
    }

    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplyFinal() override;

    // we need to mark blob id with 'dead' flag from bioseq info
    CConstRef<CPsgBlobId> CreateDLBlobId(STSESlot* tse_slot) override;

    int GetForbiddenBlobState() const
    {
        return m_ForbiddenBlobState;
    }

protected:
    // process reply without attempting to get TSE lock
    EProcessResult x_PreProcessReply(EPSG_Status status,
                                     const shared_ptr<CPSG_Reply>& reply);

    
private:
    CFastMutex m_GetMutex;
    
    // arguments
    CSeq_id_Handle m_Seq_id;

    // processing data
    EPSG_Status m_BioseqInfoStatus;
    TBioseqInfo m_BioseqInfo;

    // cache pointers and other params
    
    // result:
    shared_ptr<SPsgBioseqInfo> m_BioseqInfoResult;
    int m_ForbiddenBlobState = 0;
    CTSE_Lock m_TSE_Lock;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_GetBlob_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_GetBlob_Processor : public CPSGL_Blob_Processor
{
public:
    explicit
    CPSGL_GetBlob_Processor(const CPsgBlobId& dl_blob_id,
                            CDataSource* data_source,
                            CPSGCaches* caches = nullptr,
                            bool add_wgs_master = false);
    ~CPSGL_GetBlob_Processor() override;

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;

    const string& GetBlob_id() const
    {
        return m_Blob_id;
    }

    const CTSE_Lock& GetTSE_Lock() const
    {
        return m_TSE_Lock;
    }

    EProcessResult ProcessReplyFast(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplyFinal() override;
    
private:
    // arguments
    string m_Blob_id;

    // result
    CTSE_Lock m_TSE_Lock;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_GetChunk_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_GetChunk_Processor : public CPSGL_Blob_Processor
{
public:
    explicit
    CPSGL_GetChunk_Processor(CTSE_Chunk_Info& chunk,
                             CDataSource* data_source,
                             CPSGCaches* caches = nullptr,
                             bool add_wgs_master = false);
    ~CPSGL_GetChunk_Processor() override;
    
    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    ostream& PrintChunk(ostream& out, CTSE_Chunk_Info& chunk) const;

    void AddChunk(CTSE_Chunk_Info& chunk);

    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

private:
    // arguments
    vector<CTSE_Chunk_Info*> m_Chunks;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_NA_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_NA_Processor : public CPSGL_Blob_Processor
{
public:
    typedef vector<CSeq_id_Handle> TSeq_ids;
    explicit
    CPSGL_NA_Processor(const TSeq_ids& ids,
                       CDataSource* data_source,
                       CPSGCaches* caches = nullptr,
                       bool add_wgs_master = false);
    ~CPSGL_NA_Processor() override;

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;
    
    const TSeq_ids& GetSeq_ids() const
    {
        return m_Seq_ids;
    }

    typedef pair<EPSG_Status, shared_ptr<CPSG_NamedAnnotInfo>> TAnnotInfo;
    typedef vector<TAnnotInfo> TAnnotInfos;

    struct SResult {
        string m_NA;
        string m_Blob_id;
        CTSE_Lock m_TSE_Lock;
    };
    typedef list<SResult> TResults;
    
    const TAnnotInfos& GetAnnotInfos() const
    {
        return m_AnnotInfos;
    }
    const TResults& GetResults() const
    {
        return m_Results;
    }
    
    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessItemSlow(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;
    EProcessResult ProcessReplyFinal() override;

private:
    CFastMutex m_NAProcessorMutex;

    // arguments
    TSeq_ids m_Seq_ids;

    // processing data
    TAnnotInfos m_AnnotInfos;

    // result:
    TResults m_Results;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGL_LocalCDDBlob_Processor
/////////////////////////////////////////////////////////////////////////////

class CPSGL_LocalCDDBlob_Processor : public CPSGL_Blob_Processor
{
public:
    explicit
    CPSGL_LocalCDDBlob_Processor(CTSE_Chunk_Info& cdd_chunk_info,
                                 const SCDDIds& cdd_ids,
                                 CDataSource* data_source,
                                 CPSGCaches* caches = nullptr,
                                 bool add_wgs_master = false);
    ~CPSGL_LocalCDDBlob_Processor() override;

    const char* GetProcessorName() const override;
    ostream& PrintProcessorArgs(ostream& out) const override;

    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessReplySlow(EPSG_Status status,
                                    const shared_ptr<CPSG_Reply>& reply) override;

    CConstRef<CPsgBlobId> CreateDLBlobId(STSESlot* tse_slot) override;

private:
    CFastMutex m_CDDProcessorMutex;
    
    // arguments
    CTSE_Chunk_Info& m_CDDChunkInfo;
    SCDDIds m_CDDIds;

    // processing data
    string m_PSG_Blob_id;
};


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSORS__HPP
