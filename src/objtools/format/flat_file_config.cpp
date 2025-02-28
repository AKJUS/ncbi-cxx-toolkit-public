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
* Author:  Mati Shomrat
*
* File Description:
*   Configuration class for flat-file generator
*
*/
#include <ncbi_pch.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <util/static_map.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/flat_expt.hpp>

#include <objmgr/util/objutil.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CFlatFileConfig::CGenbankBlockCallback::EBioseqSkip
CFlatFileConfig::CGenbankBlockCallback::notify_bioseq(
    CBioseqContext& ctx )
{
    // default is to do nothing; feel free to override it
    return eBioseqSkip_No;
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CStartSectionItem & head_item )
{
    return unified_notify(block_text, ctx, head_item, fGenbankBlocks_Head);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CHtmlAnchorItem & anchor_item )
{
    return unified_notify(block_text, ctx, anchor_item, fGenbankBlocks_None);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CLocusItem &locus_item )
{
    return unified_notify(block_text, ctx, locus_item, fGenbankBlocks_Locus);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CDeflineItem & defline_item )
{
    return unified_notify(block_text, ctx,  defline_item, fGenbankBlocks_Defline);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CAccessionItem & accession_item )
{
    return unified_notify(block_text, ctx,  accession_item, fGenbankBlocks_Accession);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CVersionItem & version_item )
{
    return unified_notify(block_text, ctx,  version_item, fGenbankBlocks_Version);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CGenomeProjectItem & project_item )
{
    return unified_notify(block_text, ctx,  project_item, fGenbankBlocks_Project);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CDBSourceItem & dbsource_item )
{
    return unified_notify(block_text, ctx,  dbsource_item, fGenbankBlocks_Dbsource);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CKeywordsItem & keywords_item )
{
    return unified_notify(block_text, ctx,  keywords_item, fGenbankBlocks_Keywords);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSegmentItem & segment_item )
{
    return unified_notify(block_text, ctx,  segment_item, fGenbankBlocks_Segment);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSourceItem & source_item )
{
    return unified_notify(block_text, ctx,  source_item, fGenbankBlocks_Source);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CReferenceItem & ref_item )
{
    return unified_notify(block_text, ctx,  ref_item, fGenbankBlocks_Reference);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CCacheItem & cache_item )
{
    return unified_notify(block_text, ctx,  cache_item, fGenbankBlocks_Cache);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CCommentItem & comment_item )
{
    return unified_notify(block_text, ctx,  comment_item, fGenbankBlocks_Comment);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CPrimaryItem & primary_item )
{
    return unified_notify(block_text, ctx,  primary_item, fGenbankBlocks_Primary);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CFeatHeaderItem & featheader_item )
{
    return unified_notify(block_text, ctx,  featheader_item, fGenbankBlocks_Featheader);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSourceFeatureItem & sourcefeat_item )
{
    return unified_notify(block_text, ctx,  sourcefeat_item, fGenbankBlocks_Sourcefeat);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CFeatureItem & feature_item )
{
    return unified_notify(block_text, ctx,  feature_item, fGenbankBlocks_FeatAndGap);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CGapItem & feature_item )
{
    return unified_notify(block_text, ctx,  feature_item, fGenbankBlocks_FeatAndGap);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CBaseCountItem & basecount_item )
{
    return unified_notify(block_text, ctx,  basecount_item, fGenbankBlocks_Basecount);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const COriginItem & origin_item )
{
    return unified_notify(block_text, ctx,  origin_item, fGenbankBlocks_Origin);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSequenceItem & sequence_chunk_item )
{
    return unified_notify(block_text, ctx,  sequence_chunk_item, fGenbankBlocks_Sequence);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CContigItem & contig_item )
{
    return unified_notify(block_text, ctx,  contig_item, fGenbankBlocks_Contig);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CWGSItem & wgs_item )
{
    return unified_notify(block_text, ctx,  wgs_item, fGenbankBlocks_Wgs);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CTSAItem & tsa_item )
{
    return unified_notify(block_text, ctx,  tsa_item, fGenbankBlocks_Tsa);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CEndSectionItem & slash_item )
{
    return unified_notify(block_text, ctx,  slash_item, fGenbankBlocks_Slash);
}



CFlatFileConfig::CFlatFileConfig(
    TFormat format,
    TMode mode,
    TStyle style,
    TFlags flags,
    TView view,
    TPolicy policy,
    TCustom custom) :
    m_Format(format), m_Mode(mode), m_Style(style), m_Flags(flags), m_View(view), m_Policy(policy), m_Custom(custom)
{
    m_RefSeqConventions = false;
    m_FeatDepth = 0;
    m_GapDepth = 0;
    SetGenbankBlocks(fGenbankBlocks_All);
    SetGenbankBlockCallback(nullptr);
    SetCanceledCallback(nullptr);
    BasicCleanup(false);

    // FTable always requires master style
    if (m_Format == eFormat_FTable) {
        m_Style = eStyle_Master;
    }
    m_html_formatter.Reset(new CHTMLEmptyFormatter);
}


