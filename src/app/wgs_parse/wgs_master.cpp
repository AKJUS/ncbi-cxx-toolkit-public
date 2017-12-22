/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <map>
#include <functional>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/seqblock/GB_block.hpp>

#include "wgs_params.hpp"
#include "wgs_master.hpp"
#include "wgs_utils.hpp"
#include "wgs_asn.hpp"
#include "wgs_tax.hpp"
#include "wgs_seqentryinfo.hpp"
#include "wgs_med.hpp"


namespace wgsparse
{

static size_t CheckPubs(const CSeq_entry& entry, const string& file, list<CPubDescriptionInfo>& common_pubs)
{
    const CSeq_descr* descrs = nullptr;
    if (!GetDescr(entry, descrs)) {
        return 0;
    }

    size_t num_of_pubs = 0;

    if (!common_pubs.empty()) {
        // TODO

        return num_of_pubs;
    }

    list<CRef<CPubdesc>> pubs;

    if (descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Get()) {
            if (descr->IsPub()) {
                if (!IsPubdescContainsSub(descr->GetPub())) {
                    ++num_of_pubs;
                }

                CRef<CPubdesc> pubdesc(new CPubdesc);
                pubdesc->Assign(descr->GetPub());
                pubs.push_back(pubdesc);
            }
        }
    }

    if (pubs.empty()) {
        ERR_POST_EX(0, 0, Info << "Submission from file \"" << file << "\" is lacking publications.");
        return num_of_pubs;
    }

    if (common_pubs.empty()) {

        for (auto& pub : pubs) {
            common_pubs.push_front(CPubDescriptionInfo());
            CPubDescriptionInfo& pubdescr_info = common_pubs.front();

            pubdescr_info.m_pubdescr_synonyms.push_back(pub);
            pubdescr_info.m_pubdescr_lookup = pub;

            if (!IsPubdescContainsSub(*pub)) {
                pubdescr_info.m_pmid = SinglePubLookup(pubdescr_info.m_pubdescr_lookup);
            }
        }

        // TODO
    }
    else {
        // TODO
    }

    return num_of_pubs;
}

template <typename Func, typename Container> void CollectDataFromDescr(const CSeq_entry& entry, Container& container, Func process)
{
    if (entry.IsSeq() && !entry.GetSeq().IsNa()) {
        return;
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            for (auto& descr : descrs->Get()) {
                process(*descr, container);
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for_each(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                     [&container, &process](const CRef<CSeq_entry>& cur_entry) { CollectDataFromDescr(*cur_entry, container, process); });
        }
    }
}

static void ProcessComment(const CSeqdesc& descr, set<string>& comments)
{
    if (descr.IsComment() && !descr.GetComment().empty()) {
        comments.insert(descr.GetComment());
    }
}


static void CheckComments(const CSeq_entry& entry, CMasterInfo& info)
{
    if (info.m_common_comments_not_set) {
        CollectDataFromDescr(entry, info.m_common_comments, ProcessComment);
        info.m_common_comments_not_set = info.m_common_comments.empty();
    }
    else {

        if (!info.m_common_comments.empty()) {

            set<string> cur_comments;
            CollectDataFromDescr(entry, cur_comments, ProcessComment);

            set<string> common_comments;
            set_intersection(info.m_common_comments.begin(), info.m_common_comments.end(), cur_comments.begin(), cur_comments.end(), inserter(common_comments, common_comments.begin()));

            info.m_common_comments.swap(common_comments);
        }
    }
}

static void ProcessStructuredComment(const CSeqdesc& descr, set<string>& comments)
{
    if (IsUserObjectOfType(descr, "StructuredComment")) {

        const CUser_object& user_obj = descr.GetUser();
        comments.insert(ToString(user_obj));
    }
}

// TODO may be combined with 'CheckComments'
static void CheckStructuredComments(const CSeq_entry& entry, CMasterInfo& info)
{
    if (info.m_common_structured_comments_not_set) {
        CollectDataFromDescr(entry, info.m_common_structured_comments, ProcessStructuredComment);
        info.m_common_structured_comments_not_set = info.m_common_structured_comments.empty();
    }
    else {

        if (!info.m_common_structured_comments.empty()) {

            set<string> cur_comments;
            CollectDataFromDescr(entry, cur_comments, ProcessStructuredComment);

            set<string> common_comments;
            set_intersection(info.m_common_structured_comments.begin(), info.m_common_structured_comments.end(), cur_comments.begin(), cur_comments.end(), inserter(common_comments, common_comments.begin()));

            info.m_common_structured_comments.swap(common_comments);
        }
    }
}

