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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for the validator.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_autodef.hpp"

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <corelib/ncbiapp.hpp>

#include <objtools/edit/autodef_with_tax.hpp>


// for writing out tmp files
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>


#include <common/test_assert.h>  /* This header must go last */


extern const char* sc_TestEntryCollidingLocusTags;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBITEST_INIT_TREE()
{
    if ( !CNcbiApplication::Instance()->GetConfig().HasEntry("NCBI", "Data") ) {
    }
}

static bool s_debugMode = false;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddFlag( "debug_mode",
        "Debugging mode writes errors seen for each test" );
}

NCBITEST_AUTO_INIT()
{
    // initialization function body

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["debug_mode"]) {
        s_debugMode = true;
    }
}


static CRef<CSeq_entry> BuildSequence()
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    entry->SetSeq().SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("good");
    entry->SetSeq().SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);
    entry->SetSeq().SetDescr().Set().push_back(mdesc);
    return entry;
}


static CRef<CSeqdesc> AddSource (CRef<CSeq_entry> entry, string taxname)
{
    CRef<CSeqdesc> odesc(new CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname(taxname);

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(odesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(odesc);
    }
    return odesc;
}


static void AddTitle (CRef<CSeq_entry> entry, string defline)
{
    CRef<CSeqdesc> odesc(new CSeqdesc());
    odesc->SetTitle(defline);

    if (entry->IsSeq()) {
        bool found = false;
        if (entry->SetSeq().IsSetDescr()) {
            NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
                if ((*it)->IsTitle()) {
                    (*it)->SetTitle(defline);
                    found = true;
                }
            }
        }
        if (!found) {
            entry->SetSeq().SetDescr().Set().push_back(odesc);
        }
    } else if (entry->IsSet()) {
        if (entry->GetSet().IsSetClass() && entry->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            AddTitle (entry->SetSet().SetSeq_set().front(), defline);
        } else {
            entry->SetSet().SetDescr().Set().push_back(odesc);
        }
    }
}


size_t HasBoolField(const CUser_object& user, const string& field_name)
{
    size_t num_found = 0;
    ITERATE(CUser_object::TData, it, user.GetData()) {
        if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
            NStr::EqualNocase((*it)->GetLabel().GetStr(), field_name)) {
            num_found++;
            if (!(*it)->IsSetData()) {
                BOOST_CHECK_EQUAL("Data for " + field_name + "should be set", "Data not set");
            } else {
                BOOST_CHECK_EQUAL((*it)->GetData().Which(), CUser_field::TData::e_Bool);
                if ((*it)->GetData().IsBool()) {
                    BOOST_CHECK_EQUAL((*it)->GetData().GetBool(), true);
                }
            }
        }
    }
    return num_found;
}

size_t HasStringField(const CUser_object& user, const string& field_name, const string& value)
{
    size_t num_found = 0;
    ITERATE(CUser_object::TData, it, user.GetData()) {
        if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
            NStr::EqualNocase((*it)->GetLabel().GetStr(), field_name)) {
            num_found++;
            if (!(*it)->IsSetData()) {
                BOOST_CHECK_EQUAL("Data for " + field_name + "should be set", "Data not set");
            } else {
                BOOST_CHECK_EQUAL((*it)->GetData().Which(), CUser_field::TData::e_Str);
                if ((*it)->GetData().IsStr()) {
                    BOOST_CHECK_EQUAL((*it)->GetData().GetStr(), value);
                }
            }
        }
    }
    return num_found;
}

size_t HasIntField(const CUser_object& user, const string& field_name, int value)
{
    size_t num_found = 0;
    ITERATE(CUser_object::TData, it, user.GetData()) {
        if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
            NStr::EqualNocase((*it)->GetLabel().GetStr(), field_name)) {
            num_found++;
            if (!(*it)->IsSetData()) {
                BOOST_CHECK_EQUAL("Data for " + field_name + "should be set", "Data not set");
            } else {
                BOOST_CHECK_EQUAL((*it)->GetData().Which(), CUser_field::TData::e_Int);
                if ((*it)->GetData().IsInt()) {
                    BOOST_CHECK_EQUAL((*it)->GetData().GetInt(), value);
                }
            }
        }
    }
    return num_found;
}


void CheckAutoDefOptions(const CUser_object& user, CAutoDefOptions& opts)
{
    size_t expected_num_fields = 7;
    if (opts.GetOrgMods().size() > 0 || opts.GetSubSources().size() > 0) {
        expected_num_fields++;
    }
    if (!opts.GetDoNotApplyToSp()) {
        expected_num_fields--;
    }
    if (opts.GetUseLabels()) {
        expected_num_fields++;
    }
    if (opts.GetAllowModAtEndOfTaxname()) {
        expected_num_fields++;
    }
    if (opts.GetUseFakePromoters()) {
        expected_num_fields ++;
    }
    if (opts.GetKeepRegulatoryFeatures()) {
        expected_num_fields++;
    }
    if (opts.GetKeepIntrons()) {
        expected_num_fields++;
    }
    if (opts.GetKeepExons()) {
        expected_num_fields++;
    }
    if (opts.GetKeepuORFs()) {
        expected_num_fields++;
    }
    if (opts.GetKeepMobileElements()) {
        expected_num_fields++;
    }
    if (opts.AreAnyFeaturesSuppressed()) {
        expected_num_fields++;
    }
    if (opts.GetKeepMiscRecomb()) {
        expected_num_fields++;
    }
    if (opts.GetKeep5UTRs()) {
        expected_num_fields++;
    }
    if (opts.GetKeep3UTRs()) {
        expected_num_fields++;
    }
    if (opts.GetKeepRepeatRegion()) {
        expected_num_fields++;
    }
    if (!NStr::IsBlank(opts.GetCustomFeatureClause())) {
        expected_num_fields++;
    }

    BOOST_CHECK_EQUAL(user.GetObjectType(), CUser_object::eObjectType_AutodefOptions);
    BOOST_CHECK_EQUAL(user.GetData().size(), expected_num_fields);
    BOOST_CHECK_EQUAL(HasBoolField(user, "LeaveParenthetical"), 1);
    BOOST_CHECK_EQUAL(HasBoolField(user, "SpecifyNuclearProduct"), 1);
    if (opts.GetUseLabels()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "UseLabels"), 1);
    }
    if (opts.GetAllowModAtEndOfTaxname()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "AllowModAtEndOfTaxname"), 1);
    }
    if (opts.GetDoNotApplyToSp()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "DoNotApplyToSp"), 1);
    }
    if (opts.GetUseFakePromoters()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "UseFakePromoters"), 1);
    }
    if (opts.GetKeepIntrons()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "KeepIntrons"), 1);
    }
    if (opts.GetKeepExons()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "KeepExons"), 1);
    }
    if (opts.GetKeepuORFs()) {
        BOOST_CHECK_EQUAL(HasBoolField(user, "KeepuORFs"), 1);
    }
    BOOST_CHECK_EQUAL(HasStringField(user, "MiscFeatRule", opts.GetMiscFeatRule(opts.GetMiscFeatRule())) , 1);
    BOOST_CHECK_EQUAL(HasStringField(user, "FeatureListType", opts.GetFeatureListType(opts.GetFeatureListType())), 1);
    BOOST_CHECK_EQUAL(HasStringField(user, "HIVRule", "WantBoth"), 1);
    if (!NStr::IsBlank(opts.GetCustomFeatureClause())) {
        BOOST_CHECK_EQUAL(HasStringField(user, "CustomFeatureClause", opts.GetCustomFeatureClause()), 1);
    }
    BOOST_CHECK_EQUAL(HasIntField(user, "MaxMods", -99), 1);
    if (user.GetData().size() != expected_num_fields) {
        int field_num = 1;
        ITERATE(CUser_object::TData, it, user.GetData()) {
            if (!(*it)->IsSetLabel() || !(*it)->GetLabel().IsStr()) {
                BOOST_CHECK_EQUAL("Label should be set", "label not set for " + NStr::IntToString(field_num));
            } else {
                printf("%s\n", (*it)->GetLabel().GetStr().c_str());
            }
        }
    }
}

static void CheckDeflineMatches(CSeq_entry_Handle seh,
                                CAutoDefWithTaxonomy& autodef,
                                CRef<CAutoDefModifierCombo> mod_combo)
{
    // check defline for each nucleotide sequence
    CBioseq_CI seq_iter(seh, CSeq_inst::eMol_na);
    for ( ; seq_iter; ++seq_iter ) {
       CBioseq_Handle bh (*seq_iter);
       //Display ID of sequence
       CConstRef<CSeq_id> id = bh.GetSeqId();

       // original defline
       string orig_defline = "";
       CSeqdesc_CI desc_it(bh, CSeqdesc::e_Title, 1);
       if (desc_it) {
           orig_defline = desc_it->GetTitle();
       }

       string new_defline = autodef.GetOneDefLine(mod_combo, bh);

       BOOST_CHECK_EQUAL(orig_defline, new_defline);

       CRef<CUser_object> tmp_user = autodef.GetOptionsObject();
       CAutoDefOptions opts;
       opts.InitFromUserObject(*tmp_user);
       mod_combo->InitOptions(opts);
       CRef<CUser_object> user = opts.MakeUserObject();
       CAutoDef autodef2;
       autodef2.SetOptionsObject(*user);
       new_defline = autodef2.GetOneDefLine(bh);
       BOOST_CHECK_EQUAL(orig_defline, new_defline);
       CheckAutoDefOptions(*user, opts);
    }

    for (CBioseq_CI seq_it(seh, CSeq_inst::eMol_na); seq_it; ++seq_it) {
        
        CAutoDef autodefA;
        autodefA.SetOptions(*mod_combo);
        CRef<CUser_object> user_optsA = autodefA.GetOptionsObject();
        
        CAutoDef autodefB;
        autodefB.GetOneDefLine(mod_combo, *seq_it);
        CRef<CUser_object> user_optsB = autodefB.GetOptionsObject();
        BOOST_CHECK(user_optsA->Equals(*user_optsB));
    }

    // check popset title if needed

    if (seh.IsSet() && seh.GetSet().GetCompleteBioseq_set()->NeedsDocsumTitle()) {
        string orig_defline = "";
        CSeqdesc_CI desc_it(seh, CSeqdesc::e_Title, 1);
        if (desc_it) {
            orig_defline = desc_it->GetTitle();
        }
        string new_defline = autodef.GetDocsumDefLine(seh);
        BOOST_CHECK_EQUAL(orig_defline, new_defline);
    }
}

static void CheckDeflineMatches(CRef<CSeq_entry> entry,
    vector<CSubSource::ESubtype> subsrcs,
    vector<COrgMod::ESubtype> orgmods,
    bool init_with_descrs = false)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);

    CAutoDefWithTaxonomy autodef;

    if (init_with_descrs) {
        CAutoDef::TSources sources;
        for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter; ++b_iter) {
            for (CSeqdesc_CI desc_it(*b_iter, CSeqdesc::e_Source); desc_it; ++desc_it) {
                sources.emplace_back(&desc_it->GetSource());
            }
        }
        autodef.AddDescriptors(sources);
    }
    else {
        // add to autodef
        autodef.AddSources(seh);
    }

    CRef<CAutoDefModifierCombo> mod_combo;
    mod_combo = new CAutoDefModifierCombo ();
    mod_combo->SetUseModifierLabels(true);
    ITERATE(vector<CSubSource::ESubtype>, it, subsrcs) {
        mod_combo->AddSubsource(*it, true);
    }
    ITERATE(vector<COrgMod::ESubtype>, it, orgmods) {
        mod_combo->AddOrgMod(*it, true);
    }

    autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
    autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);

    CheckDeflineMatches(seh, autodef, mod_combo);
}