// -- destructor
CFlatFileConfig::~CFlatFileConfig(void)
{
}


// -- mode flags

// mode flags initialization
const bool CFlatFileConfig::sm_ModeFlags[4][32] = {
    // Release
    {
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, false, false, true,
        false, false
    },
    // Entrez
    // ID-4625 : do not drop illegal qualifiers
    {
        false, true, true, true, true, false, true, true, false, true,
        true, true, true, true, true, true, true, false, true, true,
        true, true, true, true, false, true, true, true, false, true,
        false, false
    },
    // GBench
    {
        false, false, false, false, false, false, false, true, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, true, false,
        false, false
    },
    // Dump
    {
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, true, false, true, false,
        false, false
    }
};


#define MODE_FLAG_GET(x, y) \
bool CFlatFileConfig::x(void) const \
{ \
    return sm_ModeFlags[static_cast<size_t>(m_Mode)][y]; \
} \

MODE_FLAG_GET(SuppressLocalId, 0);
MODE_FLAG_GET(ValidateFeatures, 1);
MODE_FLAG_GET(IgnorePatPubs, 2);
MODE_FLAG_GET(DropShortAA, 3);
MODE_FLAG_GET(AvoidLocusColl, 4);
MODE_FLAG_GET(IupacaaOnly, 5);
MODE_FLAG_GET(DropBadCitGens, 6);
MODE_FLAG_GET(NoAffilOnUnpub, 7);
MODE_FLAG_GET(DropIllegalQuals, 8);
MODE_FLAG_GET(CheckQualSyntax, 9);
MODE_FLAG_GET(NeedRequiredQuals, 10);
MODE_FLAG_GET(NeedOrganismQual, 11);
MODE_FLAG_GET(NeedAtLeastOneRef, 12);
MODE_FLAG_GET(CitArtIsoJta, 13);
MODE_FLAG_GET(DropBadDbxref, 14);
MODE_FLAG_GET(UseEmblMolType, 15);
MODE_FLAG_GET(HideBankItComment, 16);
MODE_FLAG_GET(CheckCDSProductId, 17);
MODE_FLAG_GET(FrequencyToNote, 18);
//MODE_FLAG_GET(SrcQualsToNote, 19); // implementation below
MODE_FLAG_GET(HideEmptySource, 20);
// MODE_FLAG_GET(GoQualsToNote, 21); // implementation below
//MODE_FLAG_GET(SelenocysteineToNote, 23); // implementation below
MODE_FLAG_GET(ForGBRelease, 24);
MODE_FLAG_GET(HideUnclassPartial, 25);
// MODE_FLAG_GET(CodonRecognizedToNote, 26); // implementation below
MODE_FLAG_GET(GoQualsEachMerge, 27);
MODE_FLAG_GET(ShowOutOfBoundsFeats, 28);
MODE_FLAG_GET(HideSpecificGeneMaps, 29);

#undef MODE_FLAG_GET

bool CFlatFileConfig::SrcQualsToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][19];
}

bool CFlatFileConfig::GoQualsToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][21];
}

bool CFlatFileConfig::SelenocysteineToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][23];
}

bool CFlatFileConfig::CodonRecognizedToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][26];
}

typedef SStaticPair<const char *, CFlatFileConfig::FGenbankBlocks>  TBlockElem;
static const TBlockElem sc_block_map[] = {
    { "accession",  CFlatFileConfig::fGenbankBlocks_Accession },
    { "all",        CFlatFileConfig::fGenbankBlocks_All },
    { "basecount",  CFlatFileConfig::fGenbankBlocks_Basecount },
    { "comment",    CFlatFileConfig::fGenbankBlocks_Comment },
    { "contig",     CFlatFileConfig::fGenbankBlocks_Contig },
    { "dbsource",   CFlatFileConfig::fGenbankBlocks_Dbsource },
    { "defline",    CFlatFileConfig::fGenbankBlocks_Defline },
    { "featandgap", CFlatFileConfig::fGenbankBlocks_FeatAndGap },
    { "featheader", CFlatFileConfig::fGenbankBlocks_Featheader },
    { "head",       CFlatFileConfig::fGenbankBlocks_Head },
    { "keywords",   CFlatFileConfig::fGenbankBlocks_Keywords },
    { "locus",      CFlatFileConfig::fGenbankBlocks_Locus },
    { "origin",     CFlatFileConfig::fGenbankBlocks_Origin },
    { "primary",    CFlatFileConfig::fGenbankBlocks_Primary },
    { "project",    CFlatFileConfig::fGenbankBlocks_Project },
    { "reference",  CFlatFileConfig::fGenbankBlocks_Reference },
    { "segment",    CFlatFileConfig::fGenbankBlocks_Segment },
    { "sequence",   CFlatFileConfig::fGenbankBlocks_Sequence },
    { "slash",      CFlatFileConfig::fGenbankBlocks_Slash },
    { "source",     CFlatFileConfig::fGenbankBlocks_Source },
    { "sourcefeat", CFlatFileConfig::fGenbankBlocks_Sourcefeat },
    { "tsa",        CFlatFileConfig::fGenbankBlocks_Tsa },
    { "version",    CFlatFileConfig::fGenbankBlocks_Version },
    { "wgs",        CFlatFileConfig::fGenbankBlocks_Wgs }
};
typedef CStaticArrayMap<const char *, CFlatFileConfig::FGenbankBlocks, PNocase_CStr> TBlockMap;
DEFINE_STATIC_ARRAY_MAP(TBlockMap, sc_BlockMap, sc_block_map);