static bool CheckBioSource(const CSeq_entry& entry, CMasterInfo& info, const string& file)
{
    bool ret = true;

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            auto is_biosource = [](const CRef<CSeqdesc>& descr) { return descr->IsSource(); };
            size_t num_of_biosources = count_if(descrs->Get().begin(), descrs->Get().end(), is_biosource);

            if (num_of_biosources > 1) {
                ERR_POST_EX(0, 0, Fatal << "Multiple BioSource descriptors encountered in record from file \"" << file << "\".");
                ret = false;
            }
            else if (num_of_biosources < 1) {
                ERR_POST_EX(0, 0, Warning << "Submission from file \"" << file << "\" is lacking BioSource.");
            }
            else {

                auto& biosource_it = find_if(descrs->Get().begin(), descrs->Get().end(), is_biosource);
                if (info.m_biosource.Empty()) {
                    info.m_biosource.Reset(new CBioSource);
                    info.m_biosource->Assign((*biosource_it)->GetSource());
                }
                else {
                    // TODO
                }
            }
        }
    }

    return ret;
}

struct CDBLinkInfo
{
    CRef<CUser_object> m_dblink;
    int m_dblink_state;

    string m_cur_bioseq_id;

    CDBLinkInfo() :
        m_dblink_state(eDblinkNoDblink)
    {
    }
};

static void CollectDblink(const CSeq_entry& entry, CDBLinkInfo& info)
{
    if (info.m_dblink_state == eDblinkDifferentDblink) {
        return;
    }

    if (info.m_cur_bioseq_id.empty() && entry.IsSeq()) {
        info.m_cur_bioseq_id = GetSeqIdStr(entry.GetSeq());
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Get()) {
            if (IsUserObjectOfType(*descr, "DBLink")) {

                const CUser_object& user_obj = descr->GetUser();
                if (info.m_dblink.Empty()) {
                    info.m_dblink.Reset(new CUser_object);
                    info.m_dblink->Assign(user_obj);
                    info.m_dblink_state = eDblinkNoProblem;
                }
                else if (!info.m_dblink->Equals(user_obj)) {
                    info.m_dblink_state = eDblinkDifferentDblink;
                    return;
                }
            }
        }
    }

    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for_each(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                     [&info](const CRef<CSeq_entry>& cur_entry) { CollectDblink(*cur_entry, info); });
        }
    }
}

static void CheckDblink(const CSeq_entry& entry, CMasterInfo& info, const string& file)
{
    CDBLinkInfo dblink_info;
    CollectDblink(entry, dblink_info);

    if (dblink_info.m_cur_bioseq_id.empty()) {
        dblink_info.m_cur_bioseq_id = "Unknown";
    }

    if (dblink_info.m_dblink_state & eDblinkNoDblink) {
        info.SetDblinkEmpty(file, dblink_info.m_cur_bioseq_id);
    }
    else if (dblink_info.m_dblink_state & eDblinkDifferentDblink) {
        info.SetDblinkDifferent(file, dblink_info.m_cur_bioseq_id);
    }
    else {
        if (info.m_dblink.Empty()) {
            info.m_dblink = dblink_info.m_dblink;
        }
        else if (!info.m_dblink->Equals(*dblink_info.m_dblink)) {
            info.SetDblinkDifferent(file, dblink_info.m_cur_bioseq_id);
        }
    }
}

static bool HasGenomeProjectID(const CUser_object& user_obj)
{
    return user_obj.IsSetType() && user_obj.GetType().IsStr() && user_obj.GetType().GetStr() == "GenomeProjectsDB";
}

static bool CheckGPID(const CSeq_entry& entry)
{
    const CSeq_descr* descrs = nullptr;
    bool ret = false;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {

            for (auto& descr : descrs->Get()) {
                if (descr->IsUser()) {
                    ret = HasGenomeProjectID(descr->GetUser());
                    if (ret) {
                        break;
                    }
                }
            }
        }
    }

    if (!ret && entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            auto& entry_it = find_if(entry.GetSet().GetSeq_set().begin(), entry.GetSet().GetSeq_set().end(),
                                     [](const CRef<CSeq_entry>& cur_entry) { return CheckGPID(*cur_entry); });
            ret = entry_it != entry.GetSet().GetSeq_set().end();
        }
    }

    return ret;
}

