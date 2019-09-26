#ifndef PSG_CACHE__HPP
#define PSG_CACHE__HPP

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
 * File Description: PSG LMDB-based cache
 *
 */

#include <corelib/ncbistd.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

BEGIN_NCBI_SCOPE

class CPubseqGatewayCacheBioseqInfo;
class CPubseqGatewayCacheSi2Csi;
class CPubseqGatewayCacheBlobProp;

class CPubseqGatewayCache
{
    struct SRuntimeError {
        explicit SRuntimeError(const string& msg)
            : message(msg)
        {}
        string message;
    };

 public:
    using TRuntimeError = SRuntimeError;
    using TRuntimeErrorList = deque<SRuntimeError>;

    static const size_t kRuntimeErrorLimit;

    CPubseqGatewayCache(
        const string& bioseq_info_file_name, const string& si2csi_file_name, const string& blob_prop_file_name);
    virtual ~CPubseqGatewayCache();
    void Open(const vector<string>& sat_names);

    void ResetErrors();
    const TRuntimeErrorList& GetErrors() const
    {
        return m_RuntimeErrors;
    }

/* bioseq_info */
    bool LookupBioseqInfoByAccession(
        const string& accession, string& data, int& found_version, int& found_seq_id_type, int64_t& found_gi);
    bool LookupBioseqInfoByAccessionVersion(
        const string& accession, int version, string& data, int& found_seq_id_type, int64_t& found_gi);
    bool LookupBioseqInfoByAccessionVersionSeqIdType(
        const string& accession, int version, int seq_id_type, string& data, int64_t& found_gi);
    bool LookupBioseqInfoByAccessionVersionSeqIdType(
        const string& accession, int version, int seq_id_type,
        string& data, int& found_version, int& found_seq_id_type, int64_t& found_gi);
    bool LookupBioseqInfoByAccessionGi(
        const string& accession, int64_t gi, string& data, int& found_version, int& found_seq_id_type);
    bool LookupBioseqInfoByAccessionVersionSeqIdTypeGi(
        const string& accession, int version, int seq_id_type, int64_t gi, string& data);

    static string PackBioseqInfoKey(const string& accession, int version);
    static string PackBioseqInfoKey(const string& accession, int version, int seq_id_type);
    static string PackBioseqInfoKey(const string& accession, int version, int seq_id_type, int64_t gi);
    static bool UnpackBioseqInfoKey(
        const char* key, size_t key_sz, int& version, int& seq_id_type, int64_t& gi);
    static bool UnpackBioseqInfoKey(
        const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type, int64_t& gi);

/* si2csi */
    bool LookupCsiBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data);
    bool LookupCsiBySeqIdSeqIdType(const string& sec_seqid, int sec_seq_id_type, string& data);

    static string PackSiKey(const string& sec_seqid, int sec_seq_id_type);
    static bool UnpackSiKey(const char* key, size_t key_sz, int& sec_seq_id_type);

/* blob_prop */
    bool LookupBlobPropBySatKey(int32_t sat, int32_t sat_key, int64_t& last_modified, string& data);
    bool LookupBlobPropBySatKeyLastModified(int32_t sat, int32_t sat_key, int64_t last_modified, string& data);

    static string PackBlobPropKey(int32_t sat_key);
    static string PackBlobPropKey(int32_t sat_key, int64_t last_modified);
    static bool UnpackBlobPropKey(const char* key, size_t key_sz, int64_t& last_modified);
    static bool UnpackBlobPropKey(const char* key, size_t key_sz, int64_t& last_modified, int32_t& sat_key);

 private:
    string m_BioseqInfoPath;
    string m_Si2CsiPath;
    string m_BlobPropPath;
    unique_ptr<CPubseqGatewayCacheBioseqInfo> m_BioseqInfoCache;
    unique_ptr<CPubseqGatewayCacheSi2Csi> m_Si2CsiCache;
    unique_ptr<CPubseqGatewayCacheBlobProp> m_BlobPropCache;
    TRuntimeErrorList m_RuntimeErrors;
};

END_NCBI_SCOPE

#endif  // PSG_CACHE__HPP
