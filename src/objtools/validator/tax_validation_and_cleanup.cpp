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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Tools for batch processing taxonomy-related validation and cleanup
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/validator/validatorp.hpp>

#include <serial/iterator.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>

#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objtools/validator/tax_validation_and_cleanup.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;

const string kInvalidReplyMsg = "Taxonomy service returned invalid reply";


CQualifierRequest::CQualifierRequest()
{
    x_Init();
}


void CQualifierRequest::x_Init()
{
    m_ValuesToTry.clear();
    m_RepliesProcessed = 0;
    m_Descs.clear();
    m_Feats.clear();
}


void CQualifierRequest::AddParent(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx)
{
    m_Descs.push_back(TDescPair(desc, ctx));
}


void CQualifierRequest::AddParent(CConstRef<CSeq_feat> feat)
{
    m_Feats.push_back(feat);
}


void CQualifierRequest::AddRequests(vector<CRef<COrg_ref> >& request_list) const
{
    for (auto it = m_ValuesToTry.begin(); it != m_ValuesToTry.end(); it++) {
        CRef<COrg_ref> rq(new COrg_ref);
        rq->SetTaxname(*it);
        request_list.push_back(rq);
    }
}


bool CQualifierRequest::MatchTryValue(const string& val) const
{
    for (auto it = m_ValuesToTry.begin(); it != m_ValuesToTry.end(); it++) {
        if (NStr::EqualNocase(val, *it)) {
            return true;
        }
    }
    return false;
}


CSpecificHostRequest::CSpecificHostRequest(const string& host, const COrg_ref& org) :
    CQualifierRequest(), m_Host(host), m_Response(eUnrecognized)
{
    string host_check = SpecificHostValueToCheck(host);
    if (NStr::IsBlank(host_check)) {
        m_Response = eNormal;
        return;
    }
    m_ValuesToTry.push_back(host_check);
    if (!NStr::Equal(host, host_check)) {
        m_ValuesToTry.push_back(host);
    }
    m_SuggestedFix.clear();
}


void CSpecificHostRequest::AddReply(const CT3Reply& reply)
{
    if (m_Response == eUnrecognized) {
        m_Error = InterpretSpecificHostResult(m_ValuesToTry[m_RepliesProcessed], reply, m_Host);
        if (NStr::IsBlank(m_Error)) {
            m_Response = eNormal;
            m_SuggestedFix = m_Host;
        } else if (NStr::Find(m_Error, "ambiguous") != NPOS) {
            m_Response = eAmbiguous;
        } else if (NStr::StartsWith(m_Error, "Invalid value for specific host") && !IsLikelyTaxname(m_Host)) {
            m_Response = eNormal;
            m_SuggestedFix = m_Host;
        } else {
            m_Response = eUnrecognized;
            if (NStr::IsBlank(m_SuggestedFix) && reply.IsData()) {
                if (HasMisSpellFlag(reply.GetData()) && reply.GetData().IsSetOrg()) {
                    m_SuggestedFix = reply.GetData().GetOrg().GetTaxname();
                } else if (reply.GetData().IsSetOrg()) {
                    if (!FindMatchInOrgRef(m_Host, reply.GetData().GetOrg())
                        && !IsCommonName(reply.GetData())) {
                        m_SuggestedFix = reply.GetData().GetOrg().GetTaxname();
                    }
                }
            }
        }
    }
    m_RepliesProcessed++;
}


void CSpecificHostRequest::PostErrors(CValidError_imp& imp)
{
    switch (m_Response) {
        case eNormal:
            break;
        case eAmbiguous:
            for (auto it = m_Descs.begin(); it != m_Descs.end(); it++) {
                imp.PostObjErr(eDiag_Info, eErr_SEQ_DESCR_AmbiguousSpecificHost, m_Error, *(it->first), it->second);
            }
            for (auto it = m_Feats.begin(); it != m_Feats.end(); it++) {
                imp.PostObjErr(eDiag_Info, eErr_SEQ_DESCR_AmbiguousSpecificHost, m_Error, **it);
            }
            break;
        case eUnrecognized:
            for (auto it = m_Descs.begin(); it != m_Descs.end(); it++) {
                imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, m_Error, *(it->first), it->second);
            }
            for (auto it = m_Feats.begin(); it != m_Feats.end(); it++) {
                imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, m_Error, **it);
            }
            break;        
    }
}