static void CheckDeflineMatches(CRef<CSeq_entry> entry,
    bool use_best = false,
    CAutoDefOptions::EFeatureListType list_type = CAutoDefOptions::eListAllFeatures,
    CAutoDefOptions::EMiscFeatRule misc_feat_rule = CAutoDefOptions::eNoncodingProductFeat,
    bool init_with_descrs = false)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);

    CAutoDefWithTaxonomy autodef;

    if (init_with_descrs) {
        CAutoDef::TSources sources;
        for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter; ++b_iter) {
            for (CSeqdesc_CI desc_it(*b_iter, CSeqdesc::e_Source); desc_it; ++desc_it) {
                sources.emplace_back(&desc_it->GetSource());
            }
        }
        autodef.AddDescriptors(sources);
    }
    else {
        // add to autodef
        autodef.AddSources(seh);
    }

    CRef<CAutoDefModifierCombo> mod_combo;
    if (use_best) {
        mod_combo = autodef.FindBestModifierCombo();
    } else {
        mod_combo = new CAutoDefModifierCombo ();
    }

    autodef.SetFeatureListType(list_type);
    autodef.SetMiscFeatRule(misc_feat_rule);

    CheckDeflineMatches(seh, autodef, mod_combo);
}

static void CheckDeflineMatchesWithDescr(CRef<CSeq_entry> entry,
    bool use_best = false,
    CAutoDefOptions::EFeatureListType list_type = CAutoDefOptions::eListAllFeatures,
    CAutoDefOptions::EMiscFeatRule misc_feat_rule = CAutoDefOptions::eNoncodingProductFeat)
{
    bool init_with_descrs = true;
    CheckDeflineMatches(entry, use_best, list_type, misc_feat_rule, init_with_descrs);
}

static CAutoDef::TSources s_GatherSources(const CSeq_entry& entry)
{
    auto objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);

    CAutoDef::TSources sources;
    for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter; ++b_iter) {
        for (CSeqdesc_CI desc_it(*b_iter, CSeqdesc::e_Source); desc_it; ++desc_it) {
            sources.emplace_back(&desc_it->GetSource());
        }
    }
    return sources;
}


static void CheckDeflineMatches(CRef<CSeq_entry> entry, CSeqFeatData::ESubtype feat_to_suppress, bool init_with_descrs = false)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CAutoDefWithTaxonomy autodef;

    if (init_with_descrs) {
        CAutoDef::TSources sources;
        for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter; ++b_iter) {
            for (CSeqdesc_CI desc_it(*b_iter, CSeqdesc::e_Source); desc_it; ++desc_it) {
                sources.emplace_back(&desc_it->GetSource());
            }
        }
        autodef.AddDescriptors(sources);
    }
    else {
        // add to autodef
        autodef.AddSources(seh);
    }

    CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();
    autodef.SuppressFeature(feat_to_suppress);
    autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
    autodef.SetMiscFeatRule(CAutoDefOptions::eNoncodingProductFeat);

    CheckDeflineMatches(seh, autodef, mod_combo);
}


CRef<CSeq_entry> FindNucInSeqEntry(CRef<CSeq_entry> entry)
{
    CRef<CSeq_entry> empty(NULL);
    if (!entry) {
        return empty;
    } else if (entry->IsSeq() && entry->GetSeq().IsNa()) {
        return entry;
    } else if (entry->IsSet()) {
        ITERATE(CBioseq_set::TSeq_set, it, entry->GetSet().GetSeq_set()) {
            CRef<CSeq_entry> rval = FindNucInSeqEntry(*it);
            if (rval) {
                return rval;
            }
        }
    }
    return empty;
}


static void AddFeat (CRef<CSeq_feat> feat, CRef<CSeq_entry> entry)
{
    CRef<CSeq_annot> annot;

    if (entry->IsSeq()) {
        if (!entry->GetSeq().IsSetAnnot()
            || !entry->GetSeq().GetAnnot().front()->IsFtable()) {
            CRef<CSeq_annot> new_annot(new CSeq_annot());
            entry->SetSeq().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSeq().SetAnnot().front();
        }
    } else if (entry->IsSet()) {
        if (!entry->GetSet().IsSetAnnot()
            || !entry->GetSet().GetAnnot().front()->IsFtable()) {
            CRef<CSeq_annot> new_annot(new CSeq_annot());
            entry->SetSet().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSet().SetAnnot().front();
        }
    }

    if (!feat->IsSetLocation() || feat->GetLocation().Which() == CSeq_loc::e_not_set) {
        CRef<CSeq_entry> nuc_entry = FindNucInSeqEntry(entry);
        if (nuc_entry) {
            CRef<CSeq_id> id(new CSeq_id());
            id->Assign(*(nuc_entry->GetSeq().GetId().front()));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(0);
            feat->SetLocation().SetInt().SetTo(entry->GetSeq().GetLength() - 1);
        }
    }

    annot->SetData().SetFtable().push_back(feat);
}


static CRef<CSeq_entry> MakeProteinForNucProtSet (string id, string protein_name)
{
    // make protein
    CRef<CBioseq> pseq(new CBioseq());
    pseq->SetInst().SetMol(CSeq_inst::eMol_aa);
    pseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    pseq->SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIN");
    pseq->SetInst().SetLength(8);

    CRef<CSeq_id> pid(new CSeq_id());
    pid->SetLocal().SetStr (id);
    pseq->SetId().push_back(pid);

    CRef<CSeqdesc> mpdesc(new CSeqdesc());
    mpdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    pseq->SetDescr().Set().push_back(mpdesc);

    CRef<CSeq_entry> pentry(new CSeq_entry());
    pentry->SetSeq(*pseq);

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetProt().SetName().push_back(protein_name);
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr(id);
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(7);
    AddFeat (feat, pentry);

    return pentry;
}


static CRef<CSeq_feat> MakeCDSForNucProtSet (string nuc_id, string prot_id)
{
    CRef<CSeq_feat> cds (new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr(prot_id);
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr(nuc_id);
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(26);
    return cds;
}


static CRef<CSeq_feat> MakeGeneForNucProtSet(const string& nuc_id, const string& locus, const string& allele = kEmptyStr)
{
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus(locus);
    if (!allele.empty()) {
        gene->SetData().SetGene().SetAllele(allele);
    }
    gene->SetLocation().SetInt().SetId().SetLocal().SetStr(nuc_id);
    gene->SetLocation().SetInt().SetFrom(0);
    gene->SetLocation().SetInt().SetTo(26);
    return gene;
}


static CRef<CSeq_entry> BuildNucProtSet(const string& protein_name, const string& locus = kEmptyStr, const string& allele = kEmptyStr)
{
    CRef<CBioseq_set> set(new CBioseq_set());
    set->SetClass(CBioseq_set::eClass_nuc_prot);

    // make nucleotide
    CRef<CBioseq> nseq(new CBioseq());
    nseq->SetInst().SetMol(CSeq_inst::eMol_dna);
    nseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    nseq->SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    nseq->SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("nuc");
    nseq->SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);
    nseq->SetDescr().Set().push_back(mdesc);

    CRef<CSeq_entry> nentry(new CSeq_entry());
    nentry->SetSeq(*nseq);

    if (!locus.empty()) {
        CRef<CSeq_feat> gene = MakeGeneForNucProtSet("nuc", locus, allele);
        AddFeat(gene, nentry);
    }

    set->SetSeq_set().push_back(nentry);

    // make protein
    CRef<CSeq_entry> pentry = MakeProteinForNucProtSet("prot", protein_name);

    set->SetSeq_set().push_back(pentry);

    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet(*set);

    CRef<CSeq_feat> cds = MakeCDSForNucProtSet("nuc", "prot");
    AddFeat (cds, set_entry);

    return set_entry;
}


// tests