// static
CFlatFileConfig::FGenbankBlocks CFlatFileConfig::StringToGenbankBlock(const string & str)
{
    TBlockMap::const_iterator find_iter = sc_BlockMap.find(str.c_str());
    if( find_iter == sc_BlockMap.end() ) {
        throw runtime_error("Could not translate this string to a Genbank block type: " + str);
    }
    return find_iter->second;
}

// static
const vector<string>&
CFlatFileConfig::GetAllGenbankStrings(void)
{
    static vector<string> s_vecOfGenbankStrings;
    static CFastMutex s_mutex;

    CFastMutexGuard guard(s_mutex);
    if( s_vecOfGenbankStrings.empty() ) {
        // use "set" for sorting and uniquing
        set<string> setOfGenbankStrings;
        ITERATE(TBlockMap, map_iter, sc_BlockMap) {
            setOfGenbankStrings.insert(map_iter->first);
        }
        copy( setOfGenbankStrings.begin(),
            setOfGenbankStrings.end(),
            back_inserter(s_vecOfGenbankStrings) );
    }

    return s_vecOfGenbankStrings;
}

void
CFlatFileConfig::x_ThrowHaltNow(void) const
{
    NCBI_THROW(CFlatException, eHaltRequested,
        "FlatFile Generation canceled" );
}