const string& CSpecificHostRequest::SuggestFix() const
{
    return m_ValuesToTry.empty() ? m_Host : m_SuggestedFix;
}


CStrainRequest::CStrainRequest(const string& strain, const COrg_ref& org) : CQualifierRequest(), m_Strain(strain)
{ 
    if (org.IsSetTaxname()) {
        m_Taxname = org.GetTaxname();
    } else {
        m_Taxname.clear();
    }

    if (NStr::IsBlank(strain)) {
        m_IsInvalid = true;
        return;
    }
    m_IsInvalid = false;

    m_ValuesToTry.push_back(strain);
    size_t pos = 0;
    while (strain[pos] != 0 && isalpha(strain[pos])) {
        ++pos;
    }
    if (pos < strain.length()) {
        m_ValuesToTry.push_back(strain.substr(0, pos));
    }

    if (RequireTaxname(m_Taxname)) {
        m_ValuesToTry.push_back(MakeKey(m_Taxname, strain));
    }
}


string CStrainRequest::MakeKey(const string& strain, const string& taxname)
{
    if (RequireTaxname(taxname)) {
        return taxname + " " + strain;
    } else {
        return strain;
    }
}


bool CStrainRequest::RequireTaxname(const string& taxname)
{
    if (NStr::EndsWith(taxname, " sp.")) {
        return true;
    } else {
        return false;
    }
}


bool CStrainRequest::x_IsUnwanted(const string& str)
{
    if (NStr::FindNoCase(str, "virus") != NPOS ||
        NStr::FindNoCase(str, "viroid") != NPOS ||
        NStr::FindNoCase(str, "vector") != NPOS ||
        NStr::FindNoCase(str, "phage") != NPOS) {
        return true;
    } else {
        return false;
    }
}


bool CStrainRequest::Check(const COrg_ref& org)
{
    if (org.IsSetLineage() && x_IsUnwanted(org.GetLineage())) {
        return false;
    }
    if (org.IsSetTaxname() && x_IsUnwanted(org.GetTaxname())) {
        return false;
    }
    if (!org.IsSetOrgMod()) {
        return false;
    }
    return true;
}


void CStrainRequest::PostErrors(CValidError_imp& imp)
{
    if (m_IsInvalid) {
        for (auto it = m_Descs.begin(); it != m_Descs.end(); it++) {
            imp.PostObjErr(eDiag_Info, eErr_SEQ_FEAT_InvalidQualifierValue, "Strain '" + m_Strain + "' contains taxonomic name information", *(it->first), it->second);
        }
        for (auto it = m_Feats.begin(); it != m_Feats.end(); it++) {
            imp.PostObjErr(eDiag_Info, eErr_SEQ_FEAT_InvalidQualifierValue, "Strain '" + m_Strain + "' contains taxonomic name information", **it);
        }
    }
}


void CStrainRequest::AddReply(const CT3Reply& reply)
{
    if (!m_IsInvalid) {
        if (reply.IsData() && reply.GetData().IsSetOrg()) {
            // TODO: if using just a one word input, make sure name is actually in taxname
            if (m_ValuesToTry[m_RepliesProcessed].length() < m_Strain.length()) {
                if (NStr::EqualNocase(m_ValuesToTry[m_RepliesProcessed], reply.GetData().GetOrg().GetTaxname())) {
                    m_IsInvalid = true;
                }
            } else {
                m_IsInvalid = true;
            }
        }
    }
    m_RepliesProcessed++;
}


void CQualLookupMap::AddDesc(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx)
{
    m_Populated = true;
    if (!desc->IsSource() || !desc->GetSource().IsSetOrg()) {
        return;
    }
    const COrg_ref& org = desc->GetSource().GetOrg();
    if (!org.IsSetOrgMod()) {
        return;
    }
    if (!Check(org)) {
        return;
    }
    for (auto mod_it = org.GetOrgname().GetMod().begin(); mod_it != org.GetOrgname().GetMod().end(); mod_it++) {
        if ((*mod_it)->IsSetSubtype()
            && (*mod_it)->GetSubtype() == m_Subtype
            && (*mod_it)->IsSetSubname()) {
            string qual = (*mod_it)->GetSubname();
            string key = GetKey(qual, org);
            TQualifierRequests::iterator find = m_Map.find(key);
            if (find == m_Map.end()) {
                m_Map[key] = x_MakeNewRequest(qual, org);
                m_Map[key]->AddParent(desc, ctx);
            } else {
                find->second->AddParent(desc, ctx);
            }
        }
    }
}