BOOST_AUTO_TEST_CASE(Test_SimpleAutodef)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildSequence();
    AddSource (entry, "Homo sapiens");
    AddTitle(entry, "Homo sapiens sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}
    
BOOST_AUTO_TEST_CASE(Test_UnnamedPlasmid)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource (entry, "Alcanivorax sp. HA03");
    desc->SetSource().SetGenome(CBioSource::eGenome_plasmid);
    CRef<CSubSource> sub(new CSubSource("plasmid-name", "unnamed"));
    desc->SetSource().SetSubtype().push_back(sub);
    AddTitle(entry, "Alcanivorax sp. HA03 plasmid sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_476)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("chlorocatechol 1,2-dioxygenase");
    CRef<CSeqdesc> desc = AddSource (entry, "Alcanivorax sp. HA03");
    desc->SetSource().SetGenome(CBioSource::eGenome_plasmid);
    CRef<CSubSource> sub(new CSubSource("plasmid-name", "unnamed"));
    desc->SetSource().SetSubtype().push_back(sub);
    AddTitle(entry, "Alcanivorax sp. HA03 plasmid chlorocatechol 1,2-dioxygenase gene, complete cds.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_630)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource (entry, "Clathrina aurea");
    CRef<CSubSource> sub(new CSubSource("clone", "Cau_E6"));
    desc->SetSource().SetSubtype().push_back(sub);
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> qual(new CGb_qual("satellite", "microsatellite"));
    feat->SetQual().push_back(qual);
    AddFeat(feat, entry);

    AddTitle(entry, "Clathrina aurea microsatellite sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    feat->SetComment("dinucleotide");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_169)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource (entry, "Clathrina aurea");
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetImp().SetKey("misc_feature");
    feat->SetComment("contains 5S ribosomal RNA and nontranscribed spacer");
    AddFeat(feat, entry);

    AddTitle(entry, "Clathrina aurea 5S ribosomal RNA gene region.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_374)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource (entry, "Clathrina aurea");
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetImp().SetKey("misc_feature");
    feat->SetComment("contains DNA lyase (Apn2) gene, Apn2-Mat1 intergenic spacer, and mating type protein (Mat1) gene");
    AddFeat(feat, entry);
    feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    feat->SetLocation().SetPartialStop(true, eExtreme_Biological);

    AddTitle(entry, "Clathrina aurea DNA lyase (Apn2) gene, partial sequence; Apn2-Mat1 intergenic spacer, complete sequence; and mating type protein (Mat1) gene, partial sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_155)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource (entry, "Clathrina aurea");
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetImp().SetKey("misc_feature");
    feat->SetComment("amplified with primers designed for 16S ribosomal RNA");
    AddFeat(feat, entry);

    AddTitle(entry, "Clathrina aurea sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_DocsumTitle_Popset)
{
    CRef<CSeq_entry> seq1 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetTaxname(seq1, "Pinus cembra");
    // clear previous taxid before setting new one
    unit_test_util::SetTaxon(seq1, 0);
    unit_test_util::SetTaxon(seq1, 58041);
    unit_test_util::SetOrgMod(seq1, COrgMod::eSubtype_isolate, "AcesapD07");
    string defline = "Pinus cembra AcesapD07 fake protein name gene, complete cds.";
    AddTitle(unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq1), defline);

    CRef<CSeq_entry> seq2 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::ChangeId(seq2, "2");
    unit_test_util::SetTaxname(seq2, "Pinus cembra");
    // clear previous taxid before setting new one
    unit_test_util::SetTaxon(seq2, 0);
    unit_test_util::SetTaxon(seq2, 58041);
    unit_test_util::SetOrgMod(seq2, COrgMod::eSubtype_isolate, "AcesapD12");
    defline = "Pinus cembra AcesapD12 fake protein name gene, complete cds.";
    AddTitle(unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq2), defline);

    CRef<CSeq_entry> seq3 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::ChangeId(seq3, "3");
    unit_test_util::SetTaxname(seq3, "Pinus cembra");
    // clear previous taxid before setting new one
    unit_test_util::SetTaxon(seq3, 0);
    unit_test_util::SetTaxon(seq3, 58041);
    unit_test_util::SetOrgMod(seq3, COrgMod::eSubtype_isolate, "AcesapD33");
    defline = "Pinus cembra AcesapD33 fake protein name gene, complete cds.";
    AddTitle(unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq3), defline);


    CRef<CSeq_entry> set(new CSeq_entry());
    set->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    set->SetSet().SetSeq_set().push_back(seq1);
    set->SetSet().SetSeq_set().push_back(seq2);
    set->SetSet().SetSeq_set().push_back(seq3);
    defline = "Pinus cembra fake protein name gene, complete cds.";
    AddTitle(set, defline);
    CheckDeflineMatches(set, true);
    CheckDeflineMatchesWithDescr(set, true);
}

BOOST_AUTO_TEST_CASE(Test_DocsumTitle_Physet)
{
    CRef<CSeq_entry> seq1 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetTaxname(seq1, "Bembidion mendocinum");
    // clear previous taxid before setting new one
    unit_test_util::SetTaxon(seq1, 0);
    unit_test_util::SetTaxon(seq1, 1353850);
    string defline = "Bembidion mendocinum fake protein name gene, complete cds.";
    AddTitle(unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq1), defline);

    CRef<CSeq_entry> seq2 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::ChangeId(seq2, "2");
    unit_test_util::SetTaxname(seq2, "Bembidion orregoi");
    // clear previous taxid before setting new one
    unit_test_util::SetTaxon(seq2, 0);
    unit_test_util::SetTaxon(seq2, 1353851);
    defline = "Bembidion orregoi fake protein name gene, complete cds.";
    AddTitle(unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq2), defline);

    CRef<CSeq_entry> set(new CSeq_entry());
    set->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    set->SetSet().SetSeq_set().push_back(seq1);
    set->SetSet().SetSeq_set().push_back(seq2);
    defline = "Chilioperyphus fake protein name gene, complete cds.";
    AddTitle(set, defline);
    CheckDeflineMatches(set, true);
    CheckDeflineMatchesWithDescr(set, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3108)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource(entry, "Fusarium incarnatum");
    CRef<CSeq_feat> feat1(new CSeq_feat());
    feat1->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    feat1->SetData().SetRna().SetExt().SetName("5.8S ribosomal RNA");
    AddFeat(feat1, entry);
    feat1->SetLocation().SetInt().SetTo(19);
    feat1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    CRef<CSeq_feat> feat2(new CSeq_feat());
    feat2->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    feat2->SetData().SetRna().SetExt().SetName("internal transcribed spacer 2");
    AddFeat(feat2, entry);
    feat2->SetLocation().SetInt().SetFrom(20);
    feat2->SetLocation().SetInt().SetTo(39);

    CRef<CSeq_feat> feat3(new CSeq_feat());
    feat3->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    feat3->SetData().SetRna().SetExt().SetName("28S ribosomal RNA");
    AddFeat(feat3, entry);
    feat3->SetLocation().SetInt().SetFrom(40);
    feat3->SetLocation().SetInt().SetTo(59);
    feat3->SetLocation().SetPartialStop(true, eExtreme_Biological);

    AddTitle(entry, "Fusarium incarnatum 5.8S ribosomal RNA gene, partial sequence; internal transcribed spacer 2, complete sequence; and 28S ribosomal RNA gene, partial sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    feat2->SetData().SetRna().SetType(CRNA_ref::eType_other);
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}

BOOST_AUTO_TEST_CASE(Test_GB_3099)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetTaxname(seq, "Influenza A virus (A/USA/RVD1_H1/2011(H1N1))");
    string defline = "Influenza A virus (A/USA/RVD1_H1/2011(H1N1)) hemagglutinin (HA) gene, complete cds.";
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq);
    AddTitle(nuc, defline);
    unit_test_util::SetNucProtSetProductName(seq, "hemagglutinin");
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus("HA");
    AddFeat(gene, nuc);

    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3359)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(seq, "Erwinia amylovora");
    seq->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(seq, CMolInfo::eBiomol_transcribed_RNA);
    CRef<CSeq_feat> ncrna = unit_test_util::BuildGoodFeat ();
    ncrna->SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
    ncrna->SetData().SetRna().SetExt().SetGen().SetProduct("RmaA");
    ncrna->SetData().SetRna().SetExt().SetGen().SetClass("antisense_RNA");
    unit_test_util::AddFeat (ncrna, seq);
    string defline = "Erwinia amylovora RmaA antisense RNA, complete sequence.";
    AddTitle(seq, defline);
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


void TestOneOrganelleSequenceDefline(CBioSource::TGenome genome, const string& defline)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome(seq, genome);
    AddTitle(seq, defline);
    CheckDeflineMatches(seq, true, CAutoDefOptions::eSequence);
    CheckDeflineMatchesWithDescr(seq, true, CAutoDefOptions::eSequence);
}


BOOST_AUTO_TEST_CASE(Test_SQD_1733)
{
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_unknown, "Sebaea microphylla genomic sequence.");
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_mitochondrion, "Sebaea microphylla mitochondrion sequence.");
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_apicoplast, "Sebaea microphylla apicoplast sequence.");
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_chloroplast, "Sebaea microphylla chloroplast sequence.");
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_kinetoplast, "Sebaea microphylla kinetoplast sequence.");
    TestOneOrganelleSequenceDefline(CBioSource::eGenome_leucoplast, "Sebaea microphylla leucoplast sequence.");

}

void AddExon(CRef<CSeq_entry> seq, const string& number, TSeqPos start)
{
    CRef<CSeq_feat> exon = unit_test_util::AddGoodImpFeat(seq, "exon");
    exon->ResetComment();
    exon->SetLocation().SetInt().SetFrom(start);
    exon->SetLocation().SetInt().SetTo(start + 5);
    if (!NStr::IsBlank(number)) {
        CRef<CGb_qual> qual(new CGb_qual());
        qual->SetQual("number");
        qual->SetVal(number);
        exon->SetQual().push_back(qual);
    }
}