void CFlatFileConfig::AddArgumentDescriptions(CArgDescriptions& args)
{
    CArgDescriptions* arg_desc = & args;

    // report
    {{
         arg_desc->SetCurrentGroup("Formatting Options");
         // format (default: genbank)
         arg_desc->AddDefaultKey("format", "Format",
                                 "Output format",
                                 CArgDescriptions::eString, "genbank");
         arg_desc->SetConstraint("format",
                                 &(*new CArgAllow_Strings,
                                   "genbank", "embl", "ddbj", "gbseq", "insdseq", "ftable", "gff", "gff3", "lite"));

         // mode (default: dump)
         arg_desc->AddDefaultKey("mode", "Mode",
                                 "Restriction level",
                                 CArgDescriptions::eString, "gbench");
         arg_desc->SetConstraint("mode",
                                 &(*new CArgAllow_Strings, "release", "entrez", "gbench", "dump"));

         // style (default: normal)
         arg_desc->AddDefaultKey("style", "Style",
                                 "Formatting style",
                                 CArgDescriptions::eString, "normal");
         arg_desc->SetConstraint("style",
                                 &(*new CArgAllow_Strings, "normal", "segment", "master", "contig", "conwithfeat"));

         // policy (default: adaptive)
         arg_desc->AddDefaultKey("policy", "Policy",
                                 "Far fetch policy",
                                 CArgDescriptions::eString, "adaptive");
         arg_desc->SetConstraint("policy",
                                 &(*new CArgAllow_Strings, "adaptive", "internal", "external", "exhaustive", "ftp", "web", "genomes"));

#define DENT "  "
         // flags (default: 0)
         arg_desc->AddDefaultKey("flags", "COMMA_SEPARATED_FLAGS_LIST",
                                 "Flags controlling flat file output.\n"
                                 "The value is the bitwise OR (logical addition) of:\n"
         DENT "DoHTML                    (1) - show HTML report\n"
         DENT "ShowContigFeatures        (2) - show contig features\n"
         DENT "ShowContigSources         (4) - show contig sources\n"
         DENT "ShowFarTranslations       (8) - show far translations\n"
         DENT "TranslateIfNoProduct     (16) - show translations if no products\n"
         DENT "AlwaysTranslateCDS       (32) - always translate CDS\n"
         DENT "OnlyNearFeatures         (64) - show only near features\n"
         DENT "FavorFarFeatures        (128) - show far features on segs\n"
         DENT "CopyCDSFromCDNA         (256) - copy CDS feature from cDNA\n"
         DENT "CopyGeneToCDNA          (512) - copy gene to cDNA\n"
         DENT "ShowContigInMaster     (1024) - show contig in master\n"
         DENT "HideImpFeatures        (2048) - hide imported features\n"
         DENT "HideRemoteImpFeatures  (4096) - hide remote imported features\n"
         DENT "HideSNPFeatures        (8192) - hide SNP features\n"
         DENT "HideExonFeatures      (16384) - hide exon features\n"
         DENT "HideIntronFeatures    (32768) - hide intron features\n"
         DENT "HideMiscFeatures      (65536) - hide misc features\n"
         DENT "HideCDSProdFeatures  (131072) - hide CDS product features\n"
         DENT "HideCDDFeatures      (262144) - hide CDD features\n"
         DENT "ShowTranscript       (542288) - show transcript sequence\n"
         DENT "ShowPeptides        (1048576) - show peptides\n"
         DENT "HideGeneRIFs        (2097152) - hide GeneRIFs\n"
         DENT "OnlyGeneRIFs        (4194304) - show only GeneRIFs\n"
         DENT "LatestGeneRIFs      (8388608) - show only the latest GeneRIFs\n"
         DENT "ShowContigAndSeq   (16777216) - show contig and sequence\n"
         DENT "HideSourceFeatures (33554432) - hide source features\n"
         DENT "ShowFtableRefs     (67108864) - show feature table references\n"
         DENT "OldFeaturesOrder  (134217728) - use the old feature sort order\n"
         DENT "HideGapFeatures   (268435456) - hide gap features\n"
         DENT "NeverTranslateCDS (536870912) - do not translate the CDS\n"
         DENT "ShowSeqSpans     (1073741824) - show javascript sequence spans",

                                 CArgDescriptions::eString, "");

         // custom (default: 0)
         arg_desc->AddDefaultKey("custom", "COMMA_SEPARATED_FLAGS_LIST",
                                 "Custom flat file output bits.\n"
                                 "The value is the bitwise OR (logical addition) of:\n"
         DENT "HideProteinID             (1) - hide protein_id and transcript_id\n"
         DENT "HideGI                    (2) - hide GI number\n"
         DENT "LongLocusNames            (4) - allow long locus lines\n"
         DENT "ExpandGaps                (8) - show Expand Ns link\n"
         DENT "ShowSNPFeatures          (64) - show SNP features\n"
         DENT "ShowCDDFeatures         (128) - show CDD features\n"
         DENT "ShowDebugTiming         (256) - show debug timing\n"
         DENT "FasterReleaseSets       (512) - faster release sets\n"
         DENT "DisableAnnotRefs       (1024) - disable annot references\n"
         DENT "UseSeqEntryIndexer     (2048) - use SeqEntry indexer\n"
         DENT "UseAutoDef             (4096) - use automatic defline generator\n"
         DENT "IgnoreExistingTitle    (8192) - ignore existing title\n"
         DENT "GeneRNACDSFeatures    (16384) - only Gene, RNA, and CDS features\n"
         DENT "ShowFtablePeptides    (32768) - show peptide qualifierss in feature table\n"
         DENT "DisableReferenceCache (65536) - disable reference cache\n"
         DENT "ShowDeflineModifiers (131072) - show definition line modifiers\n"
         DENT "DoNotUseAutoDef      (262144) - suppress automatic defline generator\n"
         DENT "OldTpaDisplay        (542288) - old TPA display with PRIMARY block\n"
         DENT "DisableDefaultIndex (1048576) - disable SeqEntry indexer\n"
         DENT "GeoLocNameCountry   (2097152) - use geo_loc_name instead of country\n"
         DENT "LeavePrefixSuffix   (4194304) - keep existing prefix and suffix",

                                 CArgDescriptions::eString, "");
#undef DENT

         arg_desc->AddOptionalKey("showblocks", "COMMA_SEPARATED_BLOCK_LIST",
             "Use this to only show certain parts of the flatfile (e.g. '-showblocks locus,defline').  "
             "These are all possible values for block names: " + NStr::Join(CFlatFileConfig::GetAllGenbankStrings(), ", "),
             CArgDescriptions::eString );
         arg_desc->AddOptionalKey("skipblocks", "COMMA_SEPARATED_BLOCK_LIST",
             "Use this to skip certain parts of the flatfile (e.g. '-skipblocks sequence,origin').  "
             "These are all possible values for block names: " + NStr::Join(CFlatFileConfig::GetAllGenbankStrings(), ", "),
             CArgDescriptions::eString );
         // don't allow both because it's not really clear what the user intended.
         arg_desc->SetDependency("showblocks", CArgDescriptions::eExcludes, "skipblocks");

         arg_desc->AddFlag("demo-genbank-callback",
             "When set (and genbank mode is used), this program will demonstrate the use of "
             "genbank callbacks via a very simple callback that just prints its output to stderr, then "
             "prints some statistics.  To demonstrate halting of flatfile generation, the genbank callback "
             "will halt flatfile generation if it encounters an item with the words 'HALT TEST'.  To demonstrate skipping a block, it will skip blocks with the words 'SKIP TEST' in them.  Also, blocks with the words 'MODIFY TEST' in them will have the text 'MODIFY TEST' turned into 'WAS MODIFIED TEST'.");

         arg_desc->AddFlag("no-external",
                           "Disable all external annotation sources");

         arg_desc->AddFlag("enable-external",
                           "Enable loading of external annotation sources for local file");

         arg_desc->AddFlag("resolve-all",
                           "Resolves all, e.g. for contigs.");

         arg_desc->AddOptionalKey("depth", "Depth",
                                  "Exploration depth", CArgDescriptions::eInteger);

         arg_desc->AddOptionalKey("gap-depth", "GapDepth",
                                  "Gap exploration depth", CArgDescriptions::eInteger);

         arg_desc->AddOptionalKey("max_search_segments", "MaxSearchSegments",
                                  "Max number of empty segments to search", CArgDescriptions::eInteger);

          arg_desc->AddOptionalKey("max_search_time", "MaxSearchTime",
                                  "Max time to search for first annotation", CArgDescriptions::eDouble);

        arg_desc->AddFlag("show-flags",
                           "Describe the current flag set in ENUM terms");

         // view (default: nucleotide)
         arg_desc->AddDefaultKey("view", "View", "View",
                                 CArgDescriptions::eString, "nuc");
         arg_desc->SetConstraint("view",
                                 &(*new CArgAllow_Strings, "all", "prot", "nuc"));

         // from
         arg_desc->AddOptionalKey("from", "From",
                                  "Begining of shown range", CArgDescriptions::eInteger);

         // to
         arg_desc->AddOptionalKey("to", "To",
                                  "End of shown range", CArgDescriptions::eInteger);

         // strand
         arg_desc->AddOptionalKey("strand", "Strand",
                                  "1 (plus) or 2 (minus)", CArgDescriptions::eInteger);

         // multi-interval location
         arg_desc->AddOptionalKey("location", "Location",
                                   "Multi-interval region to show", CArgDescriptions::eString);

         // accession to extract

         // html
         arg_desc->AddFlag("html", "Produce HTML output");
     }}

    // misc
    {{
         // cleanup
         arg_desc->AddFlag("cleanup",
                           "Do internal data cleanup prior to formatting");
         // no-cleanup
         arg_desc->AddFlag("nocleanup",
                           "Do not perform data cleanup prior to formatting");
         // remote
         arg_desc->AddFlag("gbload", "Use GenBank data loader");

         // repetition
         arg_desc->AddDefaultKey("count", "Count", "Number of runs",
                                 CArgDescriptions::eInteger, "1");
     }}
}

