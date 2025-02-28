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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- comment item implementation
*
*/
#include <ncbi_pch.hpp>

#include <sstream>

#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/general_macros.hpp>
#include <objects/misc/sequence_util_macros.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/context.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// static variables initialization
// bool CCommentItem::sm_FirstComment = true;

static const string kRefSeq = "REFSEQ";
static const string kRefSeqInformation = "REFSEQ INFORMATION";
static const string kRefSeqLink = "<a href=\"https://www.ncbi.nlm.nih.gov/RefSeq/\">REFSEQ</a>";
static const string kRefSeqInformationLink = "<a href=\"https://www.ncbi.nlm.nih.gov/RefSeq/\">REFSEQ INFORMATION</a>";

/////////////////////////////////////////////////////////////////////////////
//
//  CCommentItem

CCommentItem::CCommentItem(CBioseqContext& ctx, bool need_period) :
    CFlatItem(&ctx),
    m_CommentInternalIndent(0),
    m_First(false),
    m_NeedPeriod(need_period)
{
    // swap(m_First, sm_FirstComment);
}

IFlatItem::EItem CCommentItem::GetItemType(void) const
{
    return eItem_Comment;
}


CCommentItem::CCommentItem
(const string& comment,
 CBioseqContext& ctx,
 const CSerialObject* obj) :
    CFlatItem(&ctx),
    m_CommentInternalIndent(0),
    m_First(false),
    m_NeedPeriod(true)
{
    m_Comment.push_back( comment );
    if (! ctx.Config().IsFormatGBSeq() && ! ctx.Config().IsFormatINSDSeq()) {
        ExpandTildes(m_Comment.back(), eTilde_comment);
    }
    // swap(m_First, sm_FirstComment);
    if (obj) {
        x_SetObject(*obj);
    }
}