void CQualLookupMap::AddFeat(CConstRef<CSeq_feat> feat)
{
    m_Populated = true;
    if (!feat->IsSetData() || !feat->GetData().IsBiosrc() ||
        !feat->GetData().GetBiosrc().IsSetOrg()) {
        return;
    }
    const COrg_ref& org = feat->GetData().GetBiosrc().GetOrg();
    if (!org.IsSetOrgMod()) {
        return;
    }
    if (!Check(org)) {
        return;
    }
    for (auto mod_it = org.GetOrgname().GetMod().begin(); mod_it != org.GetOrgname().GetMod().end(); mod_it++) {
        if ((*mod_it)->IsSetSubtype()
            && (*mod_it)->GetSubtype() == COrgMod::eSubtype_nat_host
            && (*mod_it)->IsSetSubname()) {
            string qual = (*mod_it)->GetSubname();
            string key = GetKey(qual, feat->GetData().GetBiosrc().GetOrg());
            TQualifierRequests::iterator find = m_Map.find(key);
            if (find == m_Map.end()) {
                m_Map[key] = x_MakeNewRequest(qual, feat->GetData().GetBiosrc().GetOrg());
                m_Map[key]->AddParent(feat);
            } else {
                find->second->AddParent(feat);
            }
        }
    }
}


vector<CRef<COrg_ref> > CQualLookupMap::GetRequestList()
{
    vector<CRef<COrg_ref> > org_rq_list;
    org_rq_list.reserve(m_Map.size());
    for (auto it = m_Map.begin(); it != m_Map.end(); it++) {
        it->second->AddRequests(org_rq_list);
    }
    return org_rq_list;
}


CQualLookupMap::TQualifierRequests::iterator CQualLookupMap::x_FindRequest(const string& val)
{
    TQualifierRequests::iterator map_it = m_Map.find(val);
    if (map_it != m_Map.end() && map_it->second->NumRemainingReplies() > 0) {
        return map_it;
    }
    map_it = m_Map.begin();
    while (map_it != m_Map.end()) {
        if (map_it->second->MatchTryValue(val) && map_it->second->NumRemainingReplies() > 0) {
            return map_it;
        }
        ++map_it;
    }
    return m_Map.end();
}


string CQualLookupMap::IncrementalUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    string error_message = kEmptyStr;
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    vector<CRef<COrg_ref> >::const_iterator rq_it = input.begin();

    while (reply_it != reply.GetReply().end() && rq_it != input.end()) {
        TQualifierRequests::iterator map_it = x_FindRequest((*rq_it)->GetTaxname());
        if (map_it == m_Map.end()) {
            error_message = "Unexpected taxonomy response for " + (*rq_it)->GetTaxname();
            return error_message;
        }
        map_it->second->AddReply(**reply_it);
        ++rq_it;
        ++reply_it;
    }

    if (reply_it != reply.GetReply().end()) {
        error_message = "Unexpected taxonomy responses for " + COrgMod::GetSubtypeName(m_Subtype);
    }
    return kEmptyStr;
}


bool CQualLookupMap::IsUpdateComplete() const
{
    TQualifierRequests::const_iterator rq_it = m_Map.cbegin();
    while (rq_it != m_Map.cend()) {
        if (rq_it->second->NumRemainingReplies() > 0) {
            return false;
            break;
        }
        ++rq_it;
    }
    return true;
}


void CQualLookupMap::PostErrors(CValidError_imp& imp)
{
    TQualifierRequests::iterator rq_it = m_Map.begin();
    while (rq_it != m_Map.end()) {
        rq_it->second->PostErrors(imp);
        ++rq_it;
    }
}


CRef<CQualifierRequest> CSpecificHostMap::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CSpecificHostRequest(orig_val, org));
    return rq;
}


CRef<CQualifierRequest> CSpecificHostMapForFix::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CSpecificHostRequest(orig_val, org));
    return rq;
}


string CSpecificHostMapForFix::x_DefaultSpecificHostAdjustments(const string& host_val)
{
    string adjusted = host_val;
    NStr::TruncateSpacesInPlace(adjusted);
    adjusted = COrgMod::FixHost(adjusted);
    return adjusted;
}


