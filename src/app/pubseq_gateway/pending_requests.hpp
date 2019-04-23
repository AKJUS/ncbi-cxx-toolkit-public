#ifndef PENDING_REQUESTS__HPP
#define PENDING_REQUESTS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include "pubseq_gateway_types.hpp"
#include "pubseq_gateway_utils.hpp"


// Contains all the data required for a blob request which could be retrieved
// by sat/sat_key or seq_id/seq_id_type
struct SBlobRequest
{
    // Construct the request for the case of sat/sat_key request
    SBlobRequest(const SBlobId &  blob_id,
                 int64_t  last_modified,
                 ETSEOption  tse_option,
                 ECacheAndCassandraUse  use_cache,
                 const string &  client_id) :
        m_TSEOption(tse_option),
        m_BlobIdType(eBySatAndSatKey),
        m_UseCache(use_cache),
        m_ClientId(client_id),
        m_BlobId(blob_id),
        m_LastModified(last_modified)
    {}

    SBlobRequest() = default;

    // Construct the request for the case of seq_id/id_type request
    SBlobRequest(const CTempString &  seq_id,
                 int  seq_id_type,
                 vector<SBlobId> &  exclude_blobs,
                 ETSEOption  tse_option,
                 ECacheAndCassandraUse  use_cache,
                 const string &  client_id) :
        m_TSEOption(tse_option),
        m_BlobIdType(eBySeqId),
        m_UseCache(use_cache),
        m_ClientId(client_id),
        m_LastModified(INT64_MIN),
        m_SeqId(seq_id.data(), seq_id.size()),
        m_SeqIdType(seq_id_type),
        m_ExcludeBlobs(std::move(exclude_blobs))
    {}

    EBlobIdentificationType  GetBlobIdentificationType(void) const
    {
        return m_BlobIdType;
    }

    bool IsExcludedBlob(void) const
    {
        // NOTE: in practice it makes sense to check it only if the
        //       identification type is seq_id/seq_id_type
        // However the m_ExcludeBlobs will be empty for the sat.sat_key
        // identification anyway
        for (const auto &  item : m_ExcludeBlobs) {
            if (item == m_BlobId)
                return true;
        }
        return false;
    }

public:
    ETSEOption                  m_TSEOption;
    EBlobIdentificationType     m_BlobIdType;
    ECacheAndCassandraUse       m_UseCache;
    string                      m_ClientId;

    // Fields in case of request by sat/sat_key
    SBlobId                     m_BlobId;
    int64_t                     m_LastModified;

    // Fields in case of request by seq_id/seq_id_type
    // NB: need a copy because it could be an asynchronous request
    string                      m_SeqId;
    int                         m_SeqIdType;
    vector<SBlobId>             m_ExcludeBlobs;
};


struct SResolveRequest
{
public:
    SResolveRequest(const CTempString &  seq_id,
                    int  seq_id_type,
                    TServIncludeData  include_data_flags,
                    EOutputFormat  output_format,
                    ECacheAndCassandraUse  use_cache,
                    bool  use_psg_protocol) :
        m_SeqId(seq_id.data(), seq_id.size()), m_SeqIdType(seq_id_type),
        m_IncludeDataFlags(include_data_flags),
        m_OutputFormat(output_format),
        m_UseCache(use_cache),
        m_UsePsgProtocol(use_psg_protocol)
    {}

    SResolveRequest() = default;

public:
    string                      m_SeqId;
    int                         m_SeqIdType;
    TServIncludeData            m_IncludeDataFlags;
    EOutputFormat               m_OutputFormat;
    ECacheAndCassandraUse       m_UseCache;
    bool                        m_UsePsgProtocol;
};


struct SAnnotRequest
{
public:
    SAnnotRequest(const CTempString &  seq_id,
                  int  seq_id_type,
                  vector<CTempString>  names,
                  ECacheAndCassandraUse  use_cache) :
        m_SeqId(seq_id.data(), seq_id.size()), m_SeqIdType(seq_id_type),
        m_UseCache(use_cache)
    {
        for (const auto &  name : names)
            m_Names.push_back(string(name.data(), name.size()));
    }

    SAnnotRequest() = default;

public:
    string                      m_SeqId;
    int                         m_SeqIdType;
    vector<string>              m_Names;
    ECacheAndCassandraUse       m_UseCache;
};


#endif