CCommentItem::CCommentItem(const CSeqdesc&  desc, CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_CommentInternalIndent(0),
    m_First(false),
    m_NeedPeriod(true)
{
    // swap(m_First, sm_FirstComment);
    x_SetObject(desc);
    x_GatherInfo(ctx);
    if ( x_IsCommentEmpty() ) {
        x_SetSkip();
    }
}


CCommentItem::CCommentItem(const CSeq_feat& feat, CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_CommentInternalIndent(0),
    m_First(false),
    m_NeedPeriod(true)
{
    // swap(m_First, sm_FirstComment);
    x_SetObject(feat);
    x_GatherInfo(ctx);
    NON_CONST_ITERATE( list<string>, it, m_Comment ) {
        TrimSpacesAndJunkFromEnds( *it );
    }
    if ( x_IsCommentEmpty() ) {
        x_SetSkip();
    }
}

CCommentItem::CCommentItem(const CUser_object & userObject, CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_CommentInternalIndent(0),
    m_First(false),
    m_NeedPeriod(true)
{
    // swap(m_First, sm_FirstComment);
    x_SetObject(userObject);
    x_GatherInfo(ctx);
    if ( x_IsCommentEmpty() ) {
        x_SetSkip();
    }
}


void CCommentItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatComment(*this, text_os);
}


void CCommentItem::AddPeriod(void)
{
    if( ! m_Comment.empty() ) {
        const bool ends_with_ellipsis = NStr::EndsWith(m_Comment.back(), "...");
        ncbi::objects::AddPeriod(m_Comment.back());
        if( ends_with_ellipsis ) {
            // finish the ellipsis
            m_Comment.back() += "..";
        }
    }
}

void CCommentItem::RemoveExcessNewlines(
    const CCommentItem & next_comment )
{
    if(  m_Comment.empty() || next_comment.m_Comment.empty() ) {
        return;
    }

    // check if next_comment starts with an empty line
    const string & next_comment_first_string = next_comment.m_Comment.front();
    bool next_comment_starts_with_empty_line = false;
    ITERATE( string, next_com_line_it, next_comment_first_string ) {
        const char ch = *next_com_line_it;
        if( ch == '\n' ) {
            next_comment_starts_with_empty_line = true;
            break;
        } else if( ! isspace(ch) ) {
            break;
        }
    }

    if( ! next_comment_starts_with_empty_line ) {
        // we assume that this comment won't have excessive blank lines
        return;
    }

    // see if we have too many newlines at the end (we assume we don't have more than
    // one extra)
    string & last_str_of_comment = m_Comment.back();
    if( last_str_of_comment.empty() ) {
        return;
    }

    string::size_type pos = (last_str_of_comment.length() - 1);
    if( last_str_of_comment[pos] == '\n' ) {
        // skip final newlines because lines without newline will get
        // a newline added, so we would assume it's there anyway
        --pos;
    }
    for( ; pos < last_str_of_comment.length(); --pos ) {
        const char ch = last_str_of_comment[pos];

        if( ch == '\n' ) {
            // extra newline found: remove it
            last_str_of_comment.erase(pos);
            return;
        } else if( ! isspace(ch) ) {
            return;
        }
    }
}

void CCommentItem::RemovePeriodAfterURL(void)
{
    if( m_Comment.empty() ) {
        return;
    }

    // remove period if it's after a '/', though.
    if( NStr::EndsWith(m_Comment.back(), "/.") ) {
        m_Comment.back().resize( m_Comment.back().length() - 1 );
    }
}

const string& CCommentItem::GetNsAreGapsStr(void)
{
    static const string kNsAreGaps = "The strings of n's in this record represent " \
        "gaps between contigs, and the length of each string corresponds " \
        "to the length of the gap.";
    return kNsAreGaps;
}

typedef CConstRef<CSeq_align>            TAln;
typedef list< CRef< CSeq_align > >       TAlnList;
typedef list< CConstRef< CSeq_align > >  TAlnConstList;
typedef multimap<CAlnMap::TRange,  TAln> TAlnMap;

void x_CollectSegments
(TAlnConstList& seglist,
 const TAlnList& aln_list);

void x_CollectSegments
(TAlnConstList& seglist, const CSeq_align& aln)
{
    if ( !aln.CanGetSegs() ) {
        return;
    }

    if ( aln.GetSegs().IsDenseg() ) {
        seglist.push_back( TAln(&aln) );
    } else if ( aln.GetSegs().IsDisc() ) {
        x_CollectSegments(seglist, aln.GetSegs().GetDisc().Get());
    }
}

void x_CollectSegments
(TAlnConstList& seglist,
 const TAlnList& aln_list)
{
    ITERATE (TAlnList, it, aln_list) {
        x_CollectSegments(seglist, **it);
    }
}

string CCommentItem::GetStringForTPA
(const CUser_object& uo,
 CBioseqContext& ctx)
{
    static const string tpa_string =
        "THIRD PARTY DATABASE: This TPA record uses data from DDBJ/EMBL/GenBank ";

    if ( !ctx.IsTPA() || ctx.IsRefSeq() ) {
        return kEmptyStr;
    }
    if ( !uo.CanGetType() || !uo.GetType().IsStr() ||
         uo.GetType().GetStr() != "TpaAssembly" ) {
        return kEmptyStr;
    }

    CBioseq_Handle& seq = ctx.GetHandle();
    if (seq.IsSetInst_Hist()  &&  seq.GetInst_Hist().IsSetAssembly() && (! ctx.Config().OldTpaDisplay())) {
        // return kEmptyStr;
        TAlnConstList seglist;
        x_CollectSegments(seglist, seq.GetInst_Hist().GetAssembly());

        vector<string> histaccns;

        CConstRef<CSeq_id> other_id;

        ITERATE( TAlnConstList, it, seglist ) {
            const CSeq_align& align = **it;

            other_id.Reset(&align.GetSeq_id(1));
            if (!other_id) {
                continue;
            }
            if (other_id->IsGi()) {

                // don't show PRIMARY line if network access unavailable (and hence can't translate gi)
              CSeq_id_Handle idh = GetId(*other_id, ctx.GetScope(), sequence::eGetId_Best);
                if( ! idh ) {
                    continue;
                }

                other_id = idh.GetSeqId();
                if (other_id->IsGi()) {
                    continue;
                }
            }
            string tid = other_id->GetSeqIdString(true);
            if (other_id->IsGeneral()) {
                const CDbtag& dbt = other_id->GetGeneral();
                if (dbt.IsSetDb()  &&  NStr::EqualNocase(dbt.GetDb(), "TI")) {
                    NStr::ReplaceInPlace (tid, "ti:", "TI");
                 }
            }
            if ( !tid.empty() ) {
                histaccns.push_back(NStr::ToUpper(tid));
            }
        }
        if ( histaccns.empty() ) {
            return kEmptyStr;
        }

        sort( histaccns.begin(), histaccns.end() );
        histaccns.erase( unique( histaccns.begin(), histaccns.end() ), histaccns.end() );

        stringstream text;
        text << tpa_string << ((histaccns.size() > 1) ? "entries " : "entry ");

        size_t size = histaccns.size();
        size_t last = size - 1;

        for ( size_t i = 0; i < size; ) {
            text << histaccns[i];
            ++i;
            if ( i < size ) {
                text << ((i == last) ? " and " : ", ");
            }
        }

        return text.str();
    }

    string id;
    vector<string> accessions;
    ITERATE (CUser_object::TData, curr, uo.GetData()) {
        const CUser_field& uf = **curr;
        if ( !uf.CanGetData()  ||  !uf.GetData().IsFields() ) {
            continue;
        }

        ITERATE (CUser_field::C_Data::TFields, ufi, uf.GetData().GetFields()) {
            if( !(*ufi)->CanGetData()  ||  !(*ufi)->GetData().IsStr()  ||
                !(*ufi)->CanGetLabel() ) {
                continue;
            }
            const CObject_id& oid = (*ufi)->GetLabel();
            if ( oid.IsStr() &&
                 (NStr::CompareNocase(oid.GetStr(), "accession") == 0) ) {
                string acc = (*ufi)->GetData().GetStr();
                if ( !acc.empty() ) {
                    accessions.push_back(NStr::ToUpper(acc));
                }
            }
        }
    }
    if ( accessions.empty() ) {
        return kEmptyStr;
    }

    stringstream text;
    text << tpa_string << ((accessions.size() > 1) ? "entries " : "entry ");

    size_t size = accessions.size();
    size_t last = size - 1;

    for ( size_t i = 0; i < size; ) {
        text << accessions[i];
        ++i;
        if ( i < size ) {
            text << ((i == last) ? " and " : ", ");
        }
    }

    return text.str();
}


string CCommentItem::GetStringForBankIt(const CUser_object& uo, bool dump_mode)
{
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "Submission" ) {
        return kEmptyStr;
    }

    const string *uvc = nullptr, *bic = nullptr, *smc = nullptr;

    if ( uo.HasField("UniVecComment") ) {
        const CUser_field& uf = uo.GetField("UniVecComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            uvc = &(uf.GetData().GetStr());
        }
    }
    if ( uo.HasField("AdditionalComment") ) {
        const CUser_field& uf = uo.GetField("AdditionalComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            bic = &(uf.GetData().GetStr());
        }
    }
    if ( uo.HasField("SmartComment") && dump_mode ) {
        const CUser_field& uf = uo.GetField("SmartComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            smc = &(uf.GetData().GetStr());
        }
    }

    stringstream text;
    string pfx;
    if (uvc) {
        text << pfx << "Vector Explanation: " << *uvc;
        pfx = "~";
    }
    if (bic) {
        text << pfx << "Bankit Comment: " << *bic;
        pfx = "~";
    }
    if (smc) {
        text << pfx << "Bankit Comment: " << *smc;
        pfx = "~";
    }

    return text.str();
}


static
void s_GetAssemblyInfo(const CBioseqContext& ctx, string& s, const CUser_object& uo)
{
    s.clear();

    //const bool is_html = ctx.Config().DoHTML();
    vector<string> assembly_pieces;

    if ( uo.HasField("Assembly") ) {
        const CUser_field& field = uo.GetField("Assembly");
        if ( !field.GetData().IsFields() ) {
            return;
        }

        ITERATE (CUser_field::C_Data::TFields, fit,
            field.GetData().GetFields())
        {
            if ( !(*fit)->GetData().IsFields() ) {
                continue;
            }

            string accession;
            string name;
            // gi currently unused, but may be used in the future.
            // If you uncomment this, don't forget to uncomment other
            // locations in this function
            // int gi = 0;
            int from = 0;
            int to = 0;

            ITERATE (CUser_field::C_Data::TFields, it,
                (*fit)->GetData().GetFields())
            {
                const CUser_field& uf = **it;
                if ( !uf.CanGetLabel()  ||  !uf.GetLabel().IsStr() || ! uf.IsSetData() ) {
                    continue;
                }
                const string& label = uf.GetLabel().GetStr();

                if( uf.GetData().IsStr() ) {
                    if( label == "accession" ) {
                        accession = uf.GetData().GetStr();
                    } else if( label == "name" ) {
                        name = uf.GetData().GetStr();
                    }
                } else if( uf.GetData().IsInt() ) {
                    if( label == "gi" ) {
                        // gi currently unused, but may be used in the future.
                        // If you uncomment this, don't forget to uncomment other
                        // locations in this function
                        // gi = uf.GetData().GetInt();
                    } else if( label == "from" ) {
                        from = uf.GetData().GetInt();
                    } else if( label == "to" ) {
                        to = uf.GetData().GetInt();
                    }
                }
            }

            if ( ! accession.empty() ) {
                stringstream oss;

                // gi currently unused, but may be used in the future.
                // If you uncomment this, don't forget to uncomment other
                // locations in this function
                // try {
                //     int new_gi = sequence::GetGiForAccession( accession, scope, sequence::eGetId_ForceGi | sequence::eGetId_VerifyId );
                //     if( 0 != new_gi ) {
                //         gi = new_gi;
                //     }
                // } catch(...) {
                //     // do nothing, we know there's an error because new_gi is zero
                // }
                if (IsValidAccession(accession)) {
                    ctx.Config().GetHTMLFormatter().FormatGeneralId(oss, accession);
                } else {
                    oss << accession;
                }

                if( from > 0 && to > 0 ) {
                    oss << " (range: " << from << "-" << to << ")";
                }

                string new_piece = oss.str();
                assembly_pieces.push_back( new_piece );
            } else if( ! name.empty() ) {
                assembly_pieces.push_back( name );
            }
        }
    }

    if( ! assembly_pieces.empty() ) {
        ostringstream oss;
        oss << " The reference sequence was derived from ";

        size_t assembly_size = assembly_pieces.size();
        for ( size_t ii = 0; ii < assembly_size; ++ii ) {
            if ( ii > 0  ) {
                oss << ((ii < assembly_size - 1) ? ", " : " and ");
            }
            oss << assembly_pieces[ii];
        }
        oss << '.';

        s = oss.str();
    }
}


CCommentItem::TRefTrackStatus CCommentItem::GetRefTrackStatus
(const CUser_object& uo,
 string* st)
{
    TRefTrackStatus retval = eRefTrackStatus_Unknown;
    if (st) {
        st->erase();
    }
    if ( !uo.HasField("Status") ) {
        return retval;
    }

    const CUser_field& field = uo.GetField("Status");
    if ( field.GetData().IsStr() ) {
        string status = field.GetData().GetStr();
        if (NStr::EqualNocase(status, "Inferred")) {
            retval = eRefTrackStatus_Inferred;
        } else if (NStr::EqualNocase(status, "Provisional")) {
            retval = eRefTrackStatus_Provisional;
        } else if (NStr::EqualNocase(status, "Predicted")) {
            retval = eRefTrackStatus_Predicted;
        } else if (NStr::EqualNocase(status, "Pipeline")) {
            retval = eRefTrackStatus_Pipeline;
        } else if (NStr::EqualNocase(status, "Validated")) {
            retval = eRefTrackStatus_Validated;
        } else if (NStr::EqualNocase(status, "Reviewed")) {
            retval = eRefTrackStatus_Reviewed;
        } else if (NStr::EqualNocase(status, "Model")) {
            retval = eRefTrackStatus_Model;
        } else if (NStr::EqualNocase(status, "WGS")) {
            retval = eRefTrackStatus_WGS;
        } else if (NStr::EqualNocase(status, "TSA")) {
            retval = eRefTrackStatus_TSA;
        }

        if (st && retval != eRefTrackStatus_Unknown) {
            *st = NStr::ToUpper(status);
        }
    }

    return retval;
}

string CCommentItem::GetStringForRefTrack(const CBioseqContext& ctx, const CUser_object& uo,
    const CBioseq_Handle& bsh,
    EGenomeBuildComment eGenomeBuildComment )
{
    bool is_html = ctx.Config().DoHTML();

    if ( !uo.IsSetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "RefGeneTracking") {
        return kEmptyStr;
    }

    TRefTrackStatus status = eRefTrackStatus_Unknown;
    string status_str;
    status = GetRefTrackStatus(uo, &status_str);
    if ( status == eRefTrackStatus_Unknown ) {
        return kEmptyStr;
    }

    string collaborator;
    if ( uo.HasField("Collaborator") ) {
        const CUser_field& colab_field = uo.GetField("Collaborator");
        if ( colab_field.GetData().IsStr() ) {
            collaborator = colab_field.GetData().GetStr();
        }
    }

    string source;
    if ( uo.HasField("GenomicSource") ) {
        const CUser_field& source_field = uo.GetField("GenomicSource");
        if ( source_field.GetData().IsStr() ) {
            source = source_field.GetData().GetStr();
        }
    }

    string identical_to_start;
    string identical_to_end;
    string identical_to;

    // "accession" overrides "name", which in turn overrides "gi"
    enum EIdenticalToPriority {
        eIdenticalToPriority_Nothing = 1,
        eIdenticalToPriority_Gi,
        eIdenticalToPriority_Name,
        eIdenticalToPriority_Accn
    };
    int identical_to_priority = eIdenticalToPriority_Nothing;

    if (uo.HasField("IdenticalTo")) {
        const CUser_field& uf = uo.GetField("IdenticalTo");
        ITERATE (CUser_field::TData::TFields, it, uf.GetData().GetFields()) {
            if ( !(*it)->GetData().IsFields() ) {
                continue;
            }
            ITERATE (CUser_field::TData::TFields, i, (**it).GetData().GetFields()) {
                const CUser_field& sub = **i;
                if (sub.GetLabel().GetStr() == "from") {
                    identical_to_start = NStr::IntToString(sub.GetData().GetInt());
                }
                if (sub.GetLabel().GetStr() == "to") {
                    identical_to_end   = NStr::IntToString(sub.GetData().GetInt());
                }
                if (sub.GetLabel().GetStr() == "accession" && identical_to_priority <= eIdenticalToPriority_Accn ) {
                    identical_to = sub.GetData().GetStr();
                    identical_to_priority = eIdenticalToPriority_Accn;
                }
                if (sub.GetLabel().GetStr() == "name" && identical_to_priority <= eIdenticalToPriority_Name ) {
                    identical_to = sub.GetData().GetStr();
                    identical_to_priority = eIdenticalToPriority_Name;
                }
                if (sub.GetLabel().GetStr() == "gi" && identical_to_priority <=  eIdenticalToPriority_Gi ) {
                    identical_to = "gi:" +
                        NStr::IntToString(sub.GetData().GetInt());
                    identical_to_priority = eIdenticalToPriority_Gi;
                }
            }
        }
    }

    string build_num = CGenomeAnnotComment::GetGenomeBuildNumber(bsh);

    ostringstream oss;
    if (status == eRefTrackStatus_Pipeline) {
        oss << (is_html ? kRefSeqInformationLink : kRefSeqInformation) << ":";
    } else {
        oss << status_str << ' '
            << (is_html ? kRefSeqLink : kRefSeq) << ":";
    }
    switch ( status ) {
    case eRefTrackStatus_Inferred:
        oss << " This record is predicted by genome sequence analysis and is "
            << "not yet supported by experimental evidence.";
        break;
    case eRefTrackStatus_Pipeline:
        if( eGenomeBuildComment == eGenomeBuildComment_Yes ) {
            if ( !build_num.empty() ) {
                oss << " Features on this sequence have been produced for build "
                    << build_num << " of the NCBI's genome annotation"
                    << " [see ";
                if (is_html) {
                    oss << "<a href=\"" << strDocLink << "\">" ;
                }
                oss << "documentation";
                if (is_html) {
                    oss << "</a>";
                }
                oss << "].";
            } else {
                oss << " NCBI contigs are derived from assembled genomic sequence data.~"
                    << "Also see:~"
                    << "    Documentation of NCBI's Annotation Process ";
            }
        }
        break;
    case eRefTrackStatus_Provisional:
        if (collaborator.empty()) {
            oss << " This record has not yet been subject to final NCBI review.";
        } else {
            oss << " This record is based on preliminary "
                "annotation provided by " << collaborator << '.';
        }
        break;
    case eRefTrackStatus_Predicted:
        oss << " This record has not been reviewed and the function is unknown.";
        break;
    case eRefTrackStatus_Validated:
        oss << " This record has undergone validation or preliminary review.";
        break;
    case eRefTrackStatus_Reviewed:
        oss << " This record has been curated by "
            << (collaborator.empty() ? "NCBI staff" : collaborator) << '.';
        break;
    case eRefTrackStatus_Model:
        oss << " This record is predicted by automated computational analysis.";
        break;
    case eRefTrackStatus_WGS:
        oss << " This record is provided to represent a collection of "
            << "whole genome shotgun sequences.";
        break;
    case eRefTrackStatus_TSA:
        oss << " This record is provided to represent a collection of "
            << "transcriptome shotgun assembly sequences.";
        break;
    default:
        break;
    }

    if ( status != eRefTrackStatus_Reviewed  &&
         status != eRefTrackStatus_Provisional  &&
         !collaborator.empty() ) {
        oss << " This record has been curated by " << collaborator << '.';
    }

    if ( !source.empty() ) {
        oss << " This record is derived from an annotated genomic sequence ("
            << source << ").";
    }

    if ( !identical_to.empty() ) {
        oss << " The reference sequence is identical to ";
        const bool add_link = (is_html && identical_to_priority != eIdenticalToPriority_Name);
        if (add_link) {
            ctx.Config().GetHTMLFormatter().FormatGeneralId(oss, identical_to);
        }
        else {
            oss << identical_to;
        }

        if( ! identical_to_start.empty() && ! identical_to_end.empty() ) {
            oss << " (range: " << identical_to_start << "-" <<
                identical_to_end << ")";
        }
        oss << ".";
    }

    {{
         /// add our assembly info
         string s;
         s_GetAssemblyInfo(ctx, s, uo);
         oss << s;
     }}

    const static string kRefSeqGeneLink = "<a href=\"https://www.ncbi.nlm.nih.gov/refseq/rsg/\">RefSeqGene</a>";
    const static string kRefSeqGene = "RefSeqGene";

    /// check for a concomitant RefSeqGene item
    for (CSeqdesc_CI desc_it(bsh, CSeqdesc::e_User);
         desc_it;  ++desc_it) {
        const CUser_object& obj = desc_it->GetUser();
        if (obj.IsSetType()  &&  obj.GetType().IsStr()  &&
            obj.GetType().GetStr() == "RefSeqGene") {
            CConstRef<CUser_field> f = obj.GetFieldRef("Status");
            if (f  &&  f->GetData().IsStr()) {
                const string& status1 = f->GetData().GetStr();
                if (status1 == "Reference Standard") {
                    oss << "~This sequence is a reference standard in the "
                        << (is_html ? kRefSeqGeneLink : kRefSeqGene)
                        << " project.";
                }
            }
        }
    }

    return oss.str();
}

// LCOV_EXCL_START
string CCommentItem::GetStringForRefSeqGenome(const CUser_object& uo)
{
    if ( ! FIELD_IS_SET_AND_IS(uo, Type, Str)  ||
         uo.GetType().GetStr() != "RefSeqGenome")
    {
        return kEmptyStr;
    }

    // this holds the value we return if no issues arise
    ostringstream result_oss;

    const static string kRefSeqCat = "RefSeq Category";

    // get category name
    result_oss << kRefSeqCat << ": ";
    CConstRef<CUser_field> pCategoryField = uo.GetFieldRef(kRefSeqCat);
    if( pCategoryField &&
        FIELD_IS_SET_AND_IS(*pCategoryField, Data, Str) )
    {
        const string & sCategory = pCategoryField->GetData().GetStr();
        result_oss << sCategory << '\n';
    } else {
        result_oss << "(?UNKNOWN?)" << '\n';
    }

    // get details field
    CConstRef<CUser_field> pDetailsField = uo.GetFieldRef("Details");

    CUser_field::TMapFieldNameToRef mapFieldNameToRef;
    if( pDetailsField ) {
        pDetailsField->GetFieldsMap(mapFieldNameToRef,
            CUser_field::fFieldMapFlags_ExcludeThis);

        const static char * arrFieldNames[] = {
            "CALC", "CCA", "CLI", "COM", "FGS", "MOD", "PHY", "PRT", "QfO", "TYS", "UPR"
        };

        ITERATE_0_IDX(field_idx, ArraySize(arrFieldNames) ) {
            const CTempString sFieldName( arrFieldNames[field_idx] );
            CUser_field::SFieldNameChain field_name;
            field_name += sFieldName;

            CUser_field::TMapFieldNameToRef::const_iterator find_iter =
                mapFieldNameToRef.find(field_name);
            if( find_iter == mapFieldNameToRef.end() ) {
                // not found
                continue;
            }

            if( ! FIELD_IS_SET_AND_IS(*find_iter->second, Data, Str) ) {
                // only Str fields are supported at this time
                continue;
            }

            // might need to pad
            if( sFieldName.length() < kRefSeqCat.length() ) {
                result_oss << string(
                    (kRefSeqCat.length() - sFieldName.length()), ' ');
            }

            result_oss << sFieldName << ": "
                       << find_iter->second->GetData().GetStr() << '\n';
        }
    }

    return result_oss.str();
}
// LCOV_EXCL_STOP


string CCommentItem::GetStringForWGS(CBioseqContext& ctx)
{
    static const string default_str = "?";

    if (!ctx.IsWGSMaster()) {
        return kEmptyStr;
    }

    const string& wgsaccn = ctx.GetWGSMasterAccn();
    const string& wgsname = ctx.GetWGSMasterName();

    if (NStr::IsBlank(wgsaccn)  ||  NStr::IsBlank(wgsname)) {
        return kEmptyStr;
    }

    const string* taxname = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& src = it->GetSource();
        if (src.IsSetOrg()  &&  src.GetOrg().IsSetTaxname()  &&
            !NStr::IsBlank(src.GetOrg().GetTaxname()) ) {
            taxname = &(src.GetOrg().GetTaxname());
        }
    }

    const string* first = &default_str, *last = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()  &&
            NStr::EqualNocase(uo.GetType().GetStr(), "WGSProjects")) {
            if (uo.HasField("WGS_accession_first")) {
                const CUser_field& uf = uo.GetField("WGS_accession_first");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr()) ) {
                    first = &(uf.GetData().GetStr());
                }
            }
            if (uo.HasField("WGS_accession_last")) {
                const CUser_field& uf = uo.GetField("WGS_accession_last");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr())) {
                    last = &(uf.GetData().GetStr());
                }
            }
        }
    }

    SIZE_TYPE len = wgsname.find_first_of("0123456789");
    string version = wgsname.substr(len, 2);
    /*
    string version = (wgsname.length() == 15 || NStr::StartsWith(wgsname, "NZ_")) ?
        wgsname.substr(7, 2) : wgsname.substr(4, 2);
    */

    ostringstream text;
    text << "The " << *taxname
         << " whole genome shotgun (WGS) project has the project accession "
         << wgsaccn << ".  This version of the project (" << version
         << ") has the accession number " << wgsname << ",";
    if (*first != *last) {
        text << " and consists of sequences " << *first << "-" << *last << ".";
    } else {
        text << " and consists of sequence " << *first << ".";
    }

    return text.str();
}