static bool SubmissionDiffers(const string& file, bool same_submit)
{
    if (GetParams().IsDblinkOverride()) {
        ERR_POST_EX(0, 0, "Submission \"" << file << "\" has different Submit block. Using Submit-block from the first submission.");
    }
    else {
        ERR_POST_EX(0, 0, "Submission \"" << file << "\" has different Submit block. Will not provide Cit-sub descriptor in master Bioseq. This can be overridden by setting \"-X T\" command line switch: it'll use Submit-block from the first file.");
        same_submit = false;
    }

    return same_submit;
}

static void SortOrgRef(COrg_ref& org_ref)
{
    if (org_ref.IsSetDb()) {

        COrg_ref::TDb& db_tags = org_ref.SetDb();
        sort(db_tags.begin(), db_tags.end(), [](const CRef<CDbtag>& tag1, const CRef<CDbtag>& tag2)
            {
                if (tag1.Empty() || !tag1->IsSetDb()) {
                    return true;
                }

                if (tag2.Empty() || !tag2->IsSetDb()) {
                    return false;
                }

                return tag1->GetDb() < tag2->GetDb();
            });
    }

    if (org_ref.IsSetMod()) {
        COrg_ref::TMod& mods = org_ref.SetMod();
        mods.sort();
    }
}

static bool CheckSameOrgRefs(list<COrgRefInfo>& org_refs)
{
    if (org_refs.empty()) {
        return true;
    }

    auto& cur_org_ref = org_refs.begin();
    CRef<COrg_ref>& first_org_ref = cur_org_ref->m_org_ref;
    SortOrgRef(*first_org_ref);

    for (++cur_org_ref; cur_org_ref != org_refs.end(); ++cur_org_ref) {
        CRef<COrg_ref>& next_org_ref = cur_org_ref->m_org_ref;
        SortOrgRef(*next_org_ref);

        if (!first_org_ref->Equals(*next_org_ref)) {
            return false;
        }
    }

    return true;
}

static bool DBLinkProblemReport(const CMasterInfo& info)
{
    bool reject = false;
    if (info.m_dblink.NotEmpty() && info.m_dblink_state != eDblinkNoProblem) {
        if (info.m_dblink_state & eDblinkDifferentDblink) {
            ERR_POST_EX(0, 0, Critical << "The files being processed contain DBLink User-objects that are not identical in content. The first difference was encountered at sequence \"" <<
                        info.m_dblink_diff_info.first << "\" of input file \"" << info.m_dblink_diff_info.second << "\".");
            reject = true;
        }
        if (info.m_dblink_state & eDblinkNoDblink) {
            string err_msg = "The files being processed contain some records that lack DBLink User-objects. The first record that lacks a DBLink was encountered at sequence \"" +
                info.m_dblink_empty_info.first + "\" of input file \"" + info.m_dblink_empty_info.second + "\". ";

            if (GetParams().IsDblinkOverride()) {
                ERR_POST_EX(0, 0, Warning << err_msg << "Continue anyway.");
            }
            else {
                ERR_POST_EX(0, 0, Critical << err_msg << "Rejecting the whole project.");
                reject = true;
            }
        }
    }
    return reject;
}

static void CheckMasterDblink(const CMasterInfo& info)
{
    // TODO
}

static string GetAccessionValue(size_t val_len, int val)
{
    CNcbiOstrstream sstream;
    sstream << setfill('0') << setw(2) << GetParams().GetAssemblyVersion() << setw(val_len) << val << '\0';
    return sstream.str();
}

static const size_t LENGTH_NOT_SET = -1;

static CRef<CSeq_id> CreateAccession(int last_accession_num, size_t accession_len)
{
    size_t max_accession_len = GetMaxAccessionLen(last_accession_num);

    if (accession_len == LENGTH_NOT_SET) {
        accession_len = max_accession_len;
    }

    CRef<CSeq_id> ret;
    if (accession_len != max_accession_len) {

        CNcbiStrstream msg;
        msg << "Incorrect format for accessions, given the total number of contigs in the project: \"N+2+" << accession_len << "\" was used, but only \"N+2+" << max_accession_len << "\" is needed.";

        if (GetParams().GetSource() == eNCBI) {
            ERR_POST_EX(0, 0, Critical << msg.str());
            return ret;
        }

        ERR_POST_EX(0, 0, Info << msg.str());
    }

    string id_num(accession_len + 2, '0');

    CRef<CTextseq_id> text_id(new CTextseq_id);
    text_id->SetAccession(GetParams().GetIdPrefix() + id_num);

    id_num = GetAccessionValue(accession_len, 0);
    text_id->SetName(GetParams().GetIdPrefix() + id_num);

    text_id->SetVersion(GetParams().GetAssemblyVersion());

    auto set_fun = FindSetTextSeqIdFunc(GetParams().GetIdChoice());
    _ASSERT(set_fun != nullptr && "There should be a valid SetTextId function. Validate the ID choice.");

    if (set_fun == nullptr) {
        return ret;
    }

    ret.Reset(new CSeq_id);
    (ret->*set_fun)(*text_id);

    return ret;
}