bool CSpecificHostMapForFix::ApplyToOrg(COrg_ref& org_ref) const
{
    if (!org_ref.IsSetOrgname() ||
        !org_ref.GetOrgname().IsSetMod()) {
        return false;
    }

    bool changed = false;

    for (auto m = org_ref.SetOrgname().SetMod().begin(); m != org_ref.SetOrgname().SetMod().end(); m++) {
        if ((*m)->IsSetSubtype() &&
            (*m)->GetSubtype() == COrgMod::eSubtype_nat_host &&
            (*m)->IsSetSubname()) {
            string host_val = x_DefaultSpecificHostAdjustments((*m)->GetSubname());
            TQualifierRequests::const_iterator it = m_Map.find(host_val);
            if (it != m_Map.end()) {
                const CSpecificHostRequest* rq = dynamic_cast<const CSpecificHostRequest *>(it->second.GetPointer());
                string new_val = x_DefaultSpecificHostAdjustments(rq->SuggestFix());
                if (!NStr::IsBlank(new_val) && !NStr::Equal(new_val, (*m)->GetSubname())) {
                    (*m)->SetSubname(new_val);
                    changed = true;
                }
            }
        }
    }

    return changed;
}


CRef<CQualifierRequest> CStrainMap::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CStrainRequest(orig_val, org));
    return rq;
}


CTaxValidationAndCleanup::CTaxValidationAndCleanup()
{
    m_SrcDescs.clear();
    m_DescCtxs.clear();
    m_SrcFeats.clear();
    m_SpecificHostRequests.clear();
    m_SpecificHostRequestsBuilt = false;
    m_SpecificHostRequestsUpdated = false;
    m_StrainRequestsBuilt = false;
}


void CTaxValidationAndCleanup::Init(const CSeq_entry& se)
{
    m_SrcDescs.clear();
    m_DescCtxs.clear();
    m_SrcFeats.clear();
    m_SpecificHostRequests.clear();
    m_SpecificHostRequestsBuilt = false;
    m_SpecificHostRequestsUpdated = false;
    m_StrainRequestsBuilt = false;
    x_GatherSources(se);
}


CConstRef<CSeq_entry> CTaxValidationAndCleanup::GetTopReportObject() const
{
    if (!m_DescCtxs.empty()) {
        return m_DescCtxs.front();
    } else {
        return CConstRef<CSeq_entry>(NULL);
    }
}


void CTaxValidationAndCleanup::x_GatherSources(const CSeq_entry& se)
{
    // get source descriptors
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY(it, se)
    {
        if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
            CConstRef<CSeqdesc> desc;
            desc.Reset(*it);
            m_SrcDescs.push_back(desc);
            CConstRef<CSeq_entry> r_se;
            r_se.Reset(&se);
            m_DescCtxs.push_back(r_se);
        }
    }
    // also get features
    FOR_EACH_ANNOT_ON_SEQENTRY(annot_it, se)
    {
        FOR_EACH_SEQFEAT_ON_SEQANNOT(feat_it, **annot_it)
        {
            if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsBiosrc()
                && (*feat_it)->GetData().GetBiosrc().IsSetOrg()) {
                CConstRef<CSeq_feat> feat;
                feat.Reset(*feat_it);
                m_SrcFeats.push_back(feat);
            }
        }
    }

    // if set, recurse
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET(it, se.GetSet())
        {
            x_GatherSources(**it);
        }
    }
}


vector< CRef<COrg_ref> > CTaxValidationAndCleanup::GetTaxonomyLookupRequest() const
{
    // request list for taxon3
    vector< CRef<COrg_ref> > org_rq_list;

    // first do descriptors
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.cbegin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.cbegin();
    while (desc_it != m_SrcDescs.cend() && ctx_it != m_DescCtxs.cend()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*desc_it)->GetSource().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++desc_it;
        ++ctx_it;
    }

    // now do features
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.cbegin();
    while (feat_it != m_SrcFeats.cend()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*feat_it)->GetData().GetBiosrc().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++feat_it;
    }
    return org_rq_list;
}