string CCommentItem::GetStringForTSA(CBioseqContext& ctx)
{
    static const string default_str = "?";

    if (!ctx.IsTSAMaster()) {
        return kEmptyStr;
    }

    const string& tsaaccn = ctx.GetTSAMasterAccn();
    const string& tsaname = ctx.GetTSAMasterName();

    if (NStr::IsBlank(tsaaccn)  ||  NStr::IsBlank(tsaname)) {
        return kEmptyStr;
    }

    const string* taxname = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& src = it->GetSource();
        if (src.IsSetOrg()  &&  src.GetOrg().IsSetTaxname()  &&
            !NStr::IsBlank(src.GetOrg().GetTaxname()) ) {
            taxname = &(src.GetOrg().GetTaxname());
        }
    }

    const string* first = &default_str, *last = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()  &&
            ( NStr::EqualNocase(uo.GetType().GetStr(), "TSA-mRNA-List") ||
              NStr::EqualNocase(uo.GetType().GetStr(), "TSA-RNA-List") ) )
        {
            if (uo.HasField("Accession_first")) {
                const CUser_field& uf = uo.GetField("Accession_first");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr()) ) {
                    first = &(uf.GetData().GetStr());
                }
            } else if (uo.HasField("TSA_accession_first")) {
                const CUser_field& uf = uo.GetField("TSA_accession_first");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr()) ) {
                    first = &(uf.GetData().GetStr());
                }
            }
            if (uo.HasField("Accession_last")) {
                const CUser_field& uf = uo.GetField("Accession_last");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr())) {
                    last = &(uf.GetData().GetStr());
                }
            } else if (uo.HasField("TSA_accession_last")) {
                const CUser_field& uf = uo.GetField("TSA_accession_last");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr())) {
                    last = &(uf.GetData().GetStr());
                }
            }
        }
    }

    SIZE_TYPE len = tsaname.find_first_of("0123456789");
    string version = tsaname.substr(len, 2);

    ostringstream text;
    text << "The " << *taxname
         << " transcriptome shotgun assembly (TSA) project has the project accession "
         << tsaaccn << ".  This version of the project (" << version
         << ") has the accession number " << tsaname << ",";
    if (*first != *last) {
        text << " and consists of sequences " << *first << "-" << *last << ".";
    } else {
        text << " and consists of sequence " << *first << ".";
    }

    return text.str();
}