static void SetMolInfo(CBioseq& bioseq)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    CMolInfo& mol_info = descr->SetMolinfo();

    if (GetParams().IsTsa()) {

        bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);
        // TODO
    }
    else {

        bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

        CMolInfo::ETech tech = GetParams().IsTls() ? CMolInfo::eTech_targeted : CMolInfo::eTech_wgs;
        mol_info.SetTech(tech);
        mol_info.SetBiomol(CMolInfo::eBiomol_genomic);
    }

    bioseq.SetDescr().Set().push_back(descr);
}

static CRef<CSeqdesc> CreateCitSub(CCit_sub& cit_sub)
{
    CRef<CPub> pub(new CPub);
    pub->SetSub().Assign(cit_sub);

    CRef<CSeqdesc> descr(new CSeqdesc);

    CPubdesc& pubdescr = descr->SetPub();
    pubdescr.SetPub().Set().push_back(pub);

    if (cit_sub.IsSetImp()) {
        if (!cit_sub.IsSetDate() && cit_sub.GetImp().IsSetDate()) {
            cit_sub.SetDate().Assign(cit_sub.GetImp().GetDate());
        }
        cit_sub.ResetImp();
    }

    return descr;
}

static void AddContactInfo(CCit_sub& cit_sub, const CContact_info& contact_info)
{
    if (cit_sub.IsSetAuthors() && cit_sub.GetAuthors().IsSetAffil()) {
        return;
    }

    // TODO
}

static void CreatePub(CBioseq& bioseq, const CPubdesc& pubdescr)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    descr->SetPub().Assign(pubdescr);
    bioseq.SetDescr().Set().push_back(descr);
}

static bool IsResetGenome()
{
    return GetParams().GetSource() == eNCBI ||
        GetParams().GetUpdateMode() != eUpdateAssembly &&
        GetParams().GetUpdateMode() != eUpdateNew &&
        GetParams().GetUpdateMode() != eUpdateFull;
}

static bool CreateBiosource(CBioseq& bioseq, CBioSource& biosource, const list<COrgRefInfo>& org_refs)
{

    bool is_tax_lookup = GetParams().IsTaxonomyLookup();
    if (!PerformTaxLookup(biosource, org_refs, is_tax_lookup) && is_tax_lookup) {
        ERR_POST_EX(0, 0, Critical << "Taxonomy lookup failed on Master Bioseq. Cannot proceed.");
        return false;
    }

    if (IsResetGenome()) {
        biosource.ResetGenome();
    }

    // TODO

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetSource().Assign(biosource);
    bioseq.SetDescr().Set().push_back(descr);

    return true;
}

static void AddField(CUser_object& user_obj, const string& label, const string& val)
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(label);
    field->SetString(val);

    user_obj.SetData().push_back(field);
}

static void CreateUserObject(const CMasterInfo& info, CBioseq& bioseq)
{
    CRef<CUser_object> user_obj(new CUser_object);

    // TODO update_extra_contigs

    CObject_id& obj_id = user_obj->SetType();

    static const string ACCESSION_FIRST("_accession_first");
    static const string ACCESSION_LAST("_accession_last");

    string accession_first_label,
        accession_last_label,
        accession_first_val,
        accession_last_val;

    int first = 1,
        last = info.m_num_of_entries;

    if (GetParams().IsTsa()) {
        obj_id.SetStr("TSA-RNA-List");
        accession_first_label = "TSA";
        accession_last_label = "TSA";
    }
    else if (GetParams().IsTls()) {
        obj_id.SetStr("TLSProjects");
        accession_first_label = "TLS";
        accession_last_label = "TLS";
    }
    else {
        obj_id.SetStr("WGSProjects");
        accession_first_label = "WGS";
        accession_last_label = "WGS";
    }

    accession_first_label += ACCESSION_FIRST;
    accession_last_label += ACCESSION_LAST;

    size_t max_accession_len = GetMaxAccessionLen(last);
    accession_first_val = GetAccessionValue(max_accession_len, first);
    accession_last_val = GetAccessionValue(max_accession_len, last);

    AddField(*user_obj, accession_first_label, GetParams().GetIdPrefix() + accession_first_val);
    AddField(*user_obj, accession_last_label, GetParams().GetIdPrefix() + accession_last_val);

    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser().Assign(*user_obj);
    bioseq.SetDescr().Set().push_back(descr);
}