void CTaxValidationAndCleanup::ReportTaxLookupErrors
(const CTaxon3_reply& reply, 
 CValidError_imp& imp, 
 bool is_insd_patent) const
{
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();

    // process descriptor responses
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.cbegin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.cbegin();

    while (reply_it != reply.GetReply().end()
        && desc_it != m_SrcDescs.cend()
        && ctx_it != m_DescCtxs.cend()) {
        if ((*reply_it)->IsError()) {
            imp.HandleTaxonomyError(
                (*reply_it)->GetError(), eErr_SEQ_DESCR_TaxonomyLookupProblem,
                **desc_it, *ctx_it);

        } else if ((*reply_it)->IsData()) {
            bool is_species_level = true;
            bool is_unidentified = false;
            bool force_consult = false;
            bool has_nucleomorphs = false;
            if ((*reply_it)->GetData().IsSetOrg()) {
                const COrg_ref& orp_req = (*desc_it)->GetSource().GetOrg();
                const COrg_ref& orp_rep = (*reply_it)->GetData().GetOrg();
                if (orp_req.IsSetTaxname() && orp_rep.IsSetTaxname()) {
                    const string& taxname_req = orp_req.GetTaxname();
                    const string& taxname_rep = orp_rep.GetTaxname();
                    if (NStr::Equal(taxname_rep, "unidentified")) {
                        is_unidentified = true;
                    }
                    int taxid_request = orp_req.GetTaxId();
                    int taxid_reply = orp_rep.GetTaxId();

                    if (taxid_request != 0 && taxid_reply != 0 && taxid_request != taxid_reply) {
                        imp.PostObjErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem,
                            "Organism name is '" + taxname_req
                            + "', taxonomy ID should be '" + NStr::IntToString(taxid_reply)
                            + "' but is '" + NStr::IntToString(taxid_request) + "'",
                            **desc_it, *ctx_it);
                    }
                }
            }
            (*reply_it)->GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
            if (!is_species_level && !imp.IsWP()) {
                imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyIsSpeciesProblem,
                    "Taxonomy lookup reports is_species_level FALSE",
                    **desc_it, *ctx_it);
            }
            if (force_consult) {
                if (is_insd_patent && is_unidentified) {
                    force_consult = false;
                }
            }
            if (force_consult) {
                imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyConsultRequired,
                    "Taxonomy lookup reports taxonomy consultation needed",
                    **desc_it, *ctx_it);
            }
            if ((*desc_it)->GetSource().IsSetGenome()) {
                CBioSource::TGenome genome = (*desc_it)->GetSource().GetGenome();
                if (genome == CBioSource::eGenome_nucleomorph
                    && !has_nucleomorphs) {
                    imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyNucleomorphProblem,
                        "Taxonomy lookup does not have expected nucleomorph flag",
                        **desc_it, *ctx_it);
                } else if (genome == CBioSource::eGenome_plastid
                    && (!(*reply_it)->GetData().HasPlastids())) {
                    imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyPlastidsProblem,
                        "Taxonomy lookup does not have expected plastid flag",
                        **desc_it, *ctx_it);
                }
            }

        }
        ++reply_it;
        ++desc_it;
        ++ctx_it;
    }
    // process feat responses
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.cbegin();
    while (reply_it != reply.GetReply().cend()
        && feat_it != m_SrcFeats.end()) {
        if ((*reply_it)->IsError()) {
            imp.HandleTaxonomyError(
                (*reply_it)->GetError(),
                eErr_SEQ_DESCR_TaxonomyLookupProblem, **feat_it);

        } else if ((*reply_it)->IsData()) {
            bool is_species_level = true;
            bool force_consult = false;
            bool has_nucleomorphs = false;
            (*reply_it)->GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
            if (!is_species_level && !imp.IsWP()) {
                imp.PostErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem,
                    "Taxonomy lookup reports is_species_level FALSE",
                    **feat_it);
            }
            if (force_consult) {
                imp.PostErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem,
                    "Taxonomy lookup reports taxonomy consultation needed",
                    **feat_it);
            }
            if ((*feat_it)->GetData().GetBiosrc().IsSetGenome()) {
                CBioSource::TGenome genome = (*feat_it)->GetData().GetBiosrc().GetGenome();
                if (genome == CBioSource::eGenome_nucleomorph
                    && !has_nucleomorphs) {
                    imp.PostErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyNucleomorphProblem,
                        "Taxonomy lookup does not have expected nucleomorph flag",
                        **feat_it);
                } else if (genome == CBioSource::eGenome_plastid
                    && (!(*reply_it)->GetData().HasPlastids())) {
                    imp.PostErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyPlastidsProblem,
                        "Taxonomy lookup does not have expected plastid flag",
                        **feat_it);
                }

            }
        }
        ++reply_it;
        ++feat_it;
    }

}