string CCommentItem::GetStringForTLS(CBioseqContext& ctx)
{
    static const string default_str = "?";

    if (!ctx.IsTLSMaster()) {
        return kEmptyStr;
    }

    const string& tlsaccn = ctx.GetTLSMasterAccn();
    const string& tlsname = ctx.GetTLSMasterName();

    if (NStr::IsBlank(tlsaccn)  ||  NStr::IsBlank(tlsname)) {
        return kEmptyStr;
    }

    const string* taxname = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& src = it->GetSource();
        if (src.IsSetOrg()  &&  src.GetOrg().IsSetTaxname()  &&
            !NStr::IsBlank(src.GetOrg().GetTaxname()) ) {
            taxname = &(src.GetOrg().GetTaxname());
        }
    }

    const string* first = &default_str, *last = &default_str;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()  &&
            ( NStr::EqualNocase(uo.GetType().GetStr(), "TLSProjects") ) )
        {
            if (uo.HasField("TLS_accession_first")) {
                const CUser_field& uf = uo.GetField("TLS_accession_first");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr()) ) {
                    first = &(uf.GetData().GetStr());
                }
            }
            if (uo.HasField("TLS_accession_last")) {
                const CUser_field& uf = uo.GetField("TLS_accession_last");
                if (uf.IsSetData()  &&  uf.GetData().IsStr()  &&
                    !NStr::IsBlank(uf.GetData().GetStr())) {
                    last = &(uf.GetData().GetStr());
                }
            }
        }
    }

    SIZE_TYPE len = tlsname.find_first_of("0123456789");
    string version = tlsname.substr(len, 2);

    ostringstream text;
    text << "The " << *taxname
         << " targeted locus study (TLS) project has the project accession "
         << tlsaccn << ".  This version of the project (" << version
         << ") has the accession number " << tlsname << ",";
    if (*first != *last) {
        text << " and consists of sequences " << *first << "-" << *last << ".";
    } else {
        text << " and consists of sequence " << *first << ".";
    }

    return text.str();
}

string CCommentItem::GetStringForMolinfo(const CMolInfo& mi, CBioseqContext& ctx)
{
    _ASSERT(mi.CanGetCompleteness());

    bool is_prot = ctx.IsProt();

    switch ( mi.GetCompleteness() ) {
    case CMolInfo::eCompleteness_complete:
        return "COMPLETENESS: full length";

    case CMolInfo::eCompleteness_partial:
        return "COMPLETENESS: not full length";

    case CMolInfo::eCompleteness_no_left:
        return (is_prot ? "COMPLETENESS: incomplete on the amino end" :
                          "COMPLETENESS: incomplete on the 5' end");

    case CMolInfo::eCompleteness_no_right:
        return (is_prot ? "COMPLETENESS: incomplete on the carboxy end" :
                          "COMPLETENESS: incomplete on the 3' end");

    case CMolInfo::eCompleteness_no_ends:
        return "COMPLETENESS: incomplete on both ends";

    case CMolInfo::eCompleteness_has_left:
        return (is_prot ? "COMPLETENESS: complete on the amino end" :
                          "COMPLETENESS: complete on the 5' end");

    case CMolInfo::eCompleteness_has_right:
        return (is_prot ? "COMPLETENESS: complete on the carboxy end" :
                          "COMPLETENESS: complete on the 3' end");

    default:
        return "COMPLETENESS: unknown";
    }

    return kEmptyStr;
}


string CCommentItem::GetStringForUnordered(CBioseqContext& ctx)
{
    SDeltaSeqSummary summary;
    if (ctx.IsDelta()) {
        GetDeltaSeqSummary(ctx.GetHandle(), summary);
    }

    ostringstream text;

    text << "* NOTE: This is a partial genome representation.";
    if ( summary.num_gaps > 0 ) {
        text << " It currently~* consists of " << (summary.num_gaps + 1) << " contigs. The true order of the pieces~"
             << "* is not known and their order in this sequence record is~"
             << "* arbitrary. Gaps between the contigs are represented as~"
             << "* runs of N, but the exact sizes of the gaps are unknown.";
    }
    text << "~";

    string comment = text.str();
    ConvertQuotes(comment);
    ncbi::objects::AddPeriod(comment);

    return comment;
}


string CCommentItem::GetStringForHTGS(CBioseqContext& ctx)
{
    SDeltaSeqSummary summary;
    if (ctx.IsDelta()) {
        GetDeltaSeqSummary(ctx.GetHandle(), summary);
    }

    CMolInfo::TTech tech = ctx.GetTech();

    ostringstream text;

    if ( tech == CMolInfo::eTech_htgs_0 ) {
        if ( summary.num_segs > 0 ) {
            text << "* NOTE: This record contains " << (summary.num_gaps + 1) << " individual~"
                 << "* sequencing reads that have not been assembled into~"
                 << "* contigs. Runs of N are used to separate the reads~"
                 << "* and the order in which they appear is completely~"
                 << "* arbitrary. Low-pass sequence sampling is useful for~"
                 << "* identifying clones that may be gene-rich and allows~"
                 << "* overlap relationships among clones to be deduced.~"
                 << "* However, it should not be assumed that this clone~"
                 << "* will be sequenced to completion. In the event that~"
                 << "* the record is updated, the accession number will~"
                 << "* be preserved.";
        }
        text << "~";
        text << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_1 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~"
                 << "* consists of " << (summary.num_gaps + 1) << " contigs. The true order of the pieces~"
                 << "* is not known and their order in this sequence record is~"
                 << "* arbitrary. Gaps between the contigs are represented as~"
                 << "* runs of N, but the exact sizes of the gaps are unknown.";
        }
        text << "~* This record will be updated with the finished sequence~"
             << "* as soon as it is available and the accession number will~"
             << "* be preserved."
             << "~"
             << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_2 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~* consists of " << (summary.num_gaps + 1)
                 << " contigs. Gaps between the contigs~"
                 << "* are represented as runs of N. The order of the pieces~"
                 << "* is believed to be correct as given, however the sizes~"
                 << "* of the gaps between them are based on estimates that have~"
                 << "* provided by the submitter.";
        }
        text << "~* This sequence will be replaced~"
             << "* by the finished sequence as soon as it is available and~"
             << "* the accession number will be preserved."
             << "~"
             << summary.text;
    } else if ( !GetTechString(tech).empty() ) {
        text << "Method: " << GetTechString(tech) << ".";
    }

    string comment = text.str();
    ConvertQuotes(comment);
    ncbi::objects::AddPeriod(comment);

    return comment;
}