static bool CreateDateDescr(CBioseq& bioseq, const CDate& date, EDateIssues issue, bool is_update_date)
{
    if (date.Which() == CDate::e_not_set || issue != eDateNoIssues)
        return false;

    CRef<CSeqdesc> descr(new CSeqdesc);

    if (is_update_date) {
        descr->SetUpdate_date().Assign(date);
    }
    else {
        descr->SetCreate_date().Assign(date);
    }

    bioseq.SetDescr().Set().push_back(descr);

    return true;
}

static void AddComment(CBioseq& bioseq, const string& comment)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetComment(comment);

    bioseq.SetDescr().Set().push_back(descr);
}

static void AddStructuredComment(CBioseq& bioseq, const string& comment)
{
    CRef<CSeqdesc> descr(new CSeqdesc);

    CNcbiIstrstream stream(comment.c_str());
    CRef<CUser_object> user_obj(new CUser_object);
    stream >> MSerial_AsnText >> *user_obj;
    descr->SetUser(*user_obj);

    bioseq.SetDescr().Set().push_back(descr);
}

static void CreateDbLink(CBioseq& bioseq, CUser_object& user_obj)
{
    CRef<CSeqdesc> descr(new CSeqdesc);
    descr->SetUser(user_obj);

    bioseq.SetDescr().Set().push_back(descr);
}

static const string TPA_KEYWORD("TPA:assembly");

static bool FixTpaKeyword(set<string>& keywords)
{
    static const string TPA_KEYWORD_OLD("TPA:reassembly");

    bool ret = false;
    if (GetParams().IsVDBMode()) {

        auto tpa_keyword_it = keywords.find(TPA_KEYWORD);
        if (tpa_keyword_it != keywords.end()) {
            ret = true;
            keywords.erase(tpa_keyword_it);
        }

        tpa_keyword_it = keywords.find(TPA_KEYWORD_OLD);
        if (tpa_keyword_it != keywords.end()) {
            ret = true;
            keywords.erase(tpa_keyword_it);
        }

        if (ret) {
            keywords.insert(GetParams().GetTpaKeyword());
        }
    }

    return ret;
}

static CGB_block* ProcessKeywords(CBioseq& bioseq, const CMasterInfo& info)
{
    CRef<CSeqdesc> descr;
    if (GetParams().IsVDBMode()) {

        descr.Reset(new CSeqdesc);
        for (auto& keyword : info.m_keywords) {
            if (!keyword.empty()) {
                descr->SetGenbank().SetKeywords().push_back(keyword);
            }
        }
    }
    else {

        if (GetParams().IsTsa() && info.m_has_targeted_keyword) {
            descr.Reset(new CSeqdesc);
            descr->SetGenbank().SetKeywords().push_back("Targeted");
        }
        else if (GetParams().IsWgs() && info.m_has_gmi_keyword) {
            descr.Reset(new CSeqdesc);
            descr->SetGenbank().SetKeywords().push_back("GMI");
        }
    }

    if (descr.NotEmpty()) {
        bioseq.SetDescr().Set().push_back(descr);
    }

    return descr.NotEmpty() ? &descr->SetGenbank() : nullptr;
}

static void AddTpaKeyword(CBioseq& bioseq, CGB_block* gb_block)
{
    CRef<CSeqdesc> descr;
    if (gb_block == nullptr) {
        descr.Reset(new CSeqdesc);
        gb_block = &descr->SetGenbank();
        bioseq.SetDescr().Set().push_back(descr);
    }

    gb_block->SetKeywords().push_back(GetParams().GetTpaKeyword().empty() ? TPA_KEYWORD : GetParams().GetTpaKeyword());
}