namespace
{
CFlatFileConfig::EFormat x_GetFormat(const CArgs& args)
{
    const string& format = args["format"].AsString();
    if ( format == "genbank" ) {
        return CFlatFileConfig::eFormat_GenBank;
    } else if ( format == "embl" ) {
        return CFlatFileConfig::eFormat_EMBL;
    } else if ( format == "ddbj" ) {
        return CFlatFileConfig::eFormat_DDBJ;
    } else if ( format == "gbseq" ) {
        return CFlatFileConfig::eFormat_GBSeq;
    } else if ( format == "insdseq" ) {
        return CFlatFileConfig::eFormat_INSDSeq;
    } else if ( format == "ftable" ) {
        return CFlatFileConfig::eFormat_FTable;
    } else if ( format == "lite" ) {
        return CFlatFileConfig::eFormat_Lite;
    }
    if (format == "gff"  ||  format == "gff3") {
        string msg =
            "Asn2flat no longer supports GFF and GFF3 generation. "
            "For state-of-the-art GFF output, use annotwriter.";
        NCBI_THROW(CException, eInvalid, msg);
    }
    // default
    return CFlatFileConfig::eFormat_GenBank;
}


CFlatFileConfig::EMode x_GetMode(const CArgs& args)
{
    const string& mode = args["mode"].AsString();
    if ( mode == "release" ) {
        return CFlatFileConfig::eMode_Release;
    } else if ( mode == "entrez" ) {
        return CFlatFileConfig::eMode_Entrez;
    } else if ( mode == "gbench" ) {
        return CFlatFileConfig::eMode_GBench;
    } else if ( mode == "dump" ) {
        return CFlatFileConfig::eMode_Dump;
    }

    // default
    return CFlatFileConfig::eMode_GBench;
}


CFlatFileConfig::EStyle x_GetStyle(const CArgs& args)
{
    const string& style = args["style"].AsString();
    if ( style == "normal" ) {
        return CFlatFileConfig::eStyle_Normal;
    } else if ( style == "segment" ) {
        return CFlatFileConfig::eStyle_Segment;
    } else if ( style == "master" ) {
        return CFlatFileConfig::eStyle_Master;
    } else if ( style == "contig" ) {
        return CFlatFileConfig::eStyle_Contig;
    } else if ( style == "conwithfeat" ) {
        return CFlatFileConfig::eStyle_Contig;
    }

    // default
    return CFlatFileConfig::eStyle_Normal;
}


CFlatFileConfig::EPolicy x_GetPolicy(const CArgs& args)
{
    const string& Policy = args["policy"].AsString();
    if ( Policy == "adaptive" ) {
        return CFlatFileConfig::ePolicy_Adaptive;
    } else if ( Policy == "internal" ) {
        return CFlatFileConfig::ePolicy_Internal;
    } else if ( Policy == "external" ) {
        return CFlatFileConfig::ePolicy_External;
    } else if ( Policy == "exhaustive" ) {
        return CFlatFileConfig::ePolicy_Exhaustive;
    } else if ( Policy == "ftp" ) {
        return CFlatFileConfig::ePolicy_Ftp;
    } else if ( Policy == "web" ) {
        return CFlatFileConfig::ePolicy_Web;
    } else if ( Policy == "genomes" ) {
        return CFlatFileConfig::ePolicy_Genomes;
    }

    // default
    return CFlatFileConfig::ePolicy_Adaptive;
}


static
CFlatFileConfig::TFlags x_GetFlags(const CArgs& args)
{
    CFlatFileConfig::TFlags flags = 0;
    string sFlags = args["flags"].AsString();
    if (!sFlags.empty()) {
        vector<string> vFlags;
        NStr::Split(sFlags, ",", vFlags, NStr::fSplit_Tokenize);
        for (const string& sFlag : vFlags) {
            if (isdigit(sFlag[0])) {
                try {
                    unsigned n = NStr::StringToUInt(sFlag);
                    flags |= n;
                } catch (const CStringException&) {
                    throw;
                }
            } else {
                static const map<string, CFlatFileConfig::EFlags> mnemonics {
#define DOFLG(mnem) { #mnem, CFlatFileConfig::f##mnem }
                    DOFLG(DoHTML),
                    DOFLG(ShowContigFeatures),
                    DOFLG(ShowContigSources),
                    DOFLG(ShowFarTranslations),
                    DOFLG(TranslateIfNoProduct),
                    DOFLG(AlwaysTranslateCDS),
                    DOFLG(OnlyNearFeatures),
                    DOFLG(FavorFarFeatures),
                    DOFLG(CopyCDSFromCDNA),
                    DOFLG(CopyGeneToCDNA),
                    DOFLG(ShowContigInMaster),
                    DOFLG(HideImpFeatures),
                    DOFLG(HideRemoteImpFeatures),
                    DOFLG(HideSNPFeatures),
                    DOFLG(HideExonFeatures),
                    DOFLG(HideIntronFeatures),
                    DOFLG(HideMiscFeatures),
                    DOFLG(HideCDSProdFeatures),
                    DOFLG(HideCDDFeatures),
                    DOFLG(ShowTranscript),
                    DOFLG(ShowPeptides),
                    DOFLG(HideGeneRIFs),
                    DOFLG(OnlyGeneRIFs),
                    DOFLG(LatestGeneRIFs),
                    DOFLG(ShowContigAndSeq),
                    DOFLG(HideSourceFeatures),
                    DOFLG(ShowFtableRefs),
                    DOFLG(OldFeaturesOrder),
                    DOFLG(HideGapFeatures),
                    DOFLG(NeverTranslateCDS),
                    DOFLG(ShowSeqSpans),
#undef DOFLG
                };

                auto it = mnemonics.find(sFlag);
                if (it != mnemonics.cend()) {
                    flags |= it->second;
                } else {
                    cerr << "Error: Unrecognized flag: " << sFlag << endl;
                }
            }
        }
    }

    if (args["html"]) {
        flags |= CFlatFileConfig::fDoHTML;
    }

    if (args["show-flags"]) {

        typedef pair<CFlatFileConfig::EFlags, const char*> TFlagDescr;
        static const TFlagDescr kDescrTable[] = {
#define DOFLG(mnem) TFlagDescr(CFlatFileConfig::f##mnem, "CFlatFileConfig::f" #mnem)
            DOFLG(DoHTML),
            DOFLG(ShowContigFeatures),
            DOFLG(ShowContigSources),
            DOFLG(ShowFarTranslations),
            DOFLG(TranslateIfNoProduct),
            DOFLG(AlwaysTranslateCDS),
            DOFLG(OnlyNearFeatures),
            DOFLG(FavorFarFeatures),
            DOFLG(CopyCDSFromCDNA),
            DOFLG(CopyGeneToCDNA),
            DOFLG(ShowContigInMaster),
            DOFLG(HideImpFeatures),
            DOFLG(HideRemoteImpFeatures),
            DOFLG(HideSNPFeatures),
            DOFLG(HideExonFeatures),
            DOFLG(HideIntronFeatures),
            DOFLG(HideMiscFeatures),
            DOFLG(HideCDSProdFeatures),
            DOFLG(HideCDDFeatures),
            DOFLG(ShowTranscript),
            DOFLG(ShowPeptides),
            DOFLG(HideGeneRIFs),
            DOFLG(OnlyGeneRIFs),
            DOFLG(LatestGeneRIFs),
            DOFLG(ShowContigAndSeq),
            DOFLG(HideSourceFeatures),
            DOFLG(ShowFtableRefs),
            DOFLG(OldFeaturesOrder),
            DOFLG(HideGapFeatures),
            DOFLG(NeverTranslateCDS),
            DOFLG(ShowSeqSpans),
#undef DOFLG
        };
        static const size_t kArraySize = ArraySize(kDescrTable);
        for (size_t i = 0; i < kArraySize; ++i) {
            if (flags & kDescrTable[i].first) {
                LOG_POST(Error << "flag: "
                         << std::left << setw(40) << kDescrTable[i].second
                         << " = "
                         << std::right << setw(10) << kDescrTable[i].first
                        );
            }
        }
    }

    const string& style = args["style"].AsString();
    if ( style == "conwithfeat" ) {
        flags |= CFlatFileConfig::fShowContigFeatures;
        flags |= CFlatFileConfig::fShowContigSources;
    }

    return flags;
}

static
CFlatFileConfig::TCustom x_GetCustom(const CArgs& args)
{
    CFlatFileConfig::TCustom custom = 0;
    string sCustom = args["custom"].AsString();
    if (!sCustom.empty()) {
        vector<string> vFlags;
        NStr::Split(sCustom, ",", vFlags, NStr::fSplit_Tokenize);
        for (const string& sFlag : vFlags) {
            if (isdigit(sFlag[0])) {
                try {
                    unsigned n = NStr::StringToUInt(sFlag);
                    custom |= n;
                } catch (const CStringException&) {
                    throw;
                }
            } else {
                static const map<string, CFlatFileConfig::ECustom> mnemonics {
#define DOFLG(mnem) { #mnem, CFlatFileConfig::f##mnem }
                    DOFLG(HideProteinID),
                    DOFLG(HideGI),
                    DOFLG(LongLocusNames),
                    DOFLG(ExpandGaps),
                    DOFLG(ShowSNPFeatures),
                    DOFLG(ShowCDDFeatures),
                    DOFLG(ShowDebugTiming),
                    DOFLG(FasterReleaseSets),
                    DOFLG(DisableAnnotRefs),
                    DOFLG(UseSeqEntryIndexer),
                    DOFLG(UseAutoDef),
                    DOFLG(IgnoreExistingTitle),
                    DOFLG(GeneRNACDSFeatures),
                    DOFLG(ShowFtablePeptides),
                    DOFLG(DisableReferenceCache),
                    DOFLG(ShowDeflineModifiers),
                    DOFLG(DoNotUseAutoDef),
                    DOFLG(DisableDefaultIndex),
                    DOFLG(GeoLocNameCountry),
#undef DOFLG
                };

                auto it = mnemonics.find(sFlag);
                if (it != mnemonics.cend()) {
                    custom |= it->second;
                } else {
                    cerr << "Error: Unrecognized flag: " << sFlag << endl;
                }
            }
        }
    }

    if (args["show-flags"]) {

        typedef pair<CFlatFileConfig::ECustom, const char*> TFlagDescr;
        static const TFlagDescr kDescrTable[] = {
#define DOFLG(mnem) TFlagDescr(CFlatFileConfig::f##mnem, "CFlatFileConfig::f" #mnem)
            DOFLG(HideProteinID),
            DOFLG(HideGI),
            DOFLG(LongLocusNames),
            DOFLG(ExpandGaps),
            DOFLG(ShowSNPFeatures),
            DOFLG(ShowCDDFeatures),
            DOFLG(ShowDebugTiming),
            DOFLG(FasterReleaseSets),
            DOFLG(DisableAnnotRefs),
            DOFLG(UseSeqEntryIndexer),
            DOFLG(UseAutoDef),
            DOFLG(IgnoreExistingTitle),
            DOFLG(GeneRNACDSFeatures),
            DOFLG(ShowFtablePeptides),
            DOFLG(DisableReferenceCache),
            DOFLG(ShowDeflineModifiers),
            DOFLG(DoNotUseAutoDef),
            DOFLG(DisableDefaultIndex),
            DOFLG(GeoLocNameCountry),
#undef DOFLG
        };
        static const size_t kArraySize = ArraySize(kDescrTable);
        for (size_t i = 0; i < kArraySize; ++i) {
            if (custom & kDescrTable[i].first) {
                LOG_POST(Error << "custom: "
                         << std::left << setw(38) << kDescrTable[i].second
                         << " = "
                         << std::right << setw(10) << kDescrTable[i].first
                        );
            }
        }
    }

    return custom;
}


CFlatFileConfig::EView x_GetView(const CArgs& args)
{
    const string& view = args["view"].AsString();
    if ( view == "all" ) {
        return CFlatFileConfig::fViewAll;
    } else if ( view == "prot" ) {
        return CFlatFileConfig::fViewProteins;
    } else if ( view == "nuc" ) {
        return CFlatFileConfig::fViewNucleotides;
    }

    // default
    return CFlatFileConfig::fViewNucleotides;
}

CFlatFileConfig::TGenbankBlocks x_GetGenbankBlocks(const CArgs& args)
{
    const static CFlatFileConfig::TGenbankBlocks kDefault =
        CFlatFileConfig::fGenbankBlocks_All;

    string blocks_arg;
    // set to true if we're hiding the blocks given instead of showing them
    bool bInvertFlags = false;
    if( args["showblocks"] ) {
        blocks_arg = args["showblocks"].AsString();
    } else if( args["skipblocks"] ) {
        blocks_arg = args["skipblocks"].AsString();
        bInvertFlags = true;
    } else {
        return kDefault;
    }

    // turn the blocks into one mask
    CFlatFileConfig::TGenbankBlocks fBlocksGiven = 0;
    vector<string> vecOfBlockNames;
    NStr::Split(blocks_arg, ",", vecOfBlockNames);
    ITERATE(vector<string>, name_iter, vecOfBlockNames) {
        // Note that StringToGenbankBlock throws an
        // exception if it gets an illegal value.
        CFlatFileConfig::TGenbankBlocks fThisBlock =
            CFlatFileConfig::StringToGenbankBlock(
            NStr::TruncateSpaces(*name_iter));
        fBlocksGiven |= fThisBlock;
    }

    return ( bInvertFlags ? ~fBlocksGiven : fBlocksGiven );
}
}