string CCommentItem::GetStringForModelEvidance(const CBioseqContext& ctx, const SModelEvidance& me)
{
    const bool bHtml = ctx.Config().DoHTML();

    const string *refseq = (bHtml ? &kRefSeqLink : &kRefSeq);

    ostringstream text;

    string me_name;
    ctx.Config().GetHTMLFormatter().FormatModelEvidence(me_name, me);

    text << "MODEL " << *refseq << ":  " << "This record is predicted by "
         << "automated computational analysis. This record is derived from "
         << "a genomic sequence (" << me_name << ")";

    if ( !me.assembly.empty() ) {
        int num_assm = (int) me.assembly.size();
        text << " and transcript sequence";
        if (num_assm > 1) {
            text << "s";
        }
        text << " (";
        int count = 0;
        string prefix;
        FOR_EACH_STRING_IN_LIST (str, me.assembly) {
            string tr_name;
            ctx.Config().GetHTMLFormatter().FormatTranscript(tr_name, *str);
            text << prefix << tr_name;
            count++;
            if (num_assm == count + 1) {
                prefix = " and ";
            } else {
                prefix = ", ";
            }
        }
        text << ")";
    }

    if ( !me.method.empty() ) {
        text << " annotated using gene prediction method: " << me.method;
    }

    if ( me.mrnaEv  ||  me.estEv ) {
        text << ", supported by ";
        if ( me.mrnaEv  &&  me.estEv ) {
            text << "mRNA and EST ";
        } else if ( me.mrnaEv ) {
            text << "mRNA ";
        } else {
            text << "EST ";
        }
        // !!! for html we need much more !!!
        text << "evidence";
    }

    const char *documentation_str = ( bHtml ?
        "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_euk/process/\">Documentation</a>" :
        "Documentation" );

    text << ".~Also see:~"
        << "    " << documentation_str << " of NCBI's Annotation Process    ";

    return text.str();
}

static bool s_GetEncodeValues
(string& chromosome,
 string& assembly_date,
 string& ncbi_annotation,
 CBioseqContext& ctx)
{
    _ASSERT(ctx.IsEncode());

    const CUser_object& uo = ctx.GetEncode();
    if (uo.HasField("AssemblyDate")) {
        const CUser_field& ad = uo.GetField("AssemblyDate");
        if (ad.IsSetData()  &&  ad.GetData().IsStr()) {
            assembly_date = ad.GetData().GetStr();
        }
    } else {
        return false;
    }
    if (uo.HasField("NcbiAnnotation")) {
        const CUser_field& na = uo.GetField("NcbiAnnotation");
        if (na.IsSetData()  &&  na.GetData().IsStr()) {
            ncbi_annotation = na.GetData().GetStr();
        }
    } else {
        return false;
    }

    const string* name = nullptr;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& bio = it->GetSource();
        ITERATE (CBioSource::TSubtype, st, bio.GetSubtype()) {
            if ((*st)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                name = &(*st)->GetName();
                break;
            }
        }
    }
    if (name) {
        chromosome = *name;
    } else {
        return false;
    }

    if (NStr::IsBlank(chromosome)) {
        chromosome = "?";
    }
    if (NStr::IsBlank(assembly_date)) {
        assembly_date = "?";
    }
    if (NStr::IsBlank(ncbi_annotation)) {
        ncbi_annotation = "?";
    }
    return true;
}


string CCommentItem::GetStringForEncode(CBioseqContext& ctx)
{
    const static string kEncodeProjLink = "https://www.nhgri.nih.gov/10005107";

    const bool bHtml = ctx.Config().DoHTML();

    if (!ctx.IsEncode()) {
        return kEmptyStr;
    }

    ostringstream str;
    str << "REFSEQ:  This record was provided by the ";
    if( bHtml ) {
        str << "<a href=\"" << kEncodeProjLink << "\">";
    }
    str << "ENCODE";
    if( bHtml ) {
        str << "</a>";
    }
    str << " project.";

    string chromosome, assembly_date, ncbi_annotation;
    if (s_GetEncodeValues(chromosome, assembly_date, ncbi_annotation, ctx)) {
        str << "  It is defined by coordinates on the sequence of chromosome "
            << chromosome << " from the " << assembly_date
            << " assembly of the human genome (NCBI build " << ncbi_annotation
            << ").";
    }
    return str.str();
}

// static
string CCommentItem::GetStringForAuthorizedAccess(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const string & sAuthorizedAccess = ctx.GetAuthorizedAccess();
    if( sAuthorizedAccess.empty() ) {
        return kEmptyStr;
    }

    ostringstream str;

    str << "These data are available through the dbGaP authorized access system. ";
    if( bHtml ) {
        str << "<a href=\""
            << "https://dbgap.ncbi.nlm.nih.gov/aa/wga.cgi?adddataset="
            << sAuthorizedAccess << "&page=login\">";
        str << "Request access";
        str << "</a>";
        str << " to Study ";
        str << "<a href=\""
            << "https://www.ncbi.nlm.nih.gov/projects/gap/cgi-bin/study.cgi?study_id="
            << sAuthorizedAccess << "\">";
        str << sAuthorizedAccess;
        str << "</a>";
    } else {
        str << "Request access to Study ";
        str << sAuthorizedAccess;
    }
    str << "."; // always needs a period

    return str.str();
}

string CCommentItem::GetStringForOpticalMap(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const CPacked_seqpnt* pOpticalMapPoints = ctx.GetOpticalMapPoints();
    if( ! pOpticalMapPoints ||
        RAW_FIELD_IS_EMPTY_OR_UNSET(*pOpticalMapPoints, Points) )
    {
        return kEmptyStr;
    }

    const string & sFiletrackURL = ctx.GetFiletrackURL();

    const bool bIsCircular = FIELD_EQUALS( ctx.GetHandle(), Inst_Topology,
        CSeq_inst::eTopology_circular );
    const TSeqPos uBioseqLength =
        GET_FIELD_OR_DEFAULT(ctx.GetHandle(), Inst_Length, 0);

    ostringstream str;

    // vecOfPoints elements are 1-based
    const CPacked_seqpnt::TPoints & vecOfPoints =
        pOpticalMapPoints->GetPoints();
    _ASSERT( ! vecOfPoints.empty() );

    str << "This ";
    if( bHtml && ! sFiletrackURL.empty() ) {
        str << "<a href=\"" << sFiletrackURL << "\">";
    }
    str << "map";
    if( bHtml && ! sFiletrackURL.empty() ) {
        str << "</a>";
    }
    str << " has ";

    size_t uNumFrags = pOpticalMapPoints->GetPoints().size();
    if( ! bIsCircular )
    {
        // non-circular has an extra fragment because the
        // last fragment does NOT wrap around to continue on
        // the beginning of the bioseq.
        if (uNumFrags > 1 && vecOfPoints[uNumFrags-1] < uBioseqLength - 1) {
            ++uNumFrags;
        }
    }
    str << uNumFrags;
    str << " piece" << ( (uNumFrags > 1) ? "s" : "" ) << ":";

    // prevEndPos and thisEndPos are 1-based
    TSeqPos prevEndPos = 1;
    TSeqPos thisEndPos = vecOfPoints[0] + 1;

    // non-circular's first fragment is from 0 to the first rsite
    if ( ! bIsCircular ) {
        x_GetStringForOpticalMap_WriteFragmentLine(
            str, prevEndPos, thisEndPos, uBioseqLength,
            eFragmentType_Normal );
    }
    prevEndPos = thisEndPos + 1;

    // regular fragments
    for( size_t idx = 1; idx < vecOfPoints.size(); ++idx ) {
        thisEndPos = vecOfPoints[idx] + 1;
        x_GetStringForOpticalMap_WriteFragmentLine(
            str, prevEndPos, thisEndPos, uBioseqLength,
            eFragmentType_Normal );
        prevEndPos = thisEndPos + 1;
    }

    // The last fragment for circular wraps around to the first rsite,
    // but for non-circular it ends at the end of the bioseq
    thisEndPos = ( bIsCircular ? vecOfPoints[0] + 1 : uBioseqLength );
    if ( bIsCircular || prevEndPos < uBioseqLength - 1 ) {
        x_GetStringForOpticalMap_WriteFragmentLine(
                str, prevEndPos, thisEndPos, uBioseqLength,
                ( bIsCircular ?
                  eFragmentType_WrapAround :
                  eFragmentType_Normal ) );
    }

    return str.str();
}