BOOST_AUTO_TEST_CASE(Test_GB_3386)
{
    CRef<CSeq_entry> nps = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(nps);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (nps);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddExon(nuc, "1", cds->GetLocation().GetStart(eExtreme_Positional));

    string defline = "Sebaea microphylla fake protein name gene, exon 1 and partial cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

    AddExon(nuc, "2", cds->GetLocation().GetStart(eExtreme_Positional) + 10);
    defline = "Sebaea microphylla fake protein name gene, exons 1 and 2 and partial cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

    AddExon(nuc, "3", cds->GetLocation().GetStart(eExtreme_Positional) +20);
    defline = "Sebaea microphylla fake protein name gene, exons 1 through 3 and partial cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3410)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(seq);
    misc->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    misc->SetComment("contains internal transcribed spacer 1, 5.8S ribosomal RNA, and internal transcribed spacer 2");
    AddTitle(seq, "Sebaea microphylla internal transcribed spacer 1, 5.8S ribosomal RNA gene, and internal transcribed spacer 2, complete sequence.");

    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddTitle(seq, "Sebaea microphylla internal transcribed spacer 1, partial sequence; 5.8S ribosomal RNA gene, complete sequence; and internal transcribed spacer 2, partial sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

    misc->SetComment("contains 18S ribosomal RNA, internal transcribed spacer 1, 5.8S ribosomal RNA, and internal transcribed spacer 2");
    AddTitle(seq, "Sebaea microphylla 18S ribosomal RNA gene, partial sequence; internal transcribed spacer 1 and 5.8S ribosomal RNA gene, complete sequence; and internal transcribed spacer 2, partial sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3395)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> dloop = unit_test_util::AddGoodImpFeat (seq, "D-loop");
    dloop->ResetComment();
    AddTitle(seq, "Sebaea microphylla D-loop, complete sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3439)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(seq, "uncultured archaeon");
    CRef<CSeq_feat> dloop = unit_test_util::AddGoodImpFeat (seq, "D-loop");
    dloop->ResetComment();
    AddTitle(seq, "Uncultured archaeon D-loop, complete sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3488)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(seq, "Cypripedium japonicum");
    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(seq);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna->ResetComment();
    AddTitle(seq, "Cypripedium japonicum gene, complete sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}

BOOST_AUTO_TEST_CASE(Test_GB_3486)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(seq, "Burkholderia sp. FERM BP-3421");
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature (seq);
    gene->ResetComment();
    gene->SetData().SetGene().SetLocus("fr9A");
    AddTitle(seq, "Burkholderia sp. FERM BP-3421 fr9A gene, complete sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

    CRef<CSeq_feat> gene_cluster = unit_test_util::AddMiscFeature(seq);
    gene_cluster->SetComment("spliceostatin/FR901464 biosynthetic gene cluster");

    AddTitle(seq, "Burkholderia sp. FERM BP-3421 spliceostatin/FR901464 biosynthetic gene cluster, complete sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

}


BOOST_AUTO_TEST_CASE(Test_GB_3496)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (cds);
    gene->SetData().SetGene().SetLocus("matK");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    unit_test_util::AddFeat(gene, nuc);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet (entry);
    prot->SetData().SetProt().SetName().front() = "maturase K";

    CRef<CSeq_feat> intron = unit_test_util::AddGoodImpFeat (nuc, "intron");
    intron->SetLocation().SetInt().SetTo(nuc->GetSeq().GetLength() - 1);
    intron->SetLocation().SetPartialStart(true, eExtreme_Biological);
    intron->SetLocation().SetPartialStop(true, eExtreme_Biological);
    intron->SetPartial(true);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature (intron);
    gene2->SetData().SetGene().SetLocus("trnK");
    gene2->SetData().SetGene().SetDesc("tRNA-Lys");
    unit_test_util::AddFeat(gene2, nuc);

    AddTitle(nuc, "Sebaea microphylla tRNA-Lys (trnK) gene, partial sequence; and maturase K (matK) gene, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3458)
{
    // if second coding region does not have protein name, should still not be considered alternatively spliced
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature (cds1);
    gene1->SetData().SetGene().SetLocus("M1");
    unit_test_util::AddFeat(gene1, nuc);
    CRef<CSeq_feat> cds2 = unit_test_util::AddMiscFeature(nuc);
    cds2->SetData().SetCdregion();
    cds2->ResetComment();
    cds2->SetLocation().SetInt().SetFrom(cds1->GetLocation().GetStart(eExtreme_Positional));
    cds2->SetLocation().SetInt().SetTo(nuc->GetSeq().GetInst().GetLength() - 1);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature (cds2);
    gene2->SetData().SetGene().SetLocus("M2");
    unit_test_util::AddFeat(gene2, nuc);
    // make protein for second coding region, with no protein feature
    CRef<CSeq_entry> pentry(new CSeq_entry());
    pentry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    pentry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    pentry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIN");
    pentry->SetSeq().SetInst().SetLength(8);

    CRef<CSeq_id> pid(new CSeq_id());
    pid->SetLocal().SetStr ("prot2");
    pentry->SetSeq().SetId().push_back(pid);
    entry->SetSet().SetSeq_set().push_back(pentry);
    cds2->SetProduct().SetWhole().SetLocal().SetStr("prot2");

    AddTitle(nuc, "Sebaea microphylla M2 and fake protein name (M1) genes, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3679)
{
    // if second coding region does not have protein name, should still not be considered alternatively spliced
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature (cds1);
    gene1->SetData().SetGene().SetLocus("M1");
    unit_test_util::AddFeat(gene1, nuc);

    CRef<CSeq_feat> integron = unit_test_util::AddMiscFeature(nuc);
    integron->SetData().SetImp().SetKey("mobile_element");
    CRef<CGb_qual> q(new CGb_qual());
    q->SetQual("mobile_element_type");
    q->SetVal("integron:class I");
    integron->SetQual().push_back(q);
    integron->SetLocation().SetInt().SetFrom(0);
    integron->SetLocation().SetInt().SetTo(nuc->GetSeq().GetLength() - 1);

    AddTitle(nuc, "Sebaea microphylla class I integron fake protein name (M1) gene, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3848)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature (cds1);
    gene1->SetData().SetGene().SetLocus("gltB");
    gene1->SetData().SetGene().SetAllele("16");
    unit_test_util::AddFeat(gene1, nuc);

    AddTitle(nuc, "Sebaea microphylla fake protein name (gltB) gene, gltB-16 allele, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2075)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(seq);
    misc->SetComment("contains tRNA-Pro and control region");
    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddTitle(seq, "Sebaea microphylla tRNA-Pro gene and control region, partial sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2115)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> promoter = unit_test_util::AddMiscFeature(seq);
    promoter->ResetComment();
    promoter->SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> q(new CGb_qual());
    q->SetQual("regulatory_class");
    q->SetVal("promoter");
    promoter->SetQual().push_back(q);
    AddTitle(seq, "Sebaea microphylla promoter region.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (promoter);
    gene->SetData().SetGene().SetLocus("chs");
    unit_test_util::AddFeat(gene, seq);

    AddTitle(seq, "Sebaea microphylla chs gene, promoter region.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

}


BOOST_AUTO_TEST_CASE(Test_GB_3866)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(seq);
    misc1->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    misc1->SetComment("contains 18S ribosomal RNA and internal transcribed spacer 1");
    misc1->SetLocation().SetInt().SetFrom(0);
    misc1->SetLocation().SetInt().SetTo(15);
    misc1->SetLocation().SetPartialStart(true, eExtreme_Biological);

    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(seq);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna->SetData().SetRna().SetExt().SetName("5.8S ribosomal RNA");
    rna->SetLocation().SetInt().SetFrom(16);
    rna->SetLocation().SetInt().SetTo(19);

    CRef<CSeq_feat> misc2 = unit_test_util::AddMiscFeature(seq);
    misc2->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    misc2->SetComment("contains internal transcribed spacer 2 and 28S ribosomal RNA");
    misc2->SetLocation().SetInt().SetFrom(20);
    misc2->SetLocation().SetInt().SetTo(35);
    misc2->SetLocation().SetPartialStop(true, eExtreme_Biological);

    AddTitle(seq, "Sebaea microphylla 18S ribosomal RNA gene, partial \
sequence; internal transcribed spacer 1, 5.8S ribosomal RNA gene, and \
internal transcribed spacer 2, complete sequence; and 28S ribosomal RNA \
gene, partial sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2118)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(seq);
    misc1->SetComment("contains tRNA-Thr, tRNA-Pro, and control region");
    misc1->SetLocation().SetInt().SetFrom(0);
    misc1->SetLocation().SetInt().SetTo(15);
    misc1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc1->SetLocation().SetPartialStop(true, eExtreme_Biological);

    AddTitle(seq, "Sebaea microphylla tRNA-Thr gene, partial sequence; \
tRNA-Pro gene, complete sequence; and control region, partial sequence.");
    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);

}


BOOST_AUTO_TEST_CASE(Test_GB_1851)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(seq);
    misc1->SetComment("nonfunctional xyz due to argle");
    misc1->SetLocation().SetInt().SetFrom(0);
    misc1->SetLocation().SetInt().SetTo(15);
    misc1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc1->SetLocation().SetPartialStop(true, eExtreme_Biological);

    // kept because the misc_feature is alone
    AddTitle(seq, "Sebaea microphylla nonfunctional xyz gene, partial sequence.");
    CheckDeflineMatches(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
    CheckDeflineMatchesWithDescr(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);

    AddTitle(seq, "Sebaea microphylla nonfunctional xyz gene, partial sequence.");
    CheckDeflineMatches(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eNoncodingProductFeat);
    CheckDeflineMatchesWithDescr(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eNoncodingProductFeat);

    AddTitle(seq, "Sebaea microphylla nonfunctional xyz due to argle genomic sequence.");
    CheckDeflineMatches(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eCommentFeat);
    CheckDeflineMatchesWithDescr(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eCommentFeat);


    misc1->SetComment("similar to xyz");
    AddTitle(seq, "Sebaea microphylla xyz-like gene, partial sequence.");
    CheckDeflineMatches(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eNoncodingProductFeat);
    CheckDeflineMatchesWithDescr(seq, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eNoncodingProductFeat);

}


void s_SetProteinName(CRef<CSeq_entry> prot, const string& name)
{
    prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().front() = name;
}


CRef<CSeq_feat> s_AddCDS(CRef<CSeq_entry> np, const string& name, TSeqPos from, TSeqPos to)
{
    CRef<CSeq_entry> prev_prot = np->SetSet().SetSeq_set().back();
    CRef<CSeq_entry> new_prot (new CSeq_entry());
    new_prot->Assign(*prev_prot);
    CRef<CSeq_id> new_id(new CSeq_id());
    new_id->Assign(*(prev_prot->GetSeq().GetId().front()));
    size_t pos = NStr::Find(new_id->GetLocal().GetStr(), "_");
    string prefix = new_id->GetLocal().GetStr().substr(0, pos+ 1);
    string suffix = new_id->GetLocal().GetStr().substr(pos + 1);
    int prev_offset = NStr::StringToInt(suffix);
    new_id->SetLocal().SetStr(prefix + NStr::NumericToString(prev_offset + 1));
    unit_test_util::ChangeId(new_prot, new_id);
    s_SetProteinName(new_prot, name);
    np->SetSet().SetSeq_set().push_back(new_prot);

    CRef<CSeq_feat> prev_cds = np->SetSet().SetAnnot().front()->SetData().SetFtable().back();
    CRef<CSeq_feat> new_cds(new CSeq_feat());
    new_cds->Assign(*prev_cds);
    new_cds->SetProduct().SetWhole().Assign(*new_id);
    new_cds->SetLocation().SetInt().SetFrom(from);
    new_cds->SetLocation().SetInt().SetTo(to);
    np->SetSet().SetAnnot().front()->SetData().SetFtable().push_back(new_cds);
    return new_cds;
}


BOOST_AUTO_TEST_CASE(Test_GB_3942)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_entry> prot1 = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet (entry);

    unit_test_util::ChangeId(prot1, "_1");
    cds1->SetLocation().SetInt().SetFrom(0);
    cds1->SetLocation().SetInt().SetTo(5);
    cds1->SetProduct().SetWhole().Assign(*(prot1->GetSeq().GetId().front()));
    s_SetProteinName(prot1, "RNA-dependent RNA polymerase");

    CRef<CSeq_feat> cds2 = s_AddCDS(entry, "Coat protein", 10, 25);
    CRef<CSeq_feat> cds3 = s_AddCDS(entry, "Movement protein", 12, 20);

    cds1->SetLocation().SetPartialStart(true, eExtreme_Biological);

    AddTitle(nuc, "Sebaea microphylla RNA-dependent RNA polymerase gene, partial cds; and Coat protein and Movement protein genes, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);


    // actual splicing
    cds2->SetLocation().Assign(*(unit_test_util::MakeMixLoc(nuc->GetSeq().GetId().front())));
    cds3->SetLocation().Assign(cds2->GetLocation());
    TSeqPos old_end = cds3->GetLocation().GetMix().Get().back()->GetInt().GetTo();
    cds3->SetLocation().SetMix().Set().back()->SetInt().SetTo(old_end + 2);

    AddTitle(nuc, "Sebaea microphylla protein gene, complete cds, alternatively spliced; and RNA-dependent RNA polymerase gene, partial cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);

}


BOOST_AUTO_TEST_CASE(Test_GB_8927)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot1 = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    unit_test_util::ChangeId(prot1, "_1");
    cds1->SetLocation().SetInt().SetFrom(0);
    cds1->SetLocation().SetInt().SetTo(5);
    cds1->SetProduct().SetWhole().Assign(*(prot1->GetSeq().GetId().front()));
    s_SetProteinName(prot1, "RNA-dependent RNA polymerase");

    CRef<CSeq_feat> cds2 = s_AddCDS(entry, "Coat protein", 10, 25);
    CRef<CSeq_feat> cds3 = s_AddCDS(entry, "Movement protein", 12, 20);

    cds1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds2->SetLocation().Assign(*(unit_test_util::MakeMixLoc(nuc->GetSeq().GetId().front())));
    cds3->SetLocation().Assign(cds2->GetLocation());
    TSeqPos old_end = cds3->GetLocation().GetMix().Get().back()->GetInt().GetTo();
    cds3->SetLocation().SetMix().Set().back()->SetInt().SetTo(old_end + 2);

    unit_test_util::SetDiv(entry, "VRL");

    AddTitle(nuc, "Sebaea microphylla Movement protein and Coat protein genes, complete cds; and RNA-dependent RNA polymerase gene, partial cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_3926)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(seq);
    misc1->ResetComment();
    misc1->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    misc1->SetData().SetRna().SetExt().SetName("28S ribosomal RNA");
    misc1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc1->SetLocation().SetPartialStop(true, eExtreme_Biological);
    unit_test_util::SetOrgMod(seq, COrgMod::eSubtype_isolate, "JU6");
    unit_test_util::SetSubSource(seq, CSubSource::eSubtype_clone, "1");

    AddTitle(seq, "Sebaea microphylla isolate JU6 clone 1 28S ribosomal RNA gene, partial sequence.");

    vector<CSubSource::ESubtype> subsrcs;
    vector<COrgMod::ESubtype> orgmods;
    subsrcs.push_back(CSubSource::eSubtype_clone);
    orgmods.push_back(COrgMod::eSubtype_isolate);

    CheckDeflineMatches(seq, subsrcs, orgmods);
    CheckDeflineMatches(seq, subsrcs, orgmods, true);
}

BOOST_AUTO_TEST_CASE(Test_SQD_2181)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(nuc);
    misc1->ResetComment();
    misc1->SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> q(new CGb_qual());
    q->SetQual("regulatory_class");
    q->SetVal("promoter");
    misc1->SetQual().push_back(q);

    AddTitle(nuc, "Sebaea microphylla fake protein name gene, promoter region and complete cds.");

    auto sources = s_GatherSources(*entry);
    {
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetUseFakePromoters(true);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetUseFakePromoters(true);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}


BOOST_AUTO_TEST_CASE(Test_GB_3949)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "ATCC:12345");

    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    AddTitle(nuc, "Sebaea microphylla culture ATCC:12345 fake protein name gene, complete cds.");

    vector<CSubSource::ESubtype> subsrcs;
    vector<COrgMod::ESubtype> orgmods;
    orgmods.push_back(COrgMod::eSubtype_culture_collection);

    CheckDeflineMatches(entry, subsrcs, orgmods);
    CheckDeflineMatches(entry, subsrcs, orgmods, true);
}

BOOST_AUTO_TEST_CASE(Test_GB_4043)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    cds->SetLocation().SetInt().SetFrom(20);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    CRef<CSeq_feat> intron = unit_test_util::AddMiscFeature(nuc);
    intron->SetData().SetImp().SetKey("intron");
    intron->SetLocation().SetInt().SetFrom(0);
    intron->SetLocation().SetInt().SetTo(19);
    intron->SetLocation().SetPartialStart(true, eExtreme_Biological);
    intron->ResetComment();
    intron->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("number", "2")));
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(nuc);
    gene->SetData().SetGene().SetLocus("GAPDH");
    gene->SetLocation().SetInt().SetFrom(0);
    gene->SetLocation().SetInt().SetTo(cds->GetLocation().GetInt().GetTo());
    gene->SetLocation().SetPartialStart(true, eExtreme_Biological);
    gene->ResetComment();

    AddTitle(nuc, "Sebaea microphylla fake protein name (GAPDH) gene, intron 2 and partial cds.");

    auto sources = s_GatherSources(*entry);
    {
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = autodef.FindBestModifierCombo();

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = autodef.FindBestModifierCombo();

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}


BOOST_AUTO_TEST_CASE(Test_GB_4078)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    CRef<CSeq_feat> spacer = unit_test_util::AddMiscFeature(nuc);
    spacer->SetComment("G-L intergenic spacer");
    spacer->SetLocation().SetInt().SetFrom(cds->SetLocation().GetStart(eExtreme_Biological));
    spacer->SetLocation().SetInt().SetTo(cds->SetLocation().GetStart(eExtreme_Biological) + 2);
    spacer->SetLocation().SetPartialStop(true, eExtreme_Biological);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus("G");
    unit_test_util::AddFeat(gene, nuc);


    AddTitle(nuc, "Sebaea microphylla fake protein name (G) gene, partial cds; and G-L intergenic spacer, partial sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    unit_test_util::SetBiomol(nuc, CMolInfo::eBiomol_cRNA);
    nuc->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}

BOOST_AUTO_TEST_CASE(Test_SQD_2370)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(seq);
    misc1->SetComment("atpB-rbcL intergenic spacer region");

    AddTitle(seq, "Sebaea microphylla atpB-rbcL intergenic spacer region, complete sequence.");

    CheckDeflineMatches(seq);
    CheckDeflineMatchesWithDescr(seq);
}


BOOST_AUTO_TEST_CASE(Test_GB_4242)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(seq, "Trichoderma sp. FPZSP372");
    unit_test_util::SetOrgMod(seq, COrgMod::eSubtype_isolate, "FPZSP37");
    AddTitle(seq, "Trichoderma sp. FPZSP372 sequence.");

    vector<CSubSource::ESubtype> subsrcs;
    vector<COrgMod::ESubtype> orgmods;
    orgmods.push_back(COrgMod::eSubtype_isolate);

    CheckDeflineMatches(seq, subsrcs, orgmods);
    CheckDeflineMatches(seq, subsrcs, orgmods, true);

    // Try again, but deliberately allow modifier that includes taxname to be included
    AddTitle(seq, "Trichoderma sp. FPZSP372 isolate FPZSP37 sequence.");
    auto sources = s_GatherSources(*seq);
    
    {
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*seq);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();
        mod_combo->SetUseModifierLabels(true);
        mod_combo->SetAllowModAtEndOfTaxname(true);
        mod_combo->SetExcludeSpOrgs(false);
        ITERATE(vector<CSubSource::ESubtype>, it, subsrcs) {
            mod_combo->AddSubsource(*it, true);
        }
        ITERATE(vector<COrgMod::ESubtype>, it, orgmods) {
            mod_combo->AddOrgMod(*it, true);
        }

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();
        mod_combo->SetUseModifierLabels(true);
        mod_combo->SetAllowModAtEndOfTaxname(true);
        mod_combo->SetExcludeSpOrgs(false);
        ITERATE(vector<CSubSource::ESubtype>, it, subsrcs) {
            mod_combo->AddSubsource(*it, true);
        }
        ITERATE(vector<COrgMod::ESubtype>, it, orgmods) {
            mod_combo->AddOrgMod(*it, true);
        }

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*seq);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}

BOOST_AUTO_TEST_CASE(Test_SQD_3440)
{
    CAutoDefOptions options;
    CAutoDefModifierCombo combo;
    combo.InitOptions(options);

    CRef<CUser_object> user = options.MakeUserObject();
    BOOST_CHECK_EQUAL(user->GetObjectType(), CUser_object::eObjectType_AutodefOptions);
    options.SetUseLabels();
    user = options.MakeUserObject();
    CheckAutoDefOptions(*user, options);
}


BOOST_AUTO_TEST_CASE(Test_RemovableuORF)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("uORF");
    CRef<CSeqdesc> desc = AddSource(entry, "Alcanivorax sp. HA03");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    AddTitle(nuc, "Alcanivorax sp. HA03 uORF gene, complete cds.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    // try again, with another feature present, so uORF isn't lonely
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(nuc);
    misc->SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> q(new CGb_qual("satellite", "x"));
    misc->SetQual().push_back(q);
    AddTitle(nuc, "Alcanivorax sp. HA03 satellite x sequence.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    auto sources = s_GatherSources(*entry);
    {
        // try again, but set keepORFs flag
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetKeepuORFs(true);

        AddTitle(nuc, "Alcanivorax sp. HA03 uORF gene, complete cds; and satellite x sequence.");
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        // try again, but set keepORFs flag
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetKeepuORFs(true);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }

}

BOOST_AUTO_TEST_CASE(Test_RemovableMobileElement)
{
    // first, try with lonely optional
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> mob_el = unit_test_util::AddMiscFeature(entry);
    mob_el->SetData().SetImp().SetKey("mobile_element");
    CRef<CGb_qual> met(new CGb_qual("mobile_element_type", "SINE:x"));
    mob_el->SetQual().push_back(met);
    AddTitle(entry, "Sebaea microphylla SINE x, complete sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    // try again, with another feature present, so element isn't lonely
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> q(new CGb_qual("satellite", "y"));
    misc->SetQual().push_back(q);
    misc->SetLocation().SetInt().SetFrom(0);
    misc->SetLocation().SetInt().SetTo(10);
    mob_el->SetLocation().SetInt().SetFrom(15);
    mob_el->SetLocation().SetInt().SetTo(20);
    AddTitle(entry, "Sebaea microphylla satellite y sequence.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    auto sources = s_GatherSources(*entry);

    {
        // try again, but set keepMobileElements flag
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetKeepOptionalMobileElements(true);

        AddTitle(entry, "Sebaea microphylla satellite y sequence; and SINE x, complete sequence.");
        CheckDeflineMatches(seh, autodef, mod_combo);

        // keep non-optional mobile element when not lonely and flag not set
        met->SetVal("transposon:z");
        autodef.SetKeepOptionalMobileElements(false);
        AddTitle(entry, "Sebaea microphylla satellite y sequence; and transposon z, complete sequence.");
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        // try again, but set keepMobileElements flag
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetMiscFeatRule(CAutoDefOptions::eDelete);
        autodef.SetKeepOptionalMobileElements(true);

        met->SetVal("SINE:x");
        AddTitle(entry, "Sebaea microphylla satellite y sequence; and SINE x, complete sequence.");
        
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);

        // keep non-optional mobile element when not lonely and flag not set
        met->SetVal("transposon:z");
        autodef.SetKeepOptionalMobileElements(false);
        AddTitle(entry, "Sebaea microphylla satellite y sequence; and transposon z, complete sequence.");
        CheckDeflineMatches(seh, autodef, mod_combo);
    }

}

BOOST_AUTO_TEST_CASE(GB_5272)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("rhodanese-related sulfurtransferase");
    CRef<CSeqdesc> desc = AddSource(entry, "Coxiella burnetii");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus_tag("CBU_0065");
    AddFeat(gene, nuc);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    gene->SetLocation().SetPartialStart(true, eExtreme_Biological);
    AddTitle(nuc, "Coxiella burnetii rhodanese-related sulfurtransferase (CBU_0065) gene, partial cds.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(GB_5272a)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("hypothetical protein");
    CRef<CSeqdesc> desc = AddSource(entry, "Coxiella burnetii");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus_tag("CBU_0067");
    AddFeat(gene, nuc);
    gene->SetLocation().Assign(cds->GetLocation());

    CRef<CSeq_feat> cds2 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot2");
    cds2->SetLocation().SetInt().SetFrom(5);
    unit_test_util::AddFeat(cds2, entry);
    CRef<CSeq_entry> pentry = unit_test_util::MakeProteinForGoodNucProtSet("prot2");
    entry->SetSet().SetSeq_set().push_back(pentry);
    pentry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().front() = "hypothetical protein";
    CRef<CSeq_feat> gene2(new CSeq_feat());
    gene2->SetData().SetGene().SetLocus_tag("CBU_0068");
    AddFeat(gene2, nuc);
    gene2->SetLocation().Assign(cds2->GetLocation());

    AddTitle(nuc, "Coxiella burnetii hypothetical protein (CBU_0067) and hypothetical protein (CBU_0068) genes, complete cds.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    auto sources = s_GatherSources(*entry);

    {
        // try again, but suppress genes
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SuppressFeature(CSeqFeatData::eSubtype_gene);

        AddTitle(nuc, "Coxiella burnetii hypothetical protein genes, complete cds.");
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        // try again, but suppress genes
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = new CAutoDefModifierCombo();

        autodef.SuppressFeature(CSeqFeatData::eSubtype_gene);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}


BOOST_AUTO_TEST_CASE(GB_5272b)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("hypothetical protein");
    CRef<CSeqdesc> desc = AddSource(entry, "Coxiella burnetii");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);

    CRef<CSeq_feat> cds3 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot3");
    cds3->SetLocation().SetInt().SetFrom(5);
    unit_test_util::AddFeat(cds3, entry);
    CRef<CSeq_entry> pentry = unit_test_util::MakeProteinForGoodNucProtSet("prot3");
    entry->SetSet().SetSeq_set().push_back(pentry);
    pentry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().front() = "hypothetical protein";

    AddTitle(nuc, "Coxiella burnetii hypothetical protein genes, complete cds.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    // try again, but with intervening non-hypothetical protein gene
    CRef<CSeq_feat> cds2 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot2");
    cds2->SetLocation().SetInt().SetFrom(3);
    unit_test_util::AddFeat(cds2, entry);
    CRef<CSeq_entry> pentry2 = unit_test_util::MakeProteinForGoodNucProtSet("prot2");
    entry->SetSet().SetSeq_set().push_back(pentry2);
    pentry2->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().front() = "fake protein";

    AddTitle(nuc, "Coxiella burnetii hypothetical protein, fake protein, and hypothetical protein genes, complete cds.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}


BOOST_AUTO_TEST_CASE(SQD_3462)
{
    CRef<CSeq_entry> entry = BuildNucProtSet("brahma protein");
    CRef<CSeqdesc> desc = AddSource(entry, "Anas castanea");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_isolate, "DPIWECT127");
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(8);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> exon = unit_test_util::AddMiscFeature(nuc);
    exon->ResetComment();
    exon->SetData().SetImp().SetKey("exon");
    exon->SetLocation().SetInt().SetFrom(0);
    exon->SetLocation().SetInt().SetTo(8);
    CRef<CGb_qual> exon_number(new CGb_qual("number", "15"));
    exon->SetQual().push_back(exon_number);
    CRef<CSeq_feat> intron = unit_test_util::AddMiscFeature(nuc);
    intron->ResetComment();
    intron->SetData().SetImp().SetKey("intron");
    intron->SetLocation().SetInt().SetFrom(9);
    intron->SetLocation().SetInt().SetTo(nuc->GetSeq().GetLength() - 1);
    CRef<CGb_qual> intron_number(new CGb_qual("number", "15"));
    intron->SetQual().push_back(intron_number);

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(nuc);
    gene->ResetComment();
    gene->SetData().SetGene().SetLocus("BRM");
    gene->SetLocation().SetInt().SetTo(nuc->GetSeq().GetLength() - 1);

    AddTitle(nuc, "Anas castanea isolate DPIWECT127 brahma protein (BRM) gene, exon 15, intron 15, and partial cds.");
    auto sources = s_GatherSources(*entry);
    {
        CAutoDefWithTaxonomy autodef;

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        autodef.AddSources(seh);
        autodef.SetKeepExons(true);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        mod_combo->AddOrgMod(COrgMod::eSubtype_isolate, true);
        mod_combo->SetUseModifierLabels(true);


        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        CAutoDefWithTaxonomy autodef;
        
        autodef.AddDescriptors(sources);
        autodef.SetKeepExons(true);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        mod_combo->AddOrgMod(COrgMod::eSubtype_isolate, true);
        mod_combo->SetUseModifierLabels(true);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }

}

BOOST_AUTO_TEST_CASE(Test_IsModifierInString)
{
    // in the string, but ignore because it's at the end
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsModifierInString("abc", "abc", true), false);
    // in the string, report even at end
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsModifierInString("abc", "abc", false), true);
    // ignore because not whole word
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsModifierInString("abc", "tabc", false), false);
    // ignore because not whole word
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsModifierInString("abc", "abcq", false), false);
    // skip first match because not whole word, find second match
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsModifierInString("abc", "abcq abc", false), true);


}


BOOST_AUTO_TEST_CASE(Test_IsUsableInDefline)
{
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsUsableInDefline(CSubSource::eSubtype_plasmid_name), true);
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsUsableInDefline(CSubSource::eSubtype_collected_by), false);
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsUsableInDefline(COrgMod::eSubtype_strain), true);
    BOOST_CHECK_EQUAL(CAutoDefModifierCombo::IsUsableInDefline(COrgMod::eSubtype_variety), false);
}


BOOST_AUTO_TEST_CASE(Test_GB_5493)
{
    // first, try with lonely optional
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> miscrna = unit_test_util::AddMiscFeature(entry);
    miscrna->SetData().SetRna().SetType(CRNA_ref::eType_other);
    string remainder;
    miscrna->SetData().SetRna().SetRnaProductName("trans-spliced leader sequence SL", remainder);
    miscrna->SetComment("mini-exon");
    AddTitle(entry, "Sebaea microphylla trans-spliced leader sequence SL gene, complete sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_TargetedLocusName)
{
    CAutoDefOptions options;
    options.SetTargetedLocusName("consensus string");
    BOOST_CHECK_EQUAL(options.GetTargetedLocusName(), "consensus string");
    CRef<CUser_object> user = options.MakeUserObject();
    BOOST_CHECK_EQUAL(HasStringField(*user, "Targeted Locus Name", "consensus string"), 1);

    options.SetTargetedLocusName("other");
    BOOST_CHECK_EQUAL(options.GetTargetedLocusName(), "other");
    options.InitFromUserObject(*user);
    BOOST_CHECK_EQUAL(options.GetTargetedLocusName(), "consensus string");


}


BOOST_AUTO_TEST_CASE(Test_SQD_3602)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome(entry, CBioSource::eGenome_mitochondrion);
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetComment("contains tRNA-Pro gene, control region, tRNA-Phe  gene, and 12S ribosomal RNA gene");
    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddTitle(entry, "Sebaea microphylla tRNA-Pro gene, partial sequence; control region and tRNA-Phe gene, complete sequence; and 12S ribosomal RNA gene, partial sequence; mitochondrial.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_SB_5494)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome(entry, CBioSource::eGenome_mitochondrion);
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetComment("contains 12S ribosomal RNA gene, tRNA-Val (trnV) gene, and 16S ribosomal RNA gene");
    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddTitle(entry, "Sebaea microphylla 12S ribosomal RNA gene, partial sequence; tRNA-Val (trnV) gene, complete sequence; and 16S ribosomal RNA gene, partial sequence; mitochondrial.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_GB_5447)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot1 = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot1->SetData().SetProt().SetName().front() = "hypothetical protein";
    CRef<CSeq_feat> cds2 = unit_test_util::AddMiscFeature(nuc);
    cds2->SetData().SetCdregion();
    cds2->ResetComment();
    cds2->SetLocation().SetInt().SetFrom(cds1->GetLocation().GetStart(eExtreme_Positional));
    cds2->SetLocation().SetInt().SetTo(nuc->GetSeq().GetInst().GetLength() - 1);

    CRef<CSeq_entry> pentry(new CSeq_entry());
    pentry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    pentry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    pentry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIN");
    pentry->SetSeq().SetInst().SetLength(8);

    CRef<CSeq_id> pid(new CSeq_id());
    pid->SetLocal().SetStr("prot2");
    pentry->SetSeq().SetId().push_back(pid);
    entry->SetSet().SetSeq_set().push_back(pentry);
    cds2->SetProduct().SetWhole().SetLocal().SetStr("prot2");
    CRef<CSeq_feat> prot2 = unit_test_util::AddProtFeat(pentry);
    prot2->SetData().SetProt().SetName().front() = "hypothetical protein";

    AddTitle(nuc, "Sebaea microphylla hypothetical protein genes, complete cds.");
    CheckDeflineMatches(entry, true);
    CheckDeflineMatchesWithDescr(entry, true);

}


void MakeRegulatoryFeatureTest(const string& regulatory_class, const string& defline_interval, bool use_fake_promoters, bool keep_regulatory)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    if (!NStr::IsBlank(regulatory_class)) {
        CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
        feat->SetData().SetImp().SetKey("regulatory");
        CRef<CGb_qual> q(new CGb_qual("regulatory_class", regulatory_class));
        feat->SetQual().push_back(q);
    }
    AddTitle(nuc, "Sebaea microphylla fake protein name gene, " + defline_interval);

    {
        CAutoDefWithTaxonomy autodef;
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        autodef.AddSources(seh);
        if (use_fake_promoters) {
            autodef.SetUseFakePromoters(true);
        }
        if (keep_regulatory) {
            autodef.SetKeepRegulatoryFeatures(true);
        }

        CheckDeflineMatches(seh, autodef, mod_combo);
        scope->RemoveTopLevelSeqEntry(seh);
    }
    {
        CAutoDefWithTaxonomy autodef;
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        
        auto sources = s_GatherSources(*entry);
        autodef.AddDescriptors(sources);
        if (use_fake_promoters) {
            autodef.SetUseFakePromoters(true);
        }
        if (keep_regulatory) {
            autodef.SetKeepRegulatoryFeatures(true);
        }

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
        scope->RemoveTopLevelSeqEntry(seh);
    }
}


BOOST_AUTO_TEST_CASE(GB_5537)
{
    // a sequence with no promoter, but we set the FakePromoter flag
    MakeRegulatoryFeatureTest(kEmptyStr, "promoter region and complete cds.", true, false);

    // a sequence with a promoter, but no flags
    MakeRegulatoryFeatureTest("promoter", "complete cds.", false, false);

    // a sequence with a promoter, set the FakePromoter flag
    MakeRegulatoryFeatureTest("promoter", "promoter region and complete cds.", true, false);

    // a sequence with a promoter, set keep regulatory
    MakeRegulatoryFeatureTest("promoter", "promoter region and complete cds.", false, true);

    // a sequence with a promoter, set keep regulatory and FakePromoter
    MakeRegulatoryFeatureTest("promoter", "promoter region and complete cds.", true, true);

    // a sequence with an enhancer, but no flags
    MakeRegulatoryFeatureTest("enhancer", "complete cds.", false, false);

    // a sequence with an enhancer, set fake promoters flag
    MakeRegulatoryFeatureTest("enhancer", "promoter region and complete cds.", true, false);

    // a sequence with an enhancer, set keep regulatory
    MakeRegulatoryFeatureTest("enhancer", "enhancer and complete cds.", false, true);

}


BOOST_AUTO_TEST_CASE(Test_AutodefOptionsSpecifyNuclearCopyFlag)
{
    CAutoDefOptions opts;

    opts.SetNuclearCopyFlag(CBioSource::eGenome_mitochondrion);
    CRef<CUser_object> user = opts.MakeUserObject();
    BOOST_CHECK_EQUAL(HasStringField(*user, "NuclearCopyFlag", "mitochondrion"), 1);

}


BOOST_AUTO_TEST_CASE(Test_GB_5560)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->ResetComment();
    misc->SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> q(new CGb_qual("rpt_type", "long_terminal_repeat"));
    misc->SetQual().push_back(q);
    AddTitle(entry, "Sebaea microphylla LTR repeat region.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_GB_5758)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "a; minicircle b; c");
    AddTitle(entry, "Sebaea microphylla minicircle b sequence.");
    CheckDeflineMatches(entry);

    AddTitle(entry, "Sebaea microphylla a minicircle b sequence.");

    vector<CSubSource::ESubtype> subsrcs;
    subsrcs.push_back(CSubSource::eSubtype_other);
    vector<COrgMod::ESubtype> orgmods;
    CheckDeflineMatches(entry, subsrcs, orgmods);
    CheckDeflineMatches(entry, subsrcs, orgmods, true);
}


void TestForRecomb(CRef<CSeq_entry> entry, const string& expected)
{
    AddTitle(entry, expected);
    
    { 
        CAutoDefWithTaxonomy autodef;
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        autodef.AddSources(seh);
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetKeepMiscRecomb(true);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        auto sources = s_GatherSources(*entry);
        
        CAutoDefWithTaxonomy autodef;
        
        autodef.AddDescriptors(sources);
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetKeepMiscRecomb(true);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}


BOOST_AUTO_TEST_CASE(Test_GB_5793)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> m = unit_test_util::AddMiscFeature(entry);
    m->SetData().SetImp().SetKey("misc_recomb");
    m->SetComment("GCC2-ALK translocation breakpoint junction; microhomology");

    // by default, misc_recomb not included
    AddTitle(entry, "Sebaea microphylla sequence.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    // use option to show misc_recomb
    TestForRecomb(entry, "Sebaea microphylla GCC2-ALK translocation breakpoint junction genomic sequence.");

    // prefer recombination_class qualifier
    m->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("recombination_class", "mitotic_recombination")));
    TestForRecomb(entry, "Sebaea microphylla mitotic_recombination genomic sequence.");
}


BOOST_AUTO_TEST_CASE(Test_GB_5765)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> m = unit_test_util::AddMiscFeature(entry);
    AddTitle(entry, "Sebaea microphylla special flower.");
    auto sources = s_GatherSources(*entry);
    {
        CAutoDefWithTaxonomy autodef;
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        autodef.AddSources(seh);
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetCustomFeatureClause("special flower");
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        CAutoDefWithTaxonomy autodef;
        
        autodef.AddDescriptors(sources);
        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetCustomFeatureClause("special flower");

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}


BOOST_AUTO_TEST_CASE(Test_SQD_3914)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> m = unit_test_util::AddMiscFeature(entry);
    m->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    m->SetComment("contains 16S-23S ribosomal RNA intergenic spacer, tRNA-Ile(trnI), and tRNA-Ala(trnA)");
    AddTitle(entry, "Sebaea microphylla 16S-23S ribosomal RNA intergenic spacer, tRNA-Ile (trnI) and tRNA-Ala (trnA) genes, complete sequence.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


BOOST_AUTO_TEST_CASE(Test_CAutoDefAvailableModifier_GetOrgModLabel)
{
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_culture_collection), "culture");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_acronym), "acronym");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_strain), "strain");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_substrain), "substrain");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_type), "type");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_subtype), "subtype");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_variety), "variety");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_serotype), "serotype");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_serogroup), "serogroup");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_serovar), "serovar");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_cultivar), "cultivar");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_pathovar), "pathovar");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_chemovar), "chemovar");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_biovar), "biovar");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_biotype), "biotype");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_group), "group");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_subgroup), "subgroup");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_isolate), "isolate");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_authority), "authority");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_forma), "forma");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_ecotype), "ecotype");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_synonym), "synonym");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_anamorph), "anamorph");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_teleomorph), "teleomorph");
    BOOST_CHECK_EQUAL(CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::eSubtype_breed), "breed");
}