void CFlatFileConfig::FromArguments(const CArgs& args)
{
    CFlatFileConfig::EFormat        format         = x_GetFormat(args);
    CFlatFileConfig::EMode          mode           = x_GetMode(args);
    CFlatFileConfig::EStyle         style          = x_GetStyle(args);
    CFlatFileConfig::TFlags         flags          = x_GetFlags(args);
    CFlatFileConfig::EView          view           = x_GetView(args);
    CFlatFileConfig::EPolicy        policy         = x_GetPolicy(args);
    CFlatFileConfig::TGenbankBlocks genbank_blocks = x_GetGenbankBlocks(args);
    CFlatFileConfig::TCustom        custom         = x_GetCustom(args);

    // ID-5865 : Set the "show SNP" and "show CDD" bits based on the value of the
    // "enable-external" flag.
    if (args["no-external"]) {
        flags |= CFlatFileConfig::fHideCDDFeatures;
        flags |= CFlatFileConfig::fHideSNPFeatures;
    } else if (args["enable-external"] || args["policy"].AsString() == "external") {
        if ((flags & CFlatFileConfig::fHideCDDFeatures) == 0) {
            custom |= CFlatFileConfig::fShowCDDFeatures;
        }
        if ((flags & CFlatFileConfig::fHideSNPFeatures) == 0) {
            custom |= CFlatFileConfig::fShowSNPFeatures;
        }
    }

    SetFormat(format);
    SetMode(mode);
    SetStyle(style);
    SetFlags(flags);
    SetView(view);
    SetPolicy(policy);
    m_fGenbankBlocks = genbank_blocks;
    m_BasicCleanup = args["cleanup"];
    SetCustom(custom);

    if( args["depth"] ) {
        int featDepth = args["depth"].AsInteger();
        SetFeatDepth(featDepth);
    }
    if( args["gap-depth"] ) {
        int gapDepth = args["gap-depth"].AsInteger();
        SetGapDepth(gapDepth);
    }
    if (args["accn"]) {
        string singleAccn = args["accn"].AsString();
        SetSingleAccession(singleAccn);
    }
}