string CCommentItem::GetStringForBaseMod(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const vector< string > & sBasemodURLs = ctx.GetBasemodURLs();
    int numBases = (int) sBasemodURLs.size();

    ostringstream str;

    if ( numBases < 1 ) {
        return {};
    }

    if ( numBases == 1 ) {
        str << "This genome has a ";
        if( bHtml ) {
            FOR_EACH_STRING_IN_VECTOR (itr, sBasemodURLs) {
                string url = *itr;
                if ( ! url.empty() ) {
                    NStr::ReplaceInPlace( url, "\"", "" );
                    str << "<a href=\"" << url << "\">" << "base modification file" << "</a>";
                }
            }
        } else {
            str << "base modification file";
        }
        str << " available.";
    } else {
        str << "There are ";
        str << numBases;
        str << " base modification files";
        if( bHtml ) {
            string pfx = " (";
            string sfx = "";
            int j = 0;
            FOR_EACH_STRING_IN_VECTOR (itr, sBasemodURLs) {
                string url = *itr;
                if ( ! url.empty() ) {
                    NStr::ReplaceInPlace( url, "\"", "" );
                    j++;
                    str << pfx << "<a href=\"" << url << "\">" << j << "</a>";
                    if ( numBases == 2 ) {
                        pfx = " and ";
                    } else if ( j == numBases - 1 ) {
                        pfx = ", and ";
                    } else {
                        pfx = ", ";
                    }
                    sfx = ")";
                }
            }
            str << sfx;
        }
        str << " available for this genome.";
    }

    return str.str();
}

string CCommentItem::GetStringForUnique(CBioseqContext& ctx)
{
    if( ! ctx.IsRSUniqueProt() ) {
        return kEmptyStr;
    }

    ostringstream str;

    // this will be more complex if HTML links ever need to be added
    // or we have to cover nucs or whatever

    str << "REFSEQ: This record represents a single, non-redundant, protein "
        << "sequence which may be annotated on many different RefSeq "
        << "genomes from the same, or different, species.";

    return str.str();
}

/***************************************************************************/
/*                                 PROTECTED                               */
/***************************************************************************/


void CCommentItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CObject* obj = GetObject();
    if (! obj) {
        return;
    }
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(obj);
    if (desc) {
        x_GatherDescInfo(*desc, ctx);
    } else {
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(obj);
        if (feat) {
            x_GatherFeatInfo(*feat, ctx);
        } else {
            const CUser_object * userObject = dynamic_cast<const CUser_object*>(obj);
            if (userObject) {
                x_GatherUserObjInfo(*userObject);
            }
        }
    }
}

// returns the data_str, but wrapped in appropriate <a href...>...</a> if applicable
static
string s_HtmlizeStructuredCommentData( const bool is_html, const string &label_str, const string &data_str,
                                       const char* provider, const char* pipeline, const char* status, bool has_name, const char* organism,
                                       const char* source, const char* category, const char* accession )
{
    if( ! is_html ) {
        return data_str;
    }

    ostringstream result;
    if( label_str == "GOLD Stamp ID" && NStr::StartsWith(data_str, "Gi") ) {
        result << "<a href=\"http://genomesonline.org/cgi-bin/GOLD/bin/GOLDCards.cgi?goldstamp=" << data_str
               << "\">" << data_str << "</a>";
        return result.str();
    }
    if ( label_str == "Annotation Software Version") {
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_euk/release_notes/#version"
               << data_str
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Annotation Name") &&
        ( NStr::Equal (provider, "NCBI") || NStr::Equal (provider, "NCBI RefSeq") ) &&
        NStr::FindNoCase (pipeline, "NCBI Eukaryotic Genome Annotation Pipeline") != NPOS &&
        NStr::FindNoCase(pipeline, "EGAPx") == NPOS) {
        string fst;
        string snd;
        if (NStr::Find(data_str, "Updated Annotation Release") != NPOS) {
            NStr::Replace( data_str, " Updated Annotation Release ", "/", fst );
        } else {
            NStr::Replace( data_str, " Annotation Release ", "/", fst );
        }
        NStr::Replace( fst, " ", "_", snd );
        if (NStr::Equal (organism, "")) {
            result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_euk/"
                   << snd
                   << "\">" << data_str << "</a>";
        } else {
            result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_euk/"
                   << organism << "/"
                   << snd
                   << "\">" << data_str << "</a>";
        }
        return result.str();
    } else if ( NStr::Equal (label_str, "Annotation Version") && ( NStr::Equal (provider, "NCBI") || NStr::Equal (provider, "NCBI RefSeq") ) && NStr::Equal (status, "Full annotation") && (! has_name) ) {
        string fst;
        string snd;
        NStr::Replace( data_str, " Annotation Release ", "/", fst );
        NStr::Replace( fst, " ", "_", snd );
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_euk/"
               << snd
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Source Identifier") && NStr::Equal (source, "EMBL-EBI") ) {
        string accn = data_str;
        size_t pos = NStr::Find(accn, ".");
        if (pos > 0) {
            accn.erase(pos);
        }
        result << "<a href=\"https://www.ebi.ac.uk/interpro/entry/pfam/"
               << accn
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Evidence Accession") && NStr::Equal (source, "NCBI SPARCLE") ) {
        string fst;
        string snd;
        NStr::Replace( data_str, "Domain architecture ID ", "", fst );
        NStr::Replace( fst, " ", "_", snd );
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/Structure/sparcle/archview.html?archid="
               << snd
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Evidence Category") &&
                NStr::Equal (data_str, "Antimicrobial Resistance Allele") &&
                NStr::Equal (source, "Bacterial Antimicrobial Resistance Reference Gene Database") ) {
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/bioproject/"
               << "313047"
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Evidence Accession") &&
                NStr::Equal (source, "Bacterial Antimicrobial Resistance Reference Gene Database") ) {
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/nuccore/"
               << data_str
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Evidence Accession") && NStr::Equal (category, "HMM") ) {
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_prok/evidence/"
               << accession
               << "\">" << data_str << "</a>";
        return result.str();
    } else if ( NStr::Equal (label_str, "Evidence Accession") && NStr::Equal (category, "BlastRule") ) {
        result << "<a href=\"https://www.ncbi.nlm.nih.gov/genome/annotation_prok/evidence/"
               << accession
               << "\">" << data_str << "</a>";
        return result.str();
    } else {
        // normalize case: nothing to do
        return data_str;
    }
}

// turns data into comment lines (not line-wrapped)
// result in out_lines
// out_prefix_len holds the length of the part up to the space after the double-colon
static
void s_GetStrForStructuredComment(
    const CUser_object::TData &data,
    list<string> &out_lines,
    int &out_prefix_len,
    const bool is_first,
    const bool is_html )
{
    static const int kFieldLenThreshold = 45;

    // default prefix and suffix
    const char* prefix = "##Metadata-START##";
    const char* suffix = "##Metadata-END##";
    const char* provider = "";
    const char* pipeline = "";
    const char* status = "";
    const char* source = "";
    const char* category = "";
    const char* organism = "";
    string accession;
    bool has_name = false;

    bool fieldOverThreshold = false;

    // First, figure out the longest label so we know how to format it
    // (and set the prefix and suffix while we're at it)
    string::size_type longest_label_len = 1;
    ITERATE( CUser_object::TData, it_for_len, data ) {
        if( (*it_for_len)->GetLabel().IsStr() &&
                (*it_for_len)->GetData().IsStr() && ! (*it_for_len)->GetData().GetStr().empty() ) {
            const string &label = (*it_for_len)->GetLabel().GetStr();

            if( label == "StructuredCommentPrefix" ) {
                prefix = (*it_for_len)->GetData().GetStr().c_str();
            } else if( label == "StructuredCommentSuffix" ) {
                suffix = (*it_for_len)->GetData().GetStr().c_str();
            } else {
                if ( label == "Annotation Provider" ) {
                    provider = (*it_for_len)->GetData().GetStr().c_str();
                } else if ( label == "Annotation Pipeline" ) {
                    pipeline = (*it_for_len)->GetData().GetStr().c_str();
                } else if ( label == "Annotation Status" ) {
                    status = (*it_for_len)->GetData().GetStr().c_str();
                } else if ( label == "Annotation Name" ) {
                    has_name = true;
                } else if ( label == "URL Organism" ) {
                    organism = (*it_for_len)->GetData().GetStr().c_str();
                } else if (NStr::EqualNocase(prefix, "##Evidence-For-Name-Assignment-START##")) {
                    if ( label == "Evidence Source" ) {
                        source = (*it_for_len)->GetData().GetStr().c_str();
                    }
                    if ( label == "Evidence Category" ) {
                        category = (*it_for_len)->GetData().GetStr().c_str();
                    }
                    if ( label == "Evidence Accession" ) {
                        string accn = (*it_for_len)->GetData().GetStr();
                        string version;
                        NStr::SplitInTwo(accn, ".", accession, version);
                    }
                }
                const string::size_type label_len = label.length();
                if( (label_len > longest_label_len) && (label_len <= kFieldLenThreshold) ) {
                    longest_label_len = label_len;
                }
                if( label_len > kFieldLenThreshold ) {
                    fieldOverThreshold = true;
                }
            }
        }
    }
    out_prefix_len = (longest_label_len + 4); // "+4" because we add " :: " after the prefix

    /*
    if( ! is_first ) {
        out_lines.push_back( "\n" );
    }
    */
    out_lines.push_back( prefix );
    out_lines.back().append( "\n" );

    ITERATE( CUser_object::TData, it, data ) {

        // skip if no label
        if( ! (*it)->GetLabel().IsStr() || (*it)->GetLabel().GetStr().empty() ) {
            continue;
        }

        // skip if no data
        if( ! (*it)->GetData().IsStr() || (*it)->GetData().GetStr().empty() ) {
            continue;
        }

        // special fields are skipped
        if( (*it)->GetLabel().GetStr() == "StructuredCommentPrefix" ||
                (*it)->GetLabel().GetStr() == "StructuredCommentSuffix" ||
                (*it)->GetLabel().GetStr() == "Annotation Freeze" ||
                (*it)->GetLabel().GetStr() == "URL Organism" ) {
            continue;
        }

        // create the next line that we're going to set the contents of
        out_lines.push_back( (*it)->GetLabel().GetStr() );
        string &next_line = out_lines.back();

        // TODO: remove this if-statement once we move to C++ completely.  it just makes
        // formatting look like C even though C++'s formatting is superior
        // (example: JF320002).  We might even be able to remove the variable fieldOverThreshold
        // completely.
        if( ! fieldOverThreshold ) {
            next_line.resize( max( next_line.size(), longest_label_len), ' ' );
        }
        next_line.append( " :: " );
        next_line.append( s_HtmlizeStructuredCommentData( is_html, (*it)->GetLabel().GetStr(), (*it)->GetData().GetStr(),
                                                          provider, pipeline, status, has_name, organism, source, category, accession.c_str() ) );
        next_line.append( "\n" );

        ExpandTildes(next_line, eTilde_comment);
    }

    out_lines.push_back( suffix );
    out_lines.back().append( "\n" );
}