bool CTaxValidationAndCleanup::AdjustOrgRefsWithTaxLookupReply
( const CTaxon3_reply& reply, 
 vector<CRef<COrg_ref> > org_refs, 
 string& error_message) const
{
    bool changed = false;
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    vector<CRef<COrg_ref> >::iterator org_it = org_refs.begin();
    while (reply_it != reply.GetReply().end() && org_it != org_refs.end()) {
        if ((*reply_it)->IsData() && 
            (*reply_it)->GetData().IsSetOrg()) {
            CRef<COrg_ref> cpy(new COrg_ref());
            cpy->Assign((*reply_it)->GetData().GetOrg());
            cpy->CleanForGenBank();
            if (!cpy->Equals(**org_it)) {
                (*org_it)->Assign(*cpy);
                changed = true;
            }
        }
        ++reply_it;
        ++org_it;
    }
    if (reply_it != reply.GetReply().end()) {
        error_message = "More taxonomy replies than requests!";
    } else if (org_it != org_refs.end()) {
        error_message = "Not enough taxonomy replies!";
    }
    return changed;
}


vector<CRef<COrg_ref> > CTaxValidationAndCleanup::GetSpecificHostLookupRequest(bool for_fix)
{
    if (for_fix) {
        if (!m_HostMapForFix.IsPopulated()) {
            x_CreateQualifierMap(m_HostMapForFix);
        }
        return m_HostMapForFix.GetRequestList();
    } else {
        if (!m_HostMap.IsPopulated()) {
            x_CreateQualifierMap(m_HostMap);
        }
        return m_HostMap.GetRequestList();
    }
}

vector<CRef<COrg_ref> > CTaxValidationAndCleanup::GetStrainLookupRequest()
{
    if (!m_StrainRequestsBuilt) {
        x_CreateStrainMap();
    }

    vector<CRef<COrg_ref> > org_rq_list = m_StrainMap.GetRequestList();
    return org_rq_list;
}


void CTaxValidationAndCleanup::x_CreateQualifierMap(CQualLookupMap& lookup)
{
    //first do descriptors
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.begin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.begin();
    while (desc_it != m_SrcDescs.end() && ctx_it != m_DescCtxs.end()) {
        lookup.AddDesc(*desc_it, *ctx_it);
        ++desc_it;
        ++ctx_it;
    }
    // collect features with specific hosts
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.begin();
    while (feat_it != m_SrcFeats.end()) {
        lookup.AddFeat(*feat_it);
        ++feat_it;
    }

}

void CTaxValidationAndCleanup::x_CreateStrainMap()
{
    x_CreateQualifierMap(m_StrainMap);
    m_StrainRequestsBuilt = true;
}


void CTaxValidationAndCleanup::ReportSpecificHostErrors(CValidError_imp& imp)
{
    m_HostMap.PostErrors(imp);
}

void CTaxValidationAndCleanup::ReportSpecificHostErrors(const CTaxon3_reply& reply, CValidError_imp& imp)
{
    string error_message;
    if (!m_HostMap.IsUpdateComplete()) {
        vector<CRef<COrg_ref> > input = m_HostMap.GetRequestList();
        error_message = m_HostMap.IncrementalUpdate(input, reply);
    }
    if (!NStr::IsBlank(error_message)) {
        imp.PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, error_message, *(GetTopReportObject()));
        return;
    }

    m_HostMap.PostErrors(imp);
}


bool CTaxValidationAndCleanup::AdjustOrgRefsWithSpecificHostReply
(vector<CRef<COrg_ref> > requests, 
 const CTaxon3_reply& reply,
 vector<CRef<COrg_ref> > org_refs, 
 string& error_message)
{
    if (!m_HostMapForFix.IsUpdateComplete()) {
        // need to calculate requests for this list
        m_HostMapForFix.IncrementalUpdate(requests, reply);
    }
    return AdjustOrgRefsForSpecificHosts(org_refs);
}