void CHTMLEmptyFormatter::FormatProteinId(string& str, const CSeq_id& seq_id, const string& prot_id) const
{
    str = prot_id;
}

void CHTMLEmptyFormatter::FormatTranscriptId(string& str, const CSeq_id& seq_id, const string& nuc_id) const
{
    str = nuc_id;
}

void CHTMLEmptyFormatter::FormatNucId(string& str, const CSeq_id& seq_id, TIntId gi, const string& acc_id) const
{
    str = acc_id;
}

void CHTMLEmptyFormatter::FormatLocation(string& str, const CSeq_loc& loc, TIntId gi, const string& visible_text) const
{
    str = visible_text;
}

void CHTMLEmptyFormatter::FormatModelEvidence(string& str, const SModelEvidance& me) const
{
    str = me.name;
}

void CHTMLEmptyFormatter::FormatNucSearch(CNcbiOstream& os, const string& id) const
{
    os << id;
}

void CHTMLEmptyFormatter::FormatTaxid(string& str, const TTaxId taxid, const string& taxname) const
{
    str = taxname;
}

void CHTMLEmptyFormatter::FormatTranscript(string& str, const string& name) const
{
    str = name;
}

void CHTMLEmptyFormatter::FormatGeneralId(CNcbiOstream& os, const string& id) const
{
    os << id;
}

void CHTMLEmptyFormatter::FormatGapLink(CNcbiOstream& os, TSeqPos gap_size, const string& id, bool is_prot) const
{
    os << "          [gap " << gap_size << " " << (is_prot ? "aa" : "bp" ) << "]";
}

void CHTMLEmptyFormatter::FormatUniProtId(string& str, const string& prot_id) const
{
    str = prot_id;
}

END_SCOPE(objects)
END_NCBI_SCOPE