void CCommentItem::x_GatherDescInfo(const CSeqdesc& desc, CBioseqContext& ctx)
{
    // true for most desc infos
    EPeriod can_add_period = ePeriod_Add;

    string prefix, str, suffix;
    switch ( desc.Which() ) {
    case CSeqdesc::e_Comment:
        {{
            if (!NStr::IsBlank(desc.GetComment())) {
                str = desc.GetComment();
                TrimSpacesAndJunkFromEnds(str, true);
                ConvertQuotes(str);
                if( ! NStr::EndsWith(str, ".") && ! NStr::EndsWith(str, "/") && ! NStr::EndsWith(str, "~") ) {
                    str += '.';
                }
            }
        }}
        break;

    case CSeqdesc::e_Maploc:
        {{
            const CDbtag& dbtag = desc.GetMaploc();
            if ( dbtag.CanGetTag() ) {
                const CObject_id& oid = dbtag.GetTag();
                if ( oid.IsStr() ) {
                    prefix = "Map location: ";
                    str = oid.GetStr();
                    suffix = ".";
                } else if ( oid.IsId()  &&  dbtag.CanGetDb() ) {
                    prefix = "Map location: (Database ";
                    str = dbtag.GetDb();
                    suffix = "; id # " + NStr::IntToString(oid.GetId()) + ").";
                }
            }
        }}
        break;

    case CSeqdesc::e_Region:
        {{
            prefix = "Region: ";
            str = desc.GetRegion();
            NStr::ReplaceInPlace(str, "\"", "\'");
            ncbi::objects::AddPeriod(str);
        }}
        break;

    case CSeqdesc::e_Name:
        {{
            prefix = "Name: ";
            str = desc.GetName();
            ncbi::objects::AddPeriod(str);
        }}
        break;

    case CSeqdesc::e_User:
        {{
            const CSeqdesc_Base::TUser &userObject = desc.GetUser();

            // make sure the user object is really of type StructuredComment
            const CUser_object::TType &type = userObject.GetType();
            if( type.IsStr() && type.GetStr() == "StructuredComment" ) {
                s_GetStrForStructuredComment( userObject.GetData(),
                    m_Comment, m_CommentInternalIndent, IsFirst(), GetContext()->Config().DoHTML() );
                SetNeedPeriod( false );
                can_add_period = ePeriod_NoAdd;
                return; // special case because multiple lines
            }
        }}
        break;

    default:
        break;
    }

    if (str.empty()  ||  str == ".") {
        return;
    }
    x_SetCommentWithURLlinks(prefix, str, suffix, ctx, can_add_period);
}


void CCommentItem::x_GatherFeatInfo(const CSeq_feat& feat, CBioseqContext& ctx)
{
    if (!feat.GetData().IsComment()  ||
        !feat.CanGetComment()        ||
        NStr::IsBlank(feat.GetComment())) {
        return;
    }

    x_SetCommentWithURLlinks(kEmptyStr, feat.GetComment(), kEmptyStr, ctx, ePeriod_Add);
}

void CCommentItem::x_GatherUserObjInfo(const CUser_object& userObject )
{
    // make sure the user object is really of type StructuredComment
    const CUser_object::TType &type = userObject.GetType();
    if( type.IsStr() && type.GetStr() == "StructuredComment" ) {
        s_GetStrForStructuredComment( userObject.GetData(),
            m_Comment, m_CommentInternalIndent, IsFirst(), GetContext()->Config().DoHTML() );
        SetNeedPeriod( false );
    }
}


void CCommentItem::x_SetSkip(void)
{
    CFlatItem::x_SetSkip();
    // swap(m_First, sm_FirstComment);
}


void CCommentItem::x_SetComment(const string& comment)
{
    m_Comment.clear();
    m_Comment.push_back( comment );
    ExpandTildes(m_Comment.back(), eTilde_comment);
}


void CCommentItem::x_SetComment(const string& comment, CBioseqContext& ctx)
{
    m_Comment.clear();
    m_Comment.push_back( comment );
    if (! ctx.Config().IsFormatGBSeq() && ! ctx.Config().IsFormatINSDSeq()) {
        ExpandTildes(m_Comment.back(), eTilde_comment);
    }
}


void CCommentItem::x_SetCommentWithURLlinks
(const string& prefix,
 const string& str,
 const string& suffix,
 CBioseqContext& ctx,
 EPeriod can_add_period)
{
    // !!! test for html - find links within the comment string
    string comment = prefix;
    comment += str;
    comment += suffix;

    if (! ctx.Config().IsFormatGBSeq() && ! ctx.Config().IsFormatINSDSeq()) {
        ExpandTildes(comment, eTilde_comment);
    }
    if (NStr::IsBlank(comment)) {
        return;
    }

    if( can_add_period == ePeriod_Add ) {
        size_t pos = comment.find_last_not_of(" \n\t\r.~");
        if (pos != comment.length() - 1) {
            size_t period = comment.find_last_of('.');
            bool add_period = period > pos;
            if (add_period  &&  !NStr::EndsWith(str, "...")) {
                ncbi::objects::AddPeriod(comment);
            }
        }
    }

    ConvertQuotes( comment );

    m_Comment.clear();
    m_Comment.push_back( comment );
}

bool CCommentItem::x_IsCommentEmpty(void) const
{
    ITERATE(list<string>, it, m_Comment) {
        if( ! m_Comment.empty() ) {
            return false;
        }
    }
    return true;
}

// static
void CCommentItem::x_GetStringForOpticalMap_WriteFragmentLine(
    ostream & str, TSeqPos prevEndPos, TSeqPos thisEndPos,
    TSeqPos uBioseqLength, EFragmentType eFragmentType)
{
    str << '\n';
    str << "*  "
        << setw(7) << (prevEndPos)
        << ' '
        << setw(7) << (thisEndPos)
        << ": fragment of ";

    bool bLengthIsOkay = true; // until proven otherwise
    if( (eFragmentType == eFragmentType_Normal) &&
        (thisEndPos <= prevEndPos) )
    {
        bLengthIsOkay = false;
    } else if( (eFragmentType == eFragmentType_WrapAround) &&
        (thisEndPos >= prevEndPos) )
    {
        bLengthIsOkay = false;
    }

    if( ! bLengthIsOkay ) {
        str << "(ERROR: CANNOT CALCULATE LENGTH)";
    } else if( (thisEndPos > uBioseqLength) ||
        (prevEndPos > uBioseqLength) )
    {
        str << "(ERROR: FRAGMENT IS OUTSIDE BIOSEQ BOUNDS)";
    } else {
        if( eFragmentType == eFragmentType_Normal ) {
            str << (thisEndPos - prevEndPos + 1);
        } else {
            str << (uBioseqLength + thisEndPos - prevEndPos + 1);
        }
    }
    str << " bp in length";
}

/////////////////////////////////////////////////////////////////////////////
//
// Derived Classes

// --- CGenomeAnnotComment

CGenomeAnnotComment::CGenomeAnnotComment
(CBioseqContext& ctx,
 const string& build_num) :
    CCommentItem(ctx), m_GenomeBuildNumber(build_num)
{
    x_GatherInfo(ctx);
}


