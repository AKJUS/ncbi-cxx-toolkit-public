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
 *`
 * Author:  Colleen Bollin, Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   For validating individual features
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_FEAT__HPP
#define VALIDATOR___VALIDERROR_FEAT__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/feature_match.hpp>
#include <objtools/validator/gene_cache.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/translation_problems.hpp>
#include <objtools/validator/splice_problems.hpp>

#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CSeq_graph;
class CMappedGraph;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CByte_graph;
class CDelta_seq;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;
class CCountries;
class CInferencePrefixList;
class CComment_set;
class CTaxon3_reply;
class ITaxon3;
class CT3Error;

BEGIN_SCOPE(validator)

class CValidError_imp;
class CTaxValidationAndCleanup;
class CGeneCache;
class CValidError_base;

// =============================  Validate SeqFeat  ============================



class NCBI_VALIDATOR_EXPORT CValidError_feat : private validator::CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void SetScope(CScope& scope) { m_Scope = &scope; }
    void SetTSE(CSeq_entry_Handle seh);

    void ValidateSeqFeat(
        const CSeq_feat& feat);
    void ValidateSeqFeatWithParent(const CSeq_feat& feat, const CTSE_Handle& tse);
    void ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq);

    enum EInferenceValidCode {
        eInferenceValidCode_valid = 0,
        eInferenceValidCode_empty,
        eInferenceValidCode_bad_prefix,
        eInferenceValidCode_bad_body,
        eInferenceValidCode_single_field,
        eInferenceValidCode_spaces,
        eInferenceValidCode_comment,
        eInferenceValidCode_same_species_misused,
        eInferenceValidCode_bad_accession,
        eInferenceValidCode_bad_accession_version,
        eInferenceValidCode_accession_version_not_public,
        eInferenceValidCode_bad_accession_type,
        eInferenceValidCode_unrecognized_database
    };

    static vector<string> GetAccessionsFromInferenceString (string inference, string &prefix, string &remainder, bool &same_species);
    static bool GetPrefixAndAccessionFromInferenceAccession (string inf_accession, string &prefix, string &accession);
    static EInferenceValidCode ValidateInferenceAccession (string accession, bool fetch_accession, bool is_similar_to);
    static EInferenceValidCode ValidateInference(string inference, bool fetch_accession);

    // functions expected to be used in Discrepancy Report
    bool DoesCDSHaveShortIntrons(const CSeq_feat& feat);
    bool IsIntronShort(const CSeq_feat& feat);
    void ValidatemRNAGene (const CSeq_feat &feat);
    bool GetTSACDSOnMinusStrandErrors(const CSeq_feat& feat, const CBioseq& seq);

private:

    CSeq_entry_Handle  m_TSE;
    CGeneCache         m_GeneCache;
    CCacheImpl         m_SeqCache;

    CBioseq_Handle x_GetCachedBsh(const CSeq_loc& loc);

    void x_ValidateSeqFeatLoc(const CSeq_feat& feat);
    static size_t x_FindStartOfGap (CBioseq_Handle bsh, int pos, CScope* scope);
    void ValidateSeqFeatData(const CSeqFeatData& data, const CSeq_feat& feat);
    void ValidateSeqFeatProduct(const CSeq_loc& prod, const CSeq_feat& feat, CBioseq_Handle prot);
    void ValidateGene(const CGene_ref& gene, const CSeq_feat& feat);
    void ValidateGeneXRef(const CSeq_feat& feat);
    void ValidateGeneFeaturePair(const CSeq_feat& feat, const CSeq_feat& gene);
    static bool x_FindProteinGeneXrefByKey(CBioseq_Handle bsh, const string& key);
    static bool x_FindGeneToMatchGeneXref(const CGene_ref& xref, CSeq_entry_Handle seh);
    void ValidateOperon(const CSeq_feat& feat);
    void ValidateGeneCdsPair(const CSeq_feat& gene);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateCdTrans(const CSeq_feat& feat, bool &nonsense_intron);




    void x_ReportTranslExceptProblems(const CCDSTranslationProblems::TTranslExceptProblems& problems, const CSeq_feat& feat, bool has_exception);
    void x_ReportCDSTranslationProblems(const CSeq_feat& feat, const CCDSTranslationProblems& problems, bool far_product);

    CBioseq_Handle x_GetCDSProduct(const CSeq_feat& feat, bool& is_far);
    CBioseq_Handle x_GetRNAProduct(const CSeq_feat& feat, bool& is_far);
    CBioseq_Handle x_GetFeatureProduct(const CSeq_feat& feat, bool look_far, bool& is_far, bool& is_misplaced);
    CBioseq_Handle x_GetFeatureProduct(const CSeq_feat& feat, bool& is_far, bool& is_misplaced);

    void x_ReportTranslationMismatches(const CCDSTranslationProblems::TTranslationMismatches& mismatches, const CSeq_feat& feat, bool far_product);

    void ValidateCdsProductId(const CSeq_feat& feat);
    void ValidateCdConflict(const CCdregion& cdregion, const CSeq_feat& feat);

    EDiagSev x_SeverityForConsensusSplice(void);
    void ValidateSplice(const CSeq_feat& feat, bool check_all = false);
    void ValidateBothStrands(const CSeq_feat& feat);
    static void x_FeatLocHasBadStrandBoth(const CSeq_feat& feat, bool& both, bool& both_rev);
    void ValidateCommonCDSProduct(const CSeq_feat& feat);
    void ValidateFarProducts(const CSeq_feat& feat);
    void x_ValidateGeneId(const CSeq_feat& feat);
    void ValidateBadMRNAOverlap(const CSeq_feat& feat);
    bool x_CDSHasGoodParent(const CSeq_feat& feat);
    void ValidateCDSPartial(const CSeq_feat& feat);
    bool x_ValidateCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
        const CCdregion& cdregion, bool report_erros);

    void x_ValidateCdregionCodebreak(const CSeq_feat& feat);

    void ValidateProt(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProteinName(const string& prot_name, const CSeq_feat& feat);
    void x_ReportUninformativeNames(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProtECNumbers(const CProt_ref& prot, const CSeq_feat& feat);

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat, bool mustbemethionine);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ReportMRNATranslationProblems(size_t problems, const CSeq_feat& feat, size_t mismatches);

    void ValidateCommonMRNAProduct(const CSeq_feat& feat);
    void ValidateRnaProductType(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateIntron(const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeat (const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateNonImpFeatGbquals(const CSeq_feat& feat);

    // these functions are for validating individual genbank qualifier values
    void ValidateRptUnitVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitSeqVal (const string& val, const string& key, const CSeq_feat& feat);
    void ValidateRptUnitRangeVal (const string& val, const CSeq_feat& feat);
    void ValidateLabelVal (const string& val, const CSeq_feat& feat);
    void ValidateCompareVal (const string& val, const CSeq_feat& feat);

    void ValidateGapFeature (const CSeq_feat& feat);

    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);
    void ValidateSeqFeatXref(const CSeq_feat& feat);
    void ValidateSeqFeatXref(const CSeq_feat& feat, const CTSE_Handle& tse);
    void ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat);
    void ValidateSeqFeatXref(const CSeqFeatXref& xref, const CSeq_feat& feat, const CTSE_Handle& tse);
    void x_ValidateSeqFeatExceptXref(const CSeq_feat& feat);

    // does feat have an xref to a feature other than the one specified by id with the same subtype
    static bool HasNonReciprocalXref(const CSeq_feat& feat, 
                                     const CFeat_id& id, CSeqFeatData::ESubtype subtype,
                                     const CTSE_Handle& tse);
    void ValidateOneFeatXrefPair(const CSeq_feat& feat, const CSeq_feat& far_feat, const CSeqFeatXref& xref);
    void ValidateExtUserObject (const CUser_object& user_object, const CSeq_feat& feat);
    void ValidateGoTerms (CUser_object::TData field_list, const CSeq_feat& feat, vector<pair<string, string> >& id_terms);

    void ValidateFeatCit(const CPub_set& cit, const CSeq_feat& feat);
    void ValidateFeatComment(const string& comment, const CSeq_feat& feat);
    void ValidateFeatBioSource(const CBioSource& bsrc, const CSeq_feat& feat);

    void x_ValidateGbQual(const CGb_qual& qual, const CSeq_feat& feat);

    static bool IsPlastid(int genome);
    bool IsOverlappingGenePseudo(const CSeq_feat& feat, CScope* scope);

    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

    void ValidateCharactersInField (string value, string field_name, const CSeq_feat& feat);
    void x_ReportECNumFileStatus(const CSeq_feat& feat);

    void ReportSpliceProblems(const CSpliceProblems& problems, const string& label, const CSeq_feat& feat);
    void ReportDonorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label, const CSeq_feat& feat);
    void ReportAcceptorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label, const CSeq_feat& feat);

    bool x_CDS3primePartialTest(const CSeq_feat& feat);
    bool x_CDS5primePartialTest(const CSeq_feat& feat);
    bool x_IsNTNCNWACAccession(const CSeq_loc& loc);
    bool x_HasNonReciprocalXref(const CSeq_feat& feat, const CFeat_id& id, CSeqFeatData::ESubtype subtype);
    bool x_LocIsNmAccession(const CSeq_loc& loc);
    void x_ReportMisplacedCodingRegionProduct(const CSeq_feat& feat);

};


class CSingleFeatValidator
{
public:
    CSingleFeatValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);
    virtual ~CSingleFeatValidator() {};

    virtual void Validate();

protected:
    const CSeq_feat& m_Feat;
    CScope& m_Scope;
    CValidError_imp& m_Imp;

    void PostErr(EDiagSev sv, EErrType et, const string& msg);

    void x_ValidateBothStrands();
    static void x_LocHasStrandBoth(const CSeq_loc& feat, bool& both, bool& both_rev);
    void x_ValidateGeneId();
    void x_ValidateFeatCit();
    virtual void x_ValidateFeatComment();
    void x_ValidateGbQual(const CGb_qual& qual);
    void x_ReportECNumFileStatus();

    void x_ValidateExtUserObject();
    void x_ValidateGoTerms(CUser_object::TData field_list, vector<pair<string, string> >& id_terms);

    bool x_HasNamedQual(const string& qual_name);

};

class CCdregionValidator : public CSingleFeatValidator
{
public:
    CCdregionValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {};

protected:
    virtual void x_ValidateFeatComment();
};


CSingleFeatValidator* FeatValidatorFactory(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_FEAT__HPP */