static CRef<CSeq_entry> CreateMasterBioseq(CMasterInfo& info, CRef<CCit_sub>& cit_sub, CRef<CContact_info>& contact_info)
{
    CRef<CBioseq> bioseq(new CBioseq);
    CRef<CSeq_entry> ret;

    int last_accession_num = info.m_num_of_entries;
    size_t accession_len = LENGTH_NOT_SET;

    CRef<CSeq_id> id = CreateAccession(last_accession_num, accession_len);
    if (id.Empty()) {
        return ret;
    }

    _ASSERT(id->GetTextseq_Id() != nullptr && id->GetTextseq_Id()->IsSetName() && "CreateAccession(...) should create 'TextId' with 'Name' attribute");
    info.m_master_file_name = id->GetTextseq_Id()->GetName();

    bioseq->SetId().push_back(id);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    bioseq->SetInst().SetLength(info.m_num_of_entries);

    SetMolInfo(*bioseq);

    // Keywords
    bool is_tpa_keyword_present = false;
    CGB_block* gb_block = nullptr;

    if (info.m_keywords_set) {
        is_tpa_keyword_present = FixTpaKeyword(info.m_keywords);
        gb_block = ProcessKeywords(*bioseq, info);
    }

    if (GetParams().IsTpa() && !is_tpa_keyword_present) {
        AddTpaKeyword(*bioseq, gb_block);
    }

    // Comments
    if (info.m_common_comments.empty() && info.m_common_structured_comments.empty() && GetParams().GetSource() != eNCBI) {
        ERR_POST_EX(0, 0, Info << "All contigs are missing both text and structured comments.");
    }

    for (auto& comment : info.m_common_comments) {
        AddComment(*bioseq, comment);
    }

    for (auto& structured_comment : info.m_common_structured_comments) {
        AddStructuredComment(*bioseq, structured_comment);
    }

    // CitSub
    if (cit_sub.NotEmpty()) {
        bioseq->SetDescr().Set().push_back(CreateCitSub(*cit_sub));
        if (contact_info.NotEmpty()) {
            AddContactInfo(*cit_sub, *contact_info);
        }
    }

    // TODO

    for (auto& pubdescr : info.m_common_pubs) {
        CreatePub(*bioseq, *pubdescr.m_pubdescr_lookup);
    }

    // TODO

    if (info.m_biosource.NotEmpty()) {
        if (!CreateBiosource(*bioseq, *info.m_biosource, info.m_org_refs)) {
            return ret;
        }
    }

    if (GetParams().GetSource() != eNCBI) {
        info.m_update_date_present = CreateDateDescr(*bioseq, info.m_update_date, info.m_update_date_issues, true);
        info.m_creation_date_present = CreateDateDescr(*bioseq, info.m_creation_date, info.m_creation_date_issues, false);
    }

    if (info.m_num_of_entries) {
        CreateUserObject(info, *bioseq);
    }

    if (info.m_dblink_state == eDblinkNoProblem && info.m_dblink.NotEmpty()) {
        CreateDbLink(*bioseq, *info.m_dblink);
    }

    ret.Reset(new CSeq_entry);
    ret->SetSeq(*bioseq);
    return ret;
}

static bool IsDupIds(const list<string>& ids)
{
    set<string> unique_ids;
    for (auto& id : ids) {
        if (!unique_ids.insert(id).second) {

            ERR_POST_EX(0, 0, Error << "Found duplicated general or local id: \"" << id << "\".");
            return true;
        }
    }

    return false;
}

static bool NeedToGetAccessionPrefix() {

    return GetParams().IsUpdateScaffoldsMode() && GetParams().IsAccessionAssigned() && GetParams().GetScaffoldPrefix().empty();
}

static void ReportDateProblem(EDateIssues issue, string date_type, bool is_error)
{
    if (issue == eDateMissing) {
        ERR_POST_EX(0, 0, (is_error ? Error : Info) <<
                    date_type << " date is missing from one or more input submissions.Will not propagate " <<
                    date_type << " date to the master record.");
    }
    else if (issue == eDateDiff) {
        ERR_POST_EX(0, 0, (is_error ? Error : Info) <<
                    "Different " << date_type << " dates encountered amongst input submissions.Will not propagate " <<
                    date_type << " date to the master record.");
    }
}

static bool IsDateFound(const CSeq_descr::Tdata& descrs, CSeqdesc::E_Choice choice)
{
    auto date = find_if(descrs.begin(), descrs.end(), [choice](const CRef<CSeqdesc>& desc){ return desc->Which() == choice; });
    return date != descrs.end();
}