string CGenomeAnnotComment::GetGenomeBuildNumber(const CUser_object& uo)
{
    if ( uo.IsSetType()  &&  uo.GetType().IsStr()  &&
         uo.GetType().GetStr() == "GenomeBuild" ) {
        if ( uo.HasField("NcbiAnnotation") ) {
            string build_num;
            const CUser_field& uf = uo.GetField("NcbiAnnotation");
            if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                 !uf.GetData().GetStr().empty() ) {
                build_num = uf.GetData().GetStr();
            }

            if ( uo.HasField("NcbiVersion") ) {
                const CUser_field& uf_version = uo.GetField("NcbiVersion");
                if ( uf_version.CanGetData() && uf_version.GetData().IsStr()  &&
                     !uf_version.GetData().GetStr().empty() ) {
                    build_num += " version ";
                    build_num += uf_version.GetData().GetStr();
                }
            }
            return build_num;

        } else if ( uo.HasField("Annotation") ) {
            const CUser_field& uf = uo.GetField("Annotation");
            if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                 !uf.GetData().GetStr().empty() ) {
                static const string prefix = "NCBI build ";
                if ( NStr::StartsWith(uf.GetData().GetStr(), prefix) ) {
                    return uf.GetData().GetStr().substr(prefix.length());
                }
            }
        }
    }
    return kEmptyStr;
}


string CGenomeAnnotComment::GetGenomeBuildNumber(const CBioseq_Handle& bsh)
{
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        string s = GetGenomeBuildNumber(uo);
        if ( !s.empty() ) {
            return s;
        }
    }

    return kEmptyStr;
}

void CGenomeAnnotComment::x_GatherInfo(CBioseqContext& ctx)
{
    const bool bHtml = ctx.Config().DoHTML();

    const string *refseq = ( bHtml ? &kRefSeqLink : &kRefSeq );

    ostringstream text;

    text << "GENOME ANNOTATION " << *refseq << ": ";
    if ( ! m_GenomeBuildNumber.empty() ) {
         text << "Features on this sequence have been produced for build "
              << m_GenomeBuildNumber << " of the NCBI's genome annotation"
              << " [see ";
         if( bHtml ) {
             text << "<a href=\"" << strDocLink << "\">";
         }
         text << "documentation";
         if( bHtml ) {
             text << "</a>";
         }
         text << "].";
    } else {
        text << "NCBI contigs are derived from assembled genomic sequence data."
             << "~Also see:~"
             << "    Documentation of NCBI's Annotation Process ";
    }

    /// add our assembly info
    for (CSeqdesc_CI desc_it(ctx.GetHandle(), CSeqdesc::e_User);
         desc_it;  ++desc_it) {
        const CUser_object& uo = desc_it->GetUser();
        if ( !uo.IsSetType()  ||  !uo.GetType().IsStr()  ||
             uo.GetType().GetStr() != "RefGeneTracking") {
            continue;
        }

        string s;
        s_GetAssemblyInfo(ctx, s, uo);
        text << s;
        break;
    }

    string s = text.str();
    x_SetComment(s, ctx);
}


// --- CHistComment

CHistComment::CHistComment
(EType type,
 const CSeq_hist& hist,
 CBioseqContext& ctx) :
    CCommentItem(ctx), m_Type(type), m_Hist(&hist)
{
    x_GatherInfo(ctx);
    m_Hist.Reset();
}


string s_CreateHistCommentString
(const string& prefix,
 const string& suffix,
 const CSeq_hist_rec& hist,
 CBioseqContext& ctx)
{
    //if (!hist.CanGetDate()  ||  !hist.CanGetIds()) {
    //    return "???";
    //}

    string date;
    if (hist.IsSetDate()) {
        hist.GetDate().GetDate(&date, "%{%3N%|???%} %{%D%|??%}, %{%4Y%|????%}");
    }

    vector<TGi> gis;
    ITERATE (CSeq_hist_rec::TIds, id, hist.GetIds()) {
        if ( (*id)->IsGi() ) {
            gis.push_back((*id)->GetGi());
        }
    }

    ostringstream text;

    text << prefix << ((gis.size() > 1) ? " or before " : " ") << date
         << ' ' << suffix;

    if ( gis.empty() ) {
        text << " gi:?";
        return text.str();
    }

    for ( size_t count = 0; count < gis.size(); ++count ) {
        if ( count != 0 ) {
            text << ",";
        }
        string accn = GetAccessionForGi(gis[count], ctx.GetScope(), sequence::eWithAccessionVersion, sequence::eGetId_Best);
        text << " ";
        if (NStr::Find(accn, ".") != NPOS) {
             NcbiId(text, accn, ctx.Config().DoHTML());
        } else {
            text << "gi:";
            NcbiId(text, gis[count], ctx.Config().DoHTML());
        }
        /* was
        text << " gi:";
        ctx.Config().GetHTMLFormatter().FormatGeneralId(text, NStr::NumericToString(gis[count]));
        */
    }
    text << '.' << '\n';

    return text.str();
}

void CHistComment::x_GatherInfo(CBioseqContext& ctx)
{
    _ASSERT(m_Hist);

    switch ( m_Type ) {
    case eReplaced_by:
        if( ctx.IsWGSMaster() || ctx.IsTSAMaster() ) {
            x_SetComment(s_CreateHistCommentString(
                "[WARNING] On",
                "this project was updated. The new version is",
                m_Hist->GetReplaced_by(),
                ctx));
        } else {
            x_SetComment(s_CreateHistCommentString(
                "[WARNING] On",
                "this sequence was replaced by",
                m_Hist->GetReplaced_by(),
                ctx));
        }
        break;
    case eReplaces:
        x_SetComment(s_CreateHistCommentString(
            "On",
            "this sequence version replaced",
            m_Hist->GetReplaces(),
            ctx));
        break;
    }
}


// --- CGsdbComment

CGsdbComment::CGsdbComment(const CDbtag& dbtag, CBioseqContext& ctx) :
    CCommentItem(ctx), m_Dbtag(&dbtag)
{
    x_GatherInfo(ctx);
}


void CGsdbComment::x_GatherInfo(CBioseqContext&)
{
    if (m_Dbtag->IsSetTag()  &&  m_Dbtag->GetTag().IsId()) {
        string id = NStr::IntToString(m_Dbtag->GetTag().GetId());
        x_SetComment("GSDB:S:" + id);
    } else {
        x_SetSkip();
    }
}


// --- CLocalIdComment

CLocalIdComment::CLocalIdComment(const CObject_id& oid, CBioseqContext& ctx) :
    CCommentItem(ctx, false), m_Oid(&oid)
{
    x_GatherInfo(ctx);
}


static string s_GetOriginalID (CBioseqContext& ctx)

{
    const CBioseq_Handle& bsh = ctx.GetHandle();
    const CBioseq& seq = *bsh.GetCompleteBioseq();

    FOR_EACH_SEQDESC_ON_BIOSEQ (it, seq) {
        const CSeqdesc& desc = **it;
        if (! desc.IsUser()) continue;
        if (! desc.GetUser().IsSetType()) continue;
        const CUser_object& usr = desc.GetUser();
        const CObject_id& oi = usr.GetType();
        if (! oi.IsStr()) continue;
        const string& type = oi.GetStr();
        if (! NStr::EqualNocase(type, "OrginalID") && ! NStr::EqualNocase(type, "OriginalID")) continue;
        FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
            const CUser_field& fld = **uitr;
            if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                if (! NStr::EqualNocase(label_str, "LocalId")) continue;
                if (fld.IsSetData() && fld.GetData().IsStr()) {
                    return fld.GetData().GetStr();
                }
            }
        }
    }

    return "";
}


void CLocalIdComment::x_GatherInfo(CBioseqContext& ctx)
{
    ostringstream msg;

    string orig_id = s_GetOriginalID (ctx);
    if (!NStr::EqualNocase(orig_id, "")) {
        if ( orig_id.length() < 1000 ) {
            msg << "LocalID: " << orig_id;
        } else {
            msg << "LocalID string too large";
        }
    } else {
        switch ( m_Oid->Which() ) {
        case CObject_id::e_Id:
            msg << "LocalID: " << m_Oid->GetId();
            break;
        case CObject_id::e_Str:
            if ( m_Oid->GetStr().length() < 1000 ) {
                msg << "LocalID: " << m_Oid->GetStr();
            } else {
                msg << "LocalID string too large";
            }
            break;
        default:
            break;
        }
    }

    x_SetComment(msg.str());
}

// --- CFileIdComment

CFileIdComment::CFileIdComment(const CObject_id& oid, CBioseqContext& ctx) :
    CCommentItem(ctx, false), m_Oid(&oid)
{
    x_GatherInfo(ctx);
}


void CFileIdComment::x_GatherInfo(CBioseqContext&)
{
    ostringstream msg;

    switch ( m_Oid->Which() ) {
    case CObject_id::e_Id:
        msg << "FileID: " << m_Oid->GetId();
        break;
    case CObject_id::e_Str:
        if ( m_Oid->GetStr().length() < 1000 ) {
            msg << "FileID: " << m_Oid->GetStr();
        } else {
            msg << "FileID string too large";
        }
        break;
    default:
        break;
    }
    x_SetComment(msg.str());
}


END_SCOPE(objects)
END_NCBI_SCOPE