BOOST_AUTO_TEST_CASE(Test_GB_5618)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> utr3 = unit_test_util::AddMiscFeature(entry);
    utr3->SetLocation().SetInt().SetFrom(0);
    utr3->SetLocation().SetInt().SetTo(10);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(utr3);
    unit_test_util::AddFeat(gene1, entry);
    CRef<CSeq_feat> utr5 = unit_test_util::AddMiscFeature(entry);
    utr5->SetLocation().SetInt().SetFrom(20);
    utr5->SetLocation().SetInt().SetTo(25);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature(utr5);
    unit_test_util::AddFeat(gene2, entry);

    string defline = "Sebaea microphylla gene locus gene, complete sequence.";
    AddTitle(entry, defline);

    {
        CAutoDefWithTaxonomy autodef;

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        autodef.AddSources(seh);
        autodef.SetKeep3UTRs(true);
        autodef.SetKeep5UTRs(true);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        mod_combo->AddOrgMod(COrgMod::eSubtype_isolate, true);
        mod_combo->SetUseModifierLabels(true);

        defline = "Sebaea microphylla gene locus gene, 5' UTR and 3' UTR.";
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        auto sources = s_GatherSources(*entry);

        CAutoDefWithTaxonomy autodef;
        
        autodef.AddDescriptors(sources);
        autodef.SetKeep3UTRs(true);
        autodef.SetKeep5UTRs(true);

        CRef<CAutoDefModifierCombo> mod_combo(new CAutoDefModifierCombo());
        mod_combo->AddOrgMod(COrgMod::eSubtype_isolate, true);
        mod_combo->SetUseModifierLabels(true);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }
}