static bool IsDatePresent(const CSeq_entry& entry, CSeqdesc::E_Choice choice)
{
    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs->IsSet()) {
        if (IsDateFound(descrs->Get(), choice)) {
            return true;
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (IsDatePresent(*cur_entry, choice)) {
                return true;
            }
        }
    }

    return false;
}

static bool CheckCitSubsInBioseqSet(CMasterInfo& master_info)
{
    // TODO
    return true;
}

bool CreateMasterBioseqWithChecks(CMasterInfo& master_info)
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true,
         same_submit = true;

    CRef<CContact_info> master_contact_info;
    CRef<CCit_sub> master_cit_sub;
    CSeqEntryCommonInfo common_info;

    for (auto& file : files) {

        CNcbiIfstream in(file);

        if (!in) {
            ERR_POST_EX(0, 0, "Failed to open submission \"" << file << "\" for reading. Cannot proceed.");
            ret = false;
            break;
        }

        EInputType input_type = eSeqSubmit;
        GetInputTypeFromFile(in, input_type);

        bool first = true;
        while (in && !in.eof()) {

            CRef<CSeq_submit> seq_submit = GetSeqSubmit(in, input_type);
            if (seq_submit.Empty()) {

                if (first) {
                    ERR_POST_EX(0, 0, "Failed to read " << GetSeqSubmitTypeName(input_type) << " from file \"" << file << "\". Cannot proceed.");
                    ret = false;
                }
                break;
            }

            first = false;

            if (!FixSeqSubmit(seq_submit, master_info.m_accession_ver, true)) {
                ERR_POST_EX(0, 0, "Wrapper GenBank set has non-empty annotation (Seq-annot), which is not allowed. Cannot process this submission \"" << file << "\".");
                ret = false;
                break;
            }

            if (GetParams().GetUpdateMode() == eUpdateAssembly && master_info.m_accession_ver > 0 && GetParams().IsAccessionAssigned()) {
                // TODO
            }

            if (!seq_submit->IsSetSub()) {
                if (input_type == eSeqSubmit) {
                    ERR_POST_EX(0, 0, "Submission \"" << file << "\" is missing Submit-block.");
                }
                else if (same_submit) {
                    same_submit = CheckCitSubsInBioseqSet(master_info);
                }
            }
            else if (input_type != eSeqSubmit || GetParams().GetSource() == eNCBI) {

                CSubmit_block& submit_block = seq_submit->SetSub();
                submit_block.ResetTool();

                if (!submit_block.IsSetContact()) {
                    ERR_POST_EX(0, 0, "Submit block from submission \"" << file << "\" is missing contact information.");
                }
                else {

                    submit_block.SetContact().ResetContact();
                    if (master_contact_info.Empty()) {
                        master_contact_info.Reset(new CContact_info);
                        master_contact_info->Assign(submit_block.GetContact());
                    }
                    else if (!master_contact_info->Equals(submit_block.GetContact())) {
                        same_submit = SubmissionDiffers(file, same_submit);
                    }
                }

                if (!submit_block.IsSetCit()) {
                    ERR_POST_EX(0, 0, "Submit block from submission \"" << file << "\" is missing Cit-sub.");
                }
                else {

                    CCit_sub& cit_sub = submit_block.SetCit();
                    if (GetParams().IsSetSubmissionDate()) {
                        cit_sub.SetDate().SetStd().Assign(GetParams().GetSubmissionDate());
                    }

                    if (master_cit_sub.Empty()) {
                        master_cit_sub.Reset(new CCit_sub);
                        master_cit_sub->Assign(cit_sub);
                    }
                    else if (!master_cit_sub->Equals(cit_sub)) {
                        same_submit = SubmissionDiffers(file, same_submit);
                    }
                }

                if (!seq_submit->IsSetData()) {
                    ERR_POST_EX(0, 0, "Failed to read Seq-entry from file \"" << file << "\". Cannot proceed.");
                    break;
                }

                for (auto entry : seq_submit->GetData().GetEntrys()) {

                    if (NeedToGetAccessionPrefix()) {
                        // TODO: should eventually call SetScaffoldPrefix
                    }

                    if (GetParams().GetSource() == eNCBI) {
                        if (!master_info.m_update_date_present) {
                            master_info.m_update_date_present = IsDatePresent(*entry, CSeqdesc::e_Update_date);
                        }
                        if (!master_info.m_creation_date_present) {
                            master_info.m_creation_date_present = IsDatePresent(*entry, CSeqdesc::e_Create_date);
                        }
                    }

                    CSeqEntryInfo info(master_info.m_keywords_set, master_info.m_keywords);
                    if (!CheckSeqEntry(*entry, file, info, common_info)) {
                        master_info.m_reject = true;
                    }
                    else if (GetParams().IsTsa() && GetParams().GetFixTech() == eNoFix && info.m_biomol != CMolInfo::eBiomol_transcribed_RNA) {

                        string rna;
                        switch (info.m_biomol) {
                            case CMolInfo::eBiomol_mRNA:
                                rna = "mRNA";
                                break;
                            case CMolInfo::eBiomol_rRNA:
                                rna = "rRNA";
                                break;
                            default:
                                rna = "ncRNA";
                        }

                        ERR_POST_EX(0, 0, Warning << "Unusual biomol value \"" << rna << "\" has been used for this TSA project, instead of \"transcribed_RNA\".");
                    }

                    master_info.m_has_targeted_keyword = master_info.m_has_targeted_keyword || info.m_has_targeted_keyword;
                    master_info.m_has_gmi_keyword = master_info.m_has_gmi_keyword || info.m_has_gmi_keyword;
                    master_info.m_has_gb_block = master_info.m_has_gb_block || info.m_has_gb_block;

                    if (!GetParams().IsUpdateScaffoldsMode()) {

                        if (!GetParams().IsKeepRefs()) {

                            master_info.m_num_of_pubs = max(CheckPubs(*entry, file, master_info.m_common_pubs), master_info.m_num_of_pubs);
                            CheckComments(*entry, master_info);
                        }

                        CheckStructuredComments(*entry, master_info);
                    }

                    if (!CheckBioSource(*entry, master_info, file)) {
                        master_info.m_reject = true;
                    }

                    if (master_info.m_dblink_state != eDblinkAllProblems) {
                        CheckDblink(*entry, master_info, file);
                    }


                    if (!master_info.m_has_genome_project_id) {
                        master_info.m_has_genome_project_id = CheckGPID(*entry);
                    }

                    CollectOrgRefs(*entry, master_info.m_org_refs);

                    if (GetParams().GetSource() != eNCBI) {
                        if (master_info.m_update_date_issues == eDateNoIssues) {
                            master_info.m_update_date_issues = CheckDates(*entry, CSeqdesc::e_Update_date, master_info.m_update_date);
                            ReportDateProblem(master_info.m_update_date_issues, "Update", true);
                        }

                        if (master_info.m_creation_date_issues == eDateNoIssues) {
                            master_info.m_creation_date_issues = CheckDates(*entry, CSeqdesc::e_Create_date, master_info.m_creation_date);
                            ReportDateProblem(master_info.m_creation_date_issues, "Create", GetParams().GetSource() != eEMBL);
                        }
                    }

                    ++master_info.m_num_of_entries;

                    if (info.m_seqid_type == CSeq_id::e_Other) {
                        // TODO
                    }

                    if (!GetParams().IsAccessionsSortedInFile()) {
                        // TODO
                    }

                    if (GetParams().IsUpdateScaffoldsMode()) {
                        // TODO
                    }

                    master_info.m_object_ids.splice(master_info.m_object_ids.end(), info.m_object_ids);
                }
            }

            if (!ret) {
                break;
            }
        }

        if (!ret) {
            break;
        }
    }

    if (GetParams().IsTaxonomyLookup()) {
        LookupCommonOrgRefs(master_info.m_org_refs);
    }
    else {
        for (auto& org_ref_info : master_info.m_org_refs) {
            org_ref_info.m_org_ref_after_lookup = org_ref_info.m_org_ref;
        }
    }

    master_info.m_same_org = CheckSameOrgRefs(master_info.m_org_refs);

    if (same_submit /*&&TODO*/) {
        //TODO
    }

    master_info.m_reject = master_info.m_reject || DBLinkProblemReport(master_info);

    if (GetParams().IsAccessionAssigned()) {
        // TODO
    }

    if (IsDupIds(master_info.m_object_ids)) {
        master_info.m_reject = true;
    }

    // TODO lens ids

    // TODO some complicated error condition

    // TODO...

    CheckMasterDblink(master_info);

    // TODO ...

    if (GetParams().IsUpdateScaffoldsMode()) {
    }
    else {
        master_info.m_master_bioseq = CreateMasterBioseq(master_info, master_cit_sub, master_contact_info);

        // TODO strip authors
    }

    return ret;
}

}