#ifndef OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
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
 * Authors:  Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description:
 *   Processor of ID2 requests for CDD annotations
 *
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/cdd/id2proc_cdd/id2cdd.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_access__.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/id2/id2processor.hpp>

BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);


class CID2CDDProcessorContext : public CID2ProcessorContext
{
public:
    CID2CDDContext m_Context;
};


class NCBI_ID2PROC_CDD_EXPORT CID2CDDProcessorPacketContext : public CID2ProcessorPacketContext
{
public:
    typedef CRef<CID2_Reply> TID2ReplyPtr;
    typedef map<int, TID2ReplyPtr> TReplies;
    typedef map<int, CConstRef<CSeq_id> > TSeqIDs;
    typedef map<int, CConstRef<CID2_Blob_Id> > TBlobIDs;

    TReplies m_Replies;
    TSeqIDs  m_SeqIDs;
    TBlobIDs m_BlobIDs;
};


class NCBI_ID2PROC_CDD_EXPORT CID2CDDProcessor_Impl : public CObject
{
public:
    CID2CDDProcessor_Impl(const CConfig::TParamTree* params = 0,
                          const string& driver_name = kEmptyStr);
    ~CID2CDDProcessor_Impl(void);

    typedef CID2CDDProcessor::TReplies TReplies;

    CRef<CID2ProcessorPacketContext> ProcessPacket(CID2CDDProcessorContext* context,
                                                   CID2_Request_Packet& packet,
                                                   TReplies& replies);

    void ProcessReply(CID2ProcessorContext* context,
                      CID2ProcessorPacketContext* packet_context,
                      CID2_Reply& reply,
                      TReplies& replies);

    CRef<CID2CDDProcessorContext> CreateContext(void);
    const CID2CDDContext& GetInitialContext(void) const {
        return m_InitialContext;
    }
    void InitContext(CID2CDDContext& context, const CID2_Request& main_request);

private:
    void x_TranslateReplies(const CID2CDDContext& context,
                            CID2CDDProcessorPacketContext& packet_context);
    CRef<CID2_Reply> x_GetBlobId(const CID2CDDContext& context,
                                 int serial_number,
                                 const CID2_Seq_id& req_id);
    void x_TranslateBlobIdReply(CRef<CID2_Reply> id2_reply,
                                CConstRef<CSeq_id> id,
                                CConstRef<CCDD_Reply> cdd_reply);
    CRef<CID2_Reply> x_GetBlob(const CID2CDDContext& context,
                               int serial_number,
                               const CID2_Blob_Id& blob_id);
    void x_TranslateBlobReply(CRef<CID2_Reply> id2_reply,
                              const CID2CDDContext& context,
                              const CID2_Blob_Id& blob_id,
                              CRef<CCDD_Reply> cdd_reply);
    CRef<CID2_Reply> x_CreateID2_Reply(int serial_number,
                                       const CCDD_Reply& cdd_reply);

    CID2CDDContext m_InitialContext;
    CRef<CCDDClient> m_Client;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD_IMPL__ID2CDD_IMPL__HPP