bool CTaxValidationAndCleanup::AdjustOrgRefsForSpecificHosts(vector<CRef<COrg_ref> > org_refs)
{
    bool changed = false;
    for (auto org = org_refs.begin(); org != org_refs.end(); org++) {
        changed |= m_HostMapForFix.ApplyToOrg(**org);
    }
    return changed;
}


TSpecificHostRequests::iterator CTaxValidationAndCleanup::x_FindHostFixRequest(const string& val)
{
    TSpecificHostRequests::iterator map_it = m_SpecificHostRequests.find(val);
    if (map_it != m_SpecificHostRequests.end() && map_it->second.NumRemainingReplies() > 0) {
        return map_it;
    }
    map_it = m_SpecificHostRequests.begin();
    while (map_it != m_SpecificHostRequests.end()) {
        if (map_it->second.MatchTryValue(val) && map_it->second.NumRemainingReplies() > 0) {
            return map_it;
        }
        ++map_it;
    }
    return m_SpecificHostRequests.end();
}


string CTaxValidationAndCleanup::IncrementalSpecificHostMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    string error_message;
    if (m_HostMap.IsPopulated()) {
        error_message = m_HostMap.IncrementalUpdate(input, reply);
    }
    if (NStr::IsBlank(error_message)) {
        if (m_HostMapForFix.IsPopulated()) {
            error_message = m_HostMapForFix.IncrementalUpdate(input, reply);
        }
    }
    return error_message;
}


bool CTaxValidationAndCleanup::IsSpecificHostMapUpdateComplete() const
{
    if (m_HostMap.IsPopulated()) {
        return m_HostMap.IsUpdateComplete();
    } else if (m_HostMapForFix.IsPopulated()) {
        return m_HostMapForFix.IsUpdateComplete();
    } else {
        return false;
    }
}


void CTaxValidationAndCleanup::x_UpdateSpecificHostMapWithReply(const CTaxon3_reply& reply, string& error_message)
{
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    TSpecificHostRequests::iterator rq_it = m_SpecificHostRequests.begin();
    while (rq_it != m_SpecificHostRequests.end()) {
        while (rq_it->second.NumRemainingReplies() > 0 && reply_it != reply.GetReply().end()) {
            rq_it->second.AddReply(**reply_it);
            ++reply_it;
        }
        if (rq_it->second.NumRemainingReplies() > 0) {
            error_message = "Failed to respond to all taxonomy requests for specific host";
            break;
        }
        ++rq_it;
    }

    if (reply_it != reply.GetReply().end()) {
        error_message = "Unexpected taxonomy responses for specific host";
    }
}


bool CTaxValidationAndCleanup::x_ApplySpecificHostMap(COrg_ref& org_ref) const
{
    if (!org_ref.IsSetOrgname() ||
        !org_ref.GetOrgname().IsSetMod()) {
        return false;
    }

    bool changed = false;

    for (auto m = org_ref.SetOrgname().SetMod().begin(); m != org_ref.SetOrgname().SetMod().end(); m++) {
        if ((*m)->IsSetSubtype() &&
            (*m)->GetSubtype() == COrgMod::eSubtype_nat_host &&
            (*m)->IsSetSubname()) {
            string host_val = x_DefaultSpecificHostAdjustments((*m)->GetSubname());
            TSpecificHostRequests::const_iterator it = m_SpecificHostRequests.find(host_val);
            if (it != m_SpecificHostRequests.end()) {
                const string& new_val = it->second.SuggestFix();
                if (!NStr::IsBlank(new_val) && !NStr::Equal(new_val, (*m)->GetSubname())) {
                    (*m)->SetSubname(new_val);
                    changed = true;
                }
            }
        }
    }

    return changed;
}


string CTaxValidationAndCleanup::x_DefaultSpecificHostAdjustments(const string& host_val)
{
    string adjusted = host_val;
    NStr::TruncateSpacesInPlace(adjusted);
    adjusted = COrgMod::FixHost(adjusted);
    return adjusted;
}


string CTaxValidationAndCleanup::IncrementalStrainMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    return m_StrainMap.IncrementalUpdate(input, reply);
}


bool CTaxValidationAndCleanup::IsStrainMapUpdateComplete() const
{
    return m_StrainMap.IsUpdateComplete();
}


void CTaxValidationAndCleanup::ReportStrainErrors(CValidError_imp& imp)
{
    m_StrainMap.PostErrors(imp);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