BOOST_AUTO_TEST_CASE(Test_GB_6375)
{
    // suppress if no number
    CRef<CSeq_entry> nps = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(nps);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(nps);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddExon(nuc, "", cds->GetLocation().GetStart(eExtreme_Positional));
    string defline = "Sebaea microphylla fake protein name gene, partial cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

    // show if has number
    nps = unit_test_util::BuildGoodNucProtSet();
    nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(nps);
    cds = unit_test_util::GetCDSFromGoodNucProtSet(nps);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    AddExon(nuc, "1", cds->GetLocation().GetStart(eExtreme_Positional));
    defline = "Sebaea microphylla fake protein name gene, exon 1 and partial cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

    // suppress if coding region complete
    cds->SetLocation().SetPartialStop(false, eExtreme_Biological);
    defline = "Sebaea microphylla fake protein name gene, complete cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_6557)
{
    // nuclear gene for X product (but not for macronuclear)
    CRef<CSeq_entry> nps = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(nps);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(nps);
    prot->SetData().SetProt().SetName().front() = "LIA2 macronuclear isoform";

    string defline = "Sebaea microphylla LIA2 macronuclear isoform gene, complete cds.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

    // apicoplast
    prot->SetData().SetProt().SetName().front() = "LIA2 apicoplast protein";
    defline = "Sebaea microphylla LIA2 apicoplast protein gene, complete cds; nuclear gene for apicoplast product.";
    AddTitle(nuc, defline);
    CheckDeflineMatches(nps, true);
    CheckDeflineMatchesWithDescr(nps, true);

}


BOOST_AUTO_TEST_CASE(Test_SQD_4185)
{
    CRef<CSeq_entry> seq = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetTaxname(seq, "Influenza A virus (A/USA/RVD1_H1/2011(H1N1))");
    unit_test_util::SetSubSource(seq, CSubSource::eSubtype_segment, "4");
    string defline = "Influenza A virus (A/USA/RVD1_H1/2011(H1N1)) segment 4 hemagglutinin (HA) gene, complete cds.";
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(seq);
    AddTitle(nuc, defline);
    unit_test_util::SetNucProtSetProductName(seq, "hemagglutinin");
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus("HA");
    AddFeat(gene, nuc);

    CheckDeflineMatches(seq, true);
    CheckDeflineMatchesWithDescr(seq, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_6690)
{
    // do not include notes in deflines when calculating uniqueness
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    vector<string> notes = { "a", "b", "c" };
    vector<string>::iterator nit = notes.begin();
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry->SetSet().SetSeq_set()) {
        AddTitle(*it, "Sebaea microphylla sequence.");
        unit_test_util::SetOrgMod(*it, COrgMod::eSubtype_other, *nit);
        ++nit;
    }
    entry->SetSet().ResetDescr();
    AddTitle(entry, "Sebaea microphylla sequence.");
    
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    
    CAutoDef autodef;
    autodef.AddSources(seh);

    CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();
    BOOST_CHECK_EQUAL(mod_combo->HasOrgMod(COrgMod::eSubtype_other), false);
    BOOST_CHECK_EQUAL(mod_combo->HasSubSource(CSubSource::eSubtype_other), false);

    CheckDeflineMatches(entry, true);

    scope->RemoveTopLevelSeqEntry(seh);

    nit = notes.begin();
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry->SetSet().SetSeq_set()) {
        unit_test_util::SetOrgMod(*it, COrgMod::eSubtype_other, "");
        unit_test_util::SetOrgMod(*it, CSubSource::eSubtype_other, *nit);
        ++nit;
    }
    seh = scope->AddTopLevelSeqEntry(*entry);
    CAutoDef autodef2;
    autodef2.AddSources(seh);
    mod_combo = autodef.FindBestModifierCombo();
    BOOST_CHECK_EQUAL(mod_combo->HasOrgMod(COrgMod::eSubtype_other), false);
    BOOST_CHECK_EQUAL(mod_combo->HasSubSource(CSubSource::eSubtype_other), false);

    CheckDeflineMatches(entry, true);
}

