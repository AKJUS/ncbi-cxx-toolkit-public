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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gtf file
 *
 */

#ifndef OBJTOOLS_WRITERS___GTF_WRITER__HPP
#define OBJTOOLS_WRITERS___GTF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGtfRecord;

//  ============================================================================
class CGtfIdGenerator
//  ============================================================================
{
public:
    CGtfIdGenerator() {};

    void Reset()
    {
        mLastSuffixes.clear();
    }
    string NextId(
        const string prefix)
    {
        auto mapIt = mLastSuffixes.find(prefix);
        if (mapIt != mLastSuffixes.end()) {
            ++mapIt->second;
            return prefix + "_" + NStr::NumericToString(mapIt->second);
        }
        mLastSuffixes[prefix] = 1;
        return prefix + "_1";
    }

private:
    map<string, int> mLastSuffixes;
};

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGtfWriter:
    public CGff2Writer
//  ============================================================================
{
public:
    enum {
        fStructibutes = 1<<16,
        fNoGeneFeatures = 1<<17,
        fNoExonNumbers = 1<<18
    };

    CGtfWriter(
        CScope&,
        CNcbiOstream&,
        unsigned int = 0 );
    CGtfWriter(
        CNcbiOstream&,
        unsigned int = 0 );
    ~CGtfWriter();

    bool WriteHeader() override;
    bool WriteHeader(
        const CSeq_annot& annot) override { return CGff2Writer::WriteHeader(annot); };

protected:
    bool x_WriteBioseqHandle(
        CBioseq_Handle) override;

    bool xAssignFeatureType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;

    bool xAssignFeatureMethod(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;

    bool xAssignFeatureAttributesFormatSpecific(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    virtual bool xAssignFeatureAttributeGeneId(
        CGtfRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeTranscriptId(
        CGtfRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeTranscriptBiotype(
        CGtfRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeNote(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    bool xAssignFeatureAttributeDbxref(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    bool xAssignFeatureAttributesQualifiers(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;

    std::string xGenericGeneId(
        const CMappedFeat&,
        CGffFeatureContext& );
    std::string xGenericTranscriptId(
        const CMappedFeat&);

    bool xWriteRecord(
        const CGffWriteRecord* );

    bool xWriteFeature(
        CGffFeatureContext&,
        const CMappedFeat&) override;
    virtual bool xWriteRecordsGene(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteRecordsTranscript(
        CGffFeatureContext&,
        const CMappedFeat&,
        const string& ="" );
    virtual bool xWriteRecordsCds(
        CGffFeatureContext&,
        const CMappedFeat&,
        const string& ="" );
    virtual bool xWriteFeatureExons(
        CGffFeatureContext&,
        const CMappedFeat&,
        const string& ="" );

    virtual bool xAssignFeaturesGene(
        list<CRef<CGtfRecord>>&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeaturesTranscript(
        list<CRef<CGtfRecord>>&,
        CGffFeatureContext&,
        const CMappedFeat&,
        const string&); //transcript ID
    virtual bool xAssignFeaturesCds(
        list<CRef<CGtfRecord>>&,
        CGffFeatureContext&,
        const CMappedFeat&,
        const string& = ""); //transcript ID

private:
    using FEAT_ID = string;
    using FEAT_MAP = map<CMappedFeat, FEAT_ID>;
    using FEAT_IDS = set<FEAT_ID>;

    FEAT_MAP mFeatMap;
    FEAT_IDS mUsedFeatIds;

    using GENE_ID = string;
    using GENE_MAP = map<CMappedFeat, GENE_ID>;
    using GENE_IDS = set<GENE_ID>;

    GENE_IDS mUsedGeneIds;
    GENE_MAP mGeneMap;

    map<CMappedFeat, string> mMapFeatToGeneId;
    CGtfIdGenerator mIdGenerator;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GTF_WRITER__HPP