BOOST_AUTO_TEST_CASE(Test_GB_6690_WithDescr)
{
    // do not include notes in deflines when calculating uniqueness
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    vector<string> notes = { "a", "b", "c" };
    vector<string>::iterator nit = notes.begin();
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry->SetSet().SetSeq_set()) {
        AddTitle(*it, "Sebaea microphylla sequence.");
        unit_test_util::SetOrgMod(*it, COrgMod::eSubtype_other, *nit);
        ++nit;
    }
    entry->SetSet().ResetDescr();
    AddTitle(entry, "Sebaea microphylla sequence.");

    auto sources = s_GatherSources(*entry);
    CAutoDef autodef;
    autodef.AddDescriptors(sources);

    CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();
    BOOST_CHECK_EQUAL(mod_combo->HasOrgMod(COrgMod::eSubtype_other), false);
    BOOST_CHECK_EQUAL(mod_combo->HasSubSource(CSubSource::eSubtype_other), false);

    CheckDeflineMatchesWithDescr(entry, true);

    nit = notes.begin();
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry->SetSet().SetSeq_set()) {
        unit_test_util::SetOrgMod(*it, COrgMod::eSubtype_other, "");
        unit_test_util::SetOrgMod(*it, CSubSource::eSubtype_other, *nit);
        ++nit;
    }
    
    sources = s_GatherSources(*entry);
    CAutoDef autodef2;
    autodef2.AddDescriptors(sources);
    mod_combo = autodef.FindBestModifierCombo();
    BOOST_CHECK_EQUAL(mod_combo->HasOrgMod(COrgMod::eSubtype_other), false);
    BOOST_CHECK_EQUAL(mod_combo->HasSubSource(CSubSource::eSubtype_other), false);

    CheckDeflineMatchesWithDescr(entry, true);
}


CRef<CUser_field> MkField(const string& label, const string& val)
{
    CRef<CUser_field> f(new CUser_field());
    f->SetLabel().SetStr(label);
    f->SetData().SetStr(val);
    return f;
}


BOOST_AUTO_TEST_CASE(Test_HumanSTR)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("StructuredComment");
    obj->SetData().push_back(MkField("StructuredCommentPrefix", "##HumanSTR-START##"));
    obj->SetData().push_back(MkField("STR locus name", "TPOX"));
    obj->SetData().push_back(MkField("Length-based allele", "7"));
    obj->SetData().push_back(MkField("Bracketed repeat", "[AATG]7"));
    CRef<CSeqdesc> d(new CSeqdesc());
    d->SetUser().Assign(*obj);
    entry->SetSeq().SetDescr().Set().push_back(d);

    CRef<CSeq_feat> var = unit_test_util::AddMiscFeature(entry);
    var->SetData().SetImp().SetKey("variation");
    CRef<CDbtag> dbtag(new CDbtag());
    dbtag->SetDb("dbSNP");
    dbtag->SetTag().SetStr("rs115644759");
    var->SetDbxref().push_back(dbtag);

    string defline = "Sebaea microphylla microsatellite TPOX 7 [AATG]7 rs115644759 sequence.";
    AddTitle(entry, defline);

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}


BOOST_AUTO_TEST_CASE(Test_GB_7071)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> intron = unit_test_util::AddMiscFeature(entry);
    intron->SetData().SetImp().SetKey("intron");
    intron->SetComment("group A");

    string defline = "Sebaea microphylla intron.";
    AddTitle(entry, defline);

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}


BOOST_AUTO_TEST_CASE(Test_GB_7479)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    cds->ResetComment();
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetDesc("cullin 1");
    gene->ResetComment();
    gene->SetLocation().SetPartialStop(true, eExtreme_Biological);
    gene->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "allelic")));

    string defline = "Sebaea microphylla cullin 1 pseudogene, partial sequence.";
    AddTitle(entry, defline);

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}


void CheckInfluenzaDefline(const string& taxname, const string& strain, const string& serotype, const string& clone, const string& segment, const string& defline)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(entry, taxname);
    if (!NStr::IsBlank(strain)) {
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, strain);
    }
    if (!NStr::IsBlank(serotype)) {
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_serotype, serotype);
    }
    if (!NStr::IsBlank(clone)) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_clone, clone);
    }
    if (!NStr::IsBlank(segment)) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_segment, segment);
    }

    AddTitle(entry, defline);

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

}


BOOST_AUTO_TEST_CASE(Test_GB_7485)
{
    CheckInfluenzaDefline("Influenza A virus", "", "", "", "", "Influenza A virus sequence.");
    CheckInfluenzaDefline("Influenza B virus", "", "", "", "", "Influenza B virus sequence.");
    CheckInfluenzaDefline("Influenza A virus", "x", "", "", "", "Influenza A virus (x) sequence.");
    CheckInfluenzaDefline("Influenza B virus", "x", "", "", "", "Influenza B virus (x) sequence.");
    CheckInfluenzaDefline("Influenza A virus", "x", "y", "", "", "Influenza A virus (x(y)) sequence.");
    CheckInfluenzaDefline("Influenza B virus", "x", "y", "", "", "Influenza B virus (x) sequence.");
    CheckInfluenzaDefline("Influenza A virus", "", "y", "", "", "Influenza A virus ((y)) sequence.");
    CheckInfluenzaDefline("Influenza B virus", "", "y", "", "", "Influenza B virus sequence.");
    CheckInfluenzaDefline("Influenza A virus", "x", "y", "c", "", "Influenza A virus (x(y)) clone c sequence.");
    CheckInfluenzaDefline("Influenza B virus", "x", "y", "c", "", "Influenza B virus (x) clone c sequence.");
    CheckInfluenzaDefline("Influenza A virus", "x", "y", "", "1", "Influenza A virus (x(y)) segment 1 sequence.");
    CheckInfluenzaDefline("Influenza B virus", "x", "y", "", "1", "Influenza B virus (x) segment 1 sequence.");
    CheckInfluenzaDefline("Influenza A virus", "x", "y", "c", "1", "Influenza A virus (x(y)) clone c segment 1 sequence.");
    CheckInfluenzaDefline("Influenza B virus", "x", "y", "c", "1", "Influenza B virus (x) clone c segment 1 sequence.");

    CheckInfluenzaDefline("Influenza A virus (x(y))", "x", "y", "c", "1", "Influenza A virus (x(y)) clone c segment 1 sequence.");
    CheckInfluenzaDefline("Influenza C virus (x)", "x", "y", "c", "1", "Influenza C virus (x) clone c segment 1 sequence.");

}


BOOST_AUTO_TEST_CASE(Test_GB_7534)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetTaxname(entry, "Amomum chryseum");
    unit_test_util::SetGenome(entry, CBioSource::eGenome_chloroplast);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front() = "maturase K";

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(cds);
    gene1->SetData().SetGene().SetLocus("matK");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    AddFeat(gene1, nuc);
    cds->SetXref().push_back(CRef<CSeqFeatXref>(new CSeqFeatXref()));
    cds->SetXref().front()->SetData().Assign(gene1->GetData());

    CRef<CSeq_feat> gene2(new CSeq_feat());
    gene2->Assign(*gene1);
    gene2->SetData().SetGene().SetLocus("trnK");
    gene2->SetData().SetGene().SetDesc("tRNA-Lys");
    AddFeat(gene2, nuc);
    CRef<CSeq_feat> intron(new CSeq_feat());
    intron->Assign(*gene2);
    intron->SetData().SetImp().SetKey("intron");
    intron->SetXref().push_back(CRef<CSeqFeatXref>(new CSeqFeatXref()));
    intron->SetXref().front()->SetData().Assign(gene2->GetData());
    AddFeat(intron, nuc);

    AddTitle(entry, "Amomum chryseum tRNA-Lys (trnK) gene, intron; and maturase K (matK) gene, complete cds; chloroplast.");

    {
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = autodef.FindBestModifierCombo();


        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        auto sources = s_GatherSources(*entry);
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);
        autodef.SetKeepIntrons(true);

        CRef<CAutoDefModifierCombo> mod_combo;
        mod_combo = autodef.FindBestModifierCombo();

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }

}


BOOST_AUTO_TEST_CASE(Test_SQD_4451)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource(entry, "Fusarium incarnatum");
    CRef<CSeq_feat> feat1(new CSeq_feat());
    feat1->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    feat1->SetData().SetRna().SetExt().SetName("internal transcribed spacer region");
    AddFeat(feat1, entry);
    feat1->SetLocation().SetInt().SetFrom(0);
    feat1->SetLocation().SetInt().SetTo(59);
    feat1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    feat1->SetLocation().SetPartialStop(true, eExtreme_Biological);

    AddTitle(entry, "Fusarium incarnatum internal transcribed spacer region, partial sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_SQD_4529)
{
    CRef<CSeq_entry> entry = BuildSequence();
    CRef<CSeqdesc> desc = AddSource(entry, "Fusarium incarnatum");
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature(entry);
    feat1->SetComment("similar to beta-tubulin");

    AddTitle(entry, "Fusarium incarnatum beta-tubulin-like gene, complete sequence.");

    CheckDeflineMatches(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);

    CRef<CSeq_feat> rrna1 = unit_test_util::AddMiscFeature(entry);
    rrna1->ResetComment();
    rrna1->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rrna1->SetData().SetRna().SetExt().SetName("foo");
    AddTitle(entry, "Fusarium incarnatum foo gene, complete sequence.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);

}


void AddProtFeat(CRef<CSeq_entry> prot, CProt_ref::EProcessed proc)
{
    CRef<CSeq_feat> p = unit_test_util::AddMiscFeature(prot);
    p->SetData().SetProt().SetProcessed(proc);
    p->SetData().SetProt().SetName().clear();
    p->SetData().SetProt().SetName().push_back("RdRp");
    p->ResetComment();
}


void TestMatPeptideListing(bool cds_is_partial, bool has_sig_peptide)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    if (cds_is_partial) {
        cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
        cds->SetPartial(true);
    }
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus("ORF1");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    unit_test_util::AddFeat(gene, nuc);
    CRef<CSeq_feat> pfeat = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    pfeat->SetData().SetProt().SetName().clear();
    pfeat->SetData().SetProt().SetName().push_back("nonstructural polyprotein");
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    AddProtFeat(prot, CProt_ref::eProcessed_mature);
    if (has_sig_peptide) {
        AddProtFeat(prot, CProt_ref::eProcessed_signal_peptide);
    }

    if (cds_is_partial) {
        if (has_sig_peptide) {
            AddTitle(nuc, "Sebaea microphylla nonstructural polyprotein (ORF1) gene, partial cds.");
        } else {
            AddTitle(nuc, "Sebaea microphylla nonstructural polyprotein, RdRp region, (ORF1) gene, partial cds.");
        }
    } else {
        AddTitle(nuc, "Sebaea microphylla nonstructural polyprotein (ORF1) gene, complete cds.");
    }
    CheckDeflineMatches(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);

}

BOOST_AUTO_TEST_CASE(Test_SQD_4593)
{
    TestMatPeptideListing(true, false);
    TestMatPeptideListing(true, true);
    TestMatPeptideListing(false, false);
    TestMatPeptideListing(false, true);
}


BOOST_AUTO_TEST_CASE(Test_SQD_4607)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature(entry);
    feat1->SetComment("contains promoter and 5' UTR");

    AddTitle(entry, "Sebaea microphylla promoter region and 5' UTR, genomic sequence.");

    CheckDeflineMatches(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eListAllFeatures, CAutoDefOptions::eDelete);
}


void CheckRegulatoryFeatures(const string& expected_title, bool keep_promoters, bool keep_regulatory)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> promoter = unit_test_util::AddMiscFeature(entry);
    promoter->SetData().SetImp().SetKey("regulatory");
    promoter->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("regulatory_class", "promoter")));
    promoter->ResetComment();
    CRef<CSeq_feat> rbs = unit_test_util::AddMiscFeature(entry);
    rbs->SetData().SetImp().SetKey("regulatory");
    rbs->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("regulatory_class", "ribosome_binding_site")));
    rbs->ResetComment();

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("msa");
    gene->SetData().SetGene().SetDesc("mannose-specific adhesin");
    gene->ResetComment();

    AddTitle(entry, expected_title);
    {
        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddSources(seh);

        CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetKeepRegulatoryFeatures(keep_regulatory);
        autodef.SetUseFakePromoters(keep_promoters);

        CheckDeflineMatches(seh, autodef, mod_combo);
    }
    {
        auto sources = s_GatherSources(*entry);
        CAutoDefWithTaxonomy autodef;

        // add to autodef
        autodef.AddDescriptors(sources);

        CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();

        autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
        autodef.SetKeepRegulatoryFeatures(keep_regulatory);
        autodef.SetUseFakePromoters(keep_promoters);

        CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*object_manager));
        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
        CheckDeflineMatches(seh, autodef, mod_combo);
    }

}


BOOST_AUTO_TEST_CASE(Test_SQD_4612)
{
    CheckRegulatoryFeatures("Sebaea microphylla mannose-specific adhesin (msa) gene, promoter region.", false, false);
    CheckRegulatoryFeatures("Sebaea microphylla mannose-specific adhesin (msa) gene, promoter region.", true, false);
    CheckRegulatoryFeatures("Sebaea microphylla mannose-specific adhesin (msa) gene, promoter region and ribosome_binding_site.", true, true);

}

BOOST_AUTO_TEST_CASE(Test_GB_8547)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(entry, "Influenza A virus");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "A/Florida/57/2019");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_segment, "5");

    AddTitle(entry, "Influenza A virus (A/Florida/57/2019) segment 5 sequence.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

BOOST_AUTO_TEST_CASE(Test_GB_8604)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    CRef<CSeq_feat> pfeat = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    pfeat->SetData().SetProt().SetName().front() = "proannomuricatin G";
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName("proannomuricatin G");
    unit_test_util::AddFeat(mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(mrna);
    gene->SetData().SetGene().SetLocus("PamG");
    unit_test_util::AddFeat(gene, nuc);

    // check without mat-peptide first
    AddTitle(nuc, "Sebaea microphylla proannomuricatin G (PamG) gene, partial cds.");

    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    // check with mat-peptide
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> mat_peptide = unit_test_util::AddMiscFeature(prot);
    mat_peptide->ResetComment();
    mat_peptide->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_mature);
    mat_peptide->SetData().SetProt().SetName().push_back("annomuricatin G");

    // if suppressing mat-peptide, no change
    CheckDeflineMatches(entry, CSeqFeatData::eSubtype_mat_peptide_aa);
    CheckDeflineMatches(entry, CSeqFeatData::eSubtype_mat_peptide_aa, true);

    // show when not suppressing
    AddTitle(entry, "Sebaea microphylla proannomuricatin G, annomuricatin G region, (PamG) gene, partial cds.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);
}

CRef<CSeq_feat> MakeRegulatoryFeature(const string& reg_class, const string& comment, TSeqPos start_pos, CRef<CSeq_entry> entry)
{
    CRef<CSeq_feat> reg = unit_test_util::AddMiscFeature(entry);
    reg->SetData().SetImp().SetKey("regulatory");
    reg->SetComment(comment);
    reg->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("regulatory_class", reg_class)));
    reg->SetLocation().SetInt().SetFrom(start_pos);
    reg->SetLocation().SetInt().SetTo(start_pos + 4);
    return reg;
}

CRef<CSeq_feat> MakeRptRegion(const string& rpt_type, TSeqPos start_pos, CRef<CSeq_entry> entry)
{
    CRef<CSeq_feat> reg = unit_test_util::AddMiscFeature(entry);
    reg->ResetComment();
    reg->SetData().SetImp().SetKey("repeat_region");
    reg->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("rpt_type", rpt_type)));
    reg->SetLocation().SetInt().SetFrom(start_pos);
    reg->SetLocation().SetInt().SetTo(start_pos + 4);
    return reg;
}


void TestRepeatRegion(CRef<CSeq_entry> entry, bool init_with_descrs = false)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CAutoDefWithTaxonomy autodef;

    if (init_with_descrs) {
        CAutoDef::TSources sources;
        for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter; ++b_iter) {
            for (CSeqdesc_CI desc_it(*b_iter, CSeqdesc::e_Source); desc_it; ++desc_it) {
                sources.emplace_back(&desc_it->GetSource());
            }
        }
        autodef.AddDescriptors(sources);
    }
    else {
        // add to autodef
        autodef.AddSources(seh);
    }

    CRef<CAutoDefModifierCombo> mod_combo = autodef.FindBestModifierCombo();
    autodef.SetFeatureListType(CAutoDefOptions::eListAllFeatures);
    autodef.SetKeepRepeatRegion(true);

    CheckDeflineMatches(seh, autodef, mod_combo);

}

BOOST_AUTO_TEST_CASE(Test_GB_8854)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> rpt = MakeRptRegion("long_terminal_repeat", 15, entry);
    AddTitle(entry, "Sebaea microphylla LTR repeat region.");
    CheckDeflineMatches(entry);
    CheckDeflineMatchesWithDescr(entry);

    TestRepeatRegion(entry);
    TestRepeatRegion(entry, true);

    CRef<CSeq_feat> reg1 = MakeRegulatoryFeature("CAAT_signal", "U3 region", 0, entry);
    CRef<CSeq_feat> reg2 = MakeRegulatoryFeature("TATA_box", "U3 region", 5, entry);
    CRef<CSeq_feat> reg3 = MakeRegulatoryFeature("polyA_signal_sequence", "R-region", 10, entry);

    TestRepeatRegion(entry);
    TestRepeatRegion(entry, true);
}


BOOST_AUTO_TEST_CASE(Test_ClauseListOptions)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    AddTitle(entry, "Sebaea microphylla, complete sequence.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::eCompleteSequence);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eCompleteSequence);

    AddTitle(entry, "Sebaea microphylla, complete genome.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::eCompleteGenome);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eCompleteGenome);

    AddTitle(entry, "Sebaea microphylla, partial sequence.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::ePartialSequence);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::ePartialSequence);

    AddTitle(entry, "Sebaea microphylla, partial genome.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::ePartialGenome);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::ePartialGenome);

    AddTitle(entry, "Sebaea microphylla, whole genome shotgun sequence.");
    CheckDeflineMatches(entry, true, CAutoDefOptions::eWholeGenomeShotgunSequence);
    CheckDeflineMatchesWithDescr(entry, true, CAutoDefOptions::eWholeGenomeShotgunSequence);
}

END_SCOPE(objects)
END_NCBI_SCOPE
