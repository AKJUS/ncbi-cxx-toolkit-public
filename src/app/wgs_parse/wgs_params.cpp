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

#include <objects/general/Date_std.hpp>

#include "wgs_params.hpp"
#include "wgs_filelist.hpp"
#include "wgs_utils.hpp"
#include "wgs_errors.hpp"

USING_SCOPE(objects);

namespace wgsparse
{

struct CParams_imp
{
    bool m_test;
    bool m_keep_refs;
    bool m_copy_cit_art_from_master;
    bool m_accs_assigned;
    bool m_trust_version;
    bool m_allow_diff_citsubs;
    bool m_preserve_input_path;
    bool m_ignore_general_ids;
    bool m_binary_in;
    bool m_binary_out;
    bool m_override_existing;
    bool m_taxonomy_lookup;
    bool m_replace_dbname;
    bool m_vdb_mode;
    bool m_accessions_sorted_in_file;
    bool m_scfld_test_mode;
    bool m_force_gcode;
    bool m_strip_authors;
    bool m_allow_secondary_accession;
    bool m_dblink_override;
    bool m_medline_lookup;
    bool m_enforce_new;
    bool m_different_biosamples;
    bool m_change_nuc_title;

    string m_outdir;
    string m_id_acc_file;
    string m_acc_file;
    string m_load_order_file;
    string m_accession;
    string m_tpa_keyword;
    string m_new_nuc_title;
    string m_master_file;
    string m_scaffold_prefix;

    EUpdateMode m_update_mode;
    EScaffoldType m_scaffold_type;
    ESortOrder m_sort_order;
    EInputType m_input_type;

    TSeqPos m_gap_size;
    int m_fix_tech;

    mutable ESource m_source;
    bool m_tpa_tsa;
    bool m_tpa;

    TIdContainer m_bioproject_ids;
    TIdContainer m_biosample_ids;
    TIdContainer m_sra_ids;

    CDate_std m_submission_date;

    list<string> m_file_list;

    size_t m_major_version_pos;
    size_t m_minor_version_pos;

    mutable string m_proj_acc;
    mutable string m_proj_acc_ver;

    CParams_imp() :
        m_test(false),
        m_keep_refs(false),
        m_copy_cit_art_from_master(false),
        m_accs_assigned(false),
        m_trust_version(false),
        m_allow_diff_citsubs(false),
        m_preserve_input_path(false),
        m_ignore_general_ids(false),
        m_binary_in(false),
        m_binary_out(false),
        m_override_existing(false),
        m_taxonomy_lookup(false),
        m_replace_dbname(false),
        m_vdb_mode(false),
        m_accessions_sorted_in_file(false),
        m_scfld_test_mode(false),
        m_force_gcode(false),
        m_strip_authors(false),
        m_allow_secondary_accession(false),
        m_dblink_override(false),
        m_medline_lookup(false),
        m_enforce_new(false),
        m_different_biosamples(false),
        m_change_nuc_title(false),
        m_update_mode(eUpdateNew),
        m_scaffold_type(eRegularGenomic),
        m_sort_order(eUnsorted),
        m_input_type(eSeqSubmit),
        m_gap_size(0),
        m_fix_tech(eNoFix),
        m_source(eNotSet),
        m_tpa_tsa(false),
        m_tpa(false),
        m_major_version_pos(0),
        m_minor_version_pos(0)
    {}

    void SetAccession(const string& accession)
    {
        size_t acc_len = accession.size();
        for (; m_major_version_pos < acc_len; ++m_major_version_pos) {
            if (isdigit(accession[m_major_version_pos])) {
                break;
            }
        }

        if (m_major_version_pos) {
            m_minor_version_pos = m_major_version_pos + 1;
        }

        m_accession = accession;
    }
};

CParams::CParams() :
    m_imp(new CParams_imp)
{
}

bool CParams::IsTest() const
{
    return m_imp->m_test;
}

EUpdateMode CParams::GetUpdateMode() const
{
    return m_imp->m_update_mode;
}

ESource CParams::GetSource() const
{
    _ASSERT(!m_imp->m_accession.empty() && "Accession should be set at this moment");

    if (m_imp->m_source == eNotSet) {
        switch (m_imp->m_accession.front()) {
            case 'B':
            case 'E':
            case 'I':
            case 'T':
                m_imp->m_source = eDDBJ;
                break;

            case 'C':
            case 'F':
            case 'H':
            case 'O':
            case 'U':
                m_imp->m_source = eEMBL;
                break;

            default:
                m_imp->m_source = eNCBI;
        }
    }

    return m_imp->m_source;
}

EScaffoldType CParams::GetScaffoldType() const
{
    return m_imp->m_scaffold_type;
}

bool CParams::IsTpa() const
{
    return(m_imp->m_tpa);
}

bool CParams::IsTsa() const
{
    _ASSERT(!m_imp->m_accession.empty() && "Accession should be set at this moment");

    if(m_imp->m_tpa_tsa) {
        return true;
    }

    static const string TSA_FIRST_LETTER = "GHI";
    return TSA_FIRST_LETTER.find_first_of(m_imp->m_accession.front()) != string::npos;
}

bool CParams::IsTls() const
{
    _ASSERT(!m_imp->m_accession.empty() && "Accession should be set at this moment");

    return m_imp->m_accession.front() == 'K';
}

bool CParams::IsWgs() const
{
    return !IsTsa() && !IsTls();
}

bool CParams::IsChromosomal() const
{
    return m_imp->m_scaffold_type == eRegularChromosomal || m_imp->m_scaffold_type == eTPAChromosomal;
}

bool CParams::IsMasterInFile() const
{
    return !m_imp->m_master_file.empty();
}

bool CParams::EnforceNew() const
{
    return m_imp->m_enforce_new;
}

bool CParams::IsAccessionAssigned() const
{
    return m_imp->m_accs_assigned;
}

bool CParams::IsDblinkOverride() const
{
    return m_imp->m_dblink_override;
}

bool CParams::IsSetSubmissionDate() const
{
    return m_imp->m_submission_date.IsSetYear() &&
           m_imp->m_submission_date.IsSetMonth() &&
           m_imp->m_submission_date.IsSetDay();
}

bool CParams::IsVDBMode() const
{
    return m_imp->m_vdb_mode;
}

bool CParams::IgnoreGeneralIds() const
{
    return m_imp->m_ignore_general_ids;
}

bool CParams::IsReplaceDBName() const
{
    return m_imp->m_replace_dbname;
}

bool CParams::IsSecondaryAccsAllowed() const
{
    return m_imp->m_allow_secondary_accession;
}

bool CParams::IsKeepRefs() const
{
    return m_imp->m_keep_refs;
}

bool CParams::IsAccessionsSortedInFile() const
{
    return m_imp->m_accessions_sorted_in_file;
}

bool CParams::IsUpdateScaffoldsMode() const
{
    return GetUpdateMode() == eUpdateScaffoldsNew || GetUpdateMode() == eUpdateScaffoldsUpd;
}

bool CParams::IsTaxonomyLookup() const
{
    return m_imp->m_taxonomy_lookup;
}

bool CParams::IsScaffoldTestMode() const
{
    return m_imp->m_scfld_test_mode;
}

bool CParams::IsForcedGencode() const
{
    return m_imp->m_force_gcode;
}

bool CParams::IsMedlineLookup() const
{
    return m_imp->m_medline_lookup;
}

bool CParams::IsPreserveInputPath() const
{
    return m_imp->m_preserve_input_path;
}

bool CParams::IsOverrideExisting() const
{
    return m_imp->m_override_existing;
}

bool CParams::IsBinaryOutput() const
{
    return m_imp->m_binary_out;
}

bool CParams::IsCitArtFromMaster() const
{
    return m_imp->m_copy_cit_art_from_master;
}

bool CParams::IsStripAuthors() const
{
    return m_imp->m_strip_authors;
}

bool CParams::IsDiffCitSubAllowed() const
{
    return m_imp->m_allow_diff_citsubs;
}

bool CParams::IsDiffBioSamplesAllowed() const
{
    return m_imp->m_different_biosamples;
}

bool CParams::IsVersionTrusted() const
{
    return m_imp->m_trust_version;
}

bool CParams::NeedToChangeNucTitle() const
{
    return m_imp->m_change_nuc_title;
}

const string& CParams::GetNewNucTitle() const
{
    return m_imp->m_new_nuc_title;
}

TSeqPos CParams::GetGapSize() const
{
    return m_imp->m_gap_size;
}

int CParams::GetFixTech() const
{
    return m_imp->m_fix_tech;
}

const string& CParams::GetMasterFileName() const
{
    return m_imp->m_master_file;
}

const string& CParams::GetTpaKeyword() const
{
    return m_imp->m_tpa_keyword;
}

static const size_t ACC_NUM_OF_DIGITS = 2;
static const size_t ACC_PREFIX_PREFIX_LEN = 3; // 'AB_'

string CParams::GetIdPrefix() const
{
    return m_imp->m_accession.substr(0, m_imp->m_accession.size() - ACC_NUM_OF_DIGITS);
}

size_t CParams::GetPrefixLenAfterUnderscore() const
{
    size_t len = m_imp->m_accession.size();
    if (m_imp->m_accession.find('_') != string::npos) {
        len -= ACC_PREFIX_PREFIX_LEN;
    }
    return len - ACC_NUM_OF_DIGITS;
}

CSeq_id::E_Choice CParams::GetIdChoice() const
{
    CSeq_id::E_Choice ret = CSeq_id::e_not_set;

    if (NStr::StartsWith(m_imp->m_accession, "NZ_")) {
        ret = CSeq_id::e_Other;
    }
    else if(IsTpa())
    {
        switch (GetSource()) {
            case eDDBJ:
                ret = CSeq_id::e_Tpd;
                break;
            case eEMBL:
                ret = CSeq_id::e_Tpe;
                break;
            default:
                ret = CSeq_id::e_Tpg;
        }
    }
    else {

        switch (GetSource()) {
            case eDDBJ:
                ret = CSeq_id::e_Ddbj;
                break;
            case eEMBL:
                ret = CSeq_id::e_Embl;
                break;
            default:
                ret = CSeq_id::e_Genbank;
        }
    }

    return ret;
}

const CDate_std& CParams::GetSubmissionDate() const
{
    return m_imp->m_submission_date;
}

const string& CParams::GetOutputDir() const
{
    return m_imp->m_outdir;
}

const string& CParams::GetScaffoldPrefix() const
{
    static const map<EScaffoldType, string> SCAFFOLD_PREFIX = {
        { eRegularGenomic, "GG" },
        { eRegularChromosomal, "CM" },
        { eGenColGenomic, "KK" },
        { eTPAGenomic, "GJ" },
        { eTPAChromosomal, "GK" }
    };

    if (m_imp->m_accs_assigned) {
        return m_imp->m_scaffold_prefix;
    }

    return SCAFFOLD_PREFIX.at(m_imp->m_scaffold_type);
}

char CParams::GetMajorAssemblyVersion() const
{
    return m_imp->m_accession[m_imp->m_major_version_pos];
}

char CParams::GetMinorAssemblyVersion() const
{
    return m_imp->m_accession[m_imp->m_minor_version_pos];
}

int CParams::GetAssemblyVersion() const
{
    return (GetMajorAssemblyVersion() - '0') * 10 + (GetMinorAssemblyVersion() - '0');
}

const list<string>& CParams::GetInputFiles() const
{
    return m_imp->m_file_list;
}

const string& CParams::GetProjPrefix() const
{
    static const string TLS("TLS:");
    static const string TSA("TSA:");
    static const string WGS("WGS:");

    if (IsTls()) {
        return TLS;
    }
    else if (IsTsa()) {
        return TSA;
    }
    return WGS;
}

const string& CParams::GetProjAccStr() const
{
    if (m_imp->m_proj_acc.empty()) {

        m_imp->m_proj_acc = GetProjPrefix();
        m_imp->m_proj_acc += m_imp->m_accession.substr(0, m_imp->m_major_version_pos);
    }

    return m_imp->m_proj_acc;
}

const string& CParams::GetProjAccVerStr() const
{
    if (m_imp->m_proj_acc_ver.empty()) {
        m_imp->m_proj_acc_ver = GetProjAccStr();
        m_imp->m_proj_acc_ver += GetMajorAssemblyVersion();
        m_imp->m_proj_acc_ver += GetMinorAssemblyVersion();
    }

    return m_imp->m_proj_acc_ver;
}

const string& CParams::GetAccession() const
{
    return m_imp->m_accession;
}

ESortOrder CParams::GetSortOrder() const
{
    return m_imp->m_sort_order;
}

const string& CParams::GetLoadOrderFile() const
{
    return m_imp->m_load_order_file;
}

const string& CParams::GetAccFile() const
{
    return m_imp->m_acc_file;
}

const string& CParams::GetIdAccFile() const
{
    return m_imp->m_id_acc_file;
}

const TIdContainer& CParams::GetBioProjectIds() const
{
    return m_imp->m_bioproject_ids;
}

const TIdContainer& CParams::GetBioSampleIds() const
{
    return m_imp->m_biosample_ids;
}

const TIdContainer& CParams::GetSRAIds() const
{
    return m_imp->m_sra_ids;
}

static std::unique_ptr<CParams> params;

const CParams& GetParams()
{
    if (!params) {
        params.reset(new CParams);
    }

    return *params;
}

static const size_t MIN_BIOPROJECT_ID_SIZE = 6;
static const size_t FIRST_BIOPROJECT_ID_DIGIT_INDEX = 6;

static bool IsValidBioProjectId(const string& id, char first_accession_char)
{
    if (id.empty()) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioProjectId, Error << "Empty BioProject accession number provided in command line.");
        return false;
    }

    // PRJ[D|E|N|][A-Z]\d+\d

    bool bad_format = false;
    if (id.size() < MIN_BIOPROJECT_ID_SIZE) {
        bad_format = true;
    }
    else if (!NStr::StartsWith(id, "PRJ") ||
             (id[3] != 'E' && id[3] != 'N' && id[3] != 'D') ||
             id[4] < 'A' || id[4] > 'Z' || !isdigit(id[5]))
             bad_format = true;
    else {

        bad_format = !IsDigits(id.begin() + FIRST_BIOPROJECT_ID_DIGIT_INDEX, id.end());
    }

    if (bad_format) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioProjectId, Error << "Incorrectly formatted BioProject accession number provided in command line: \"" << id << "\".");
        return false;
    }

    static const string ACCESSION_FIRST_LETTER = "ABCDEJLMN";

    if (ACCESSION_FIRST_LETTER.find_first_of(first_accession_char) != string::npos && id[3] == 'N' && id[4] == 'A') {
        return true;
    }

    static const string ACCESSION_FIRST_LETTER_N = "ADJLMN";
    static const string ACCESSION_FIRST_LETTER_D = "BE";
    static const string ACCESSION_FIRST_LETTER_E = "C";

    auto CheckAccFirstChar = [](const string& acc_first_letter, char acc_first)
                               {
                                   return acc_first_letter.find_first_of(acc_first) != string::npos;
                               };

    bad_format = 
        (CheckAccFirstChar(ACCESSION_FIRST_LETTER_N, first_accession_char) && id[3] != 'N') ||
        (CheckAccFirstChar(ACCESSION_FIRST_LETTER_D, first_accession_char) && id[3] != 'D') ||
        (CheckAccFirstChar(ACCESSION_FIRST_LETTER_E, first_accession_char) && id[3] != 'E');

    if (bad_format) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioProjectId, Warning << "BioProject accession number provided in command line does not match the source of the record: \"" << id << "\".");
    }

    return true;
}

static bool IsValidIdParam(const string& ids)
{
    for (auto ch : ids) {
        if (!isdigit(ch) && !(ch >= 'A' && ch <= 'Z') && ch != ',')
            return false;
    }

    return true;
}

static bool SetBioProjectIds(const list<string>& ids, TIdContainer& bioproject_ids, char first_accession_char)
{
    for (auto& id : ids) {
        if (!IsValidBioProjectId(id, first_accession_char)) {
            return false;
        }

        bioproject_ids.insert(id);
    }
    return true;
}

static bool SetBioSampleSRAIds(const list<string>& ids, TIdContainer& biosample_ids, TIdContainer& sra_ids)
{
    static const size_t MIN_ID_SIZE = 4;

    for (auto& id : ids) {

        if (id.size() < MIN_ID_SIZE) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioSampleId, Error << "Empty BioSample/SRA id number provided in command line.");
            return false;
        }

        if (IsValidSRA(id)) {
            sra_ids.insert(id);
        }
        else if (IsValidBiosample(id)) {
            biosample_ids.insert(id);
        }
        else {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioSampleId, Error << "Incorrectly formatted BioSample/SRA id number provided in command line: \"" << id << "\".");
            return false;
        }
    }

    return true;
}

enum EAccInvalid {
    eValidAcc,
    eInvalidAcc,
    eInvalidNewAccFirstChar
};

static EAccInvalid IsValidAccession(const string& accession)
{
    static const size_t ACC_MIN_LEN = 6;
    static const size_t NEW_ACC_NUM_OF_LETTERS = 6;
    static const size_t VALID_NUM_OF_LETTERS[] = {4, NEW_ACC_NUM_OF_LETTERS };
    static const auto VALID_NUM_OF_LETTERS_END = VALID_NUM_OF_LETTERS + sizeof(VALID_NUM_OF_LETTERS) / sizeof(VALID_NUM_OF_LETTERS[0]);

    if (accession.size() < ACC_MIN_LEN) {
        return eInvalidAcc;
    }

    string::const_iterator start = accession.begin();
    if (NStr::StartsWith(accession, "NZ_")) {
        start += ACC_PREFIX_PREFIX_LEN;
    }

    size_t num_of_letters = 0;
    for (; start != accession.end(); ++start) {
        if (!isalpha(*start)) {
            break;
        }
        ++num_of_letters;
    }

    if (find(VALID_NUM_OF_LETTERS, VALID_NUM_OF_LETTERS_END, num_of_letters) == VALID_NUM_OF_LETTERS_END) {
        return eInvalidAcc;
    }

    bool non_zero = false;
    size_t num_of_digits = 0;
    for (; start != accession.end(); ++start) {
        if (!isdigit(*start)) {
            return eInvalidAcc;
        }

        if (*start != '0') {
            non_zero = true;
        }
        ++num_of_digits;
    }

    EAccInvalid ret = (num_of_digits == ACC_NUM_OF_DIGITS && non_zero) ? eValidAcc : eInvalidAcc;

    if (ret == eValidAcc && num_of_letters == NEW_ACC_NUM_OF_LETTERS) {
        if (accession[0] < 'A' || accession[0] > 'D') {
            ret = eInvalidNewAccFirstChar;
        }
    }
    return ret;
}

static int GetMonthNumber(const string& month)
{
    static const map<string, int> MONTH_NUM =
    {
        {"JAN", 1}, {"FEB", 2}, {"MAR", 3}, {"APR", 4}, {"MAY", 5}, {"JUN", 6},
        {"JUL", 7}, {"AUG", 8}, {"SEP", 9}, {"OCT", 10}, {"NOV", 11}, {"DEC", 12}
    };

    auto it = MONTH_NUM.find(month);

    return it != MONTH_NUM.end() ? it->second : -1;
}

static bool ParseSubmissionDate(const string& str, CDate_std& date)
{
    static const size_t DATE_STR_LEN = 11;
    static const size_t FIRST_DASH_POS = 2;
    static const size_t SECOND_DASH_POS = 6;
    static const size_t MONTH_STR_LEN = 3;

    size_t len = str.size();
    if (len != DATE_STR_LEN) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (i == FIRST_DASH_POS || i == SECOND_DASH_POS) {
            if (str[i] != '-') {
                return false;
            }
        }
        else if ((i < FIRST_DASH_POS || i > SECOND_DASH_POS) && !isdigit(str[i])) {
            return false;
        }
    }

    int day = NStr::StringToInt(str.c_str(), NStr::fAllowTrailingSymbols),
        month = GetMonthNumber(str.substr(FIRST_DASH_POS + 1, MONTH_STR_LEN)),
        year = NStr::StringToInt(str.c_str() + SECOND_DASH_POS + 1);

    if (month < 1 || month > 12) {
        return false;
    }

    static const int DAYS_IN_MONTH[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (day < 1 || day > DAYS_IN_MONTH[month - 1]) {
        return false;
    }

    date.SetDay() = day;
    date.SetMonth() = month;
    date.SetYear() = year;
    return true;
}

static void BuildFilenameWithPath(const string& outdir, string& filename)
{
    if (filename.find('/') == string::npos &&
        filename.find('\\') == string::npos) { // does not contain path, just a filename
        filename = outdir + '/' + filename;
    }
}

static long long ToLongLong(string::const_iterator& it, string::const_iterator end)
{
    long long ret = 0;
    for (; it != end && isdigit(*it); ++it) {
        ret *= 10;
        ret += *it - '0';
    }

    return ret;
}

static bool FileNameCmp(const string& name1, const string& name2)
{
    auto it1 = name1.begin(),
         it2 = name2.begin();

    for (; it1 != name1.end() && it2 != name2.end(); ++it1, ++it2) {

        if (isdigit(*it1) && isdigit(*it2))
        {
            long long num1 = ToLongLong(it1, name1.end()),
                      num2 = ToLongLong(it2, name2.end());

            if (num1 != num2) {
                return num1 < num2;
            }

            if (it1 == name1.end() && it2 == name2.end()) {
                return false; // semantically equal strings, like 'abc01' and 'abc0001'
            }

            if (it1 == name1.end() || it2 == name2.end()) {
                return it1 == name1.end();
            }
        }

        if (*it1 != *it2) {
            return *it1 < *it2;
        }
    }

    return false;
}

static void ReportBioProjectProblem(const string& bioproject_ids)
{
    ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioProjectId, Error << "One or more BioProject accession numbers for this WGS/TSA project, provided in command line, is incorrect: \"" << bioproject_ids << "\".");
}

bool SetParams(const CArgs& args)
{
    if (!params) {
        GetParams();
    }

    CParams_imp& params_imp = *params->m_imp;

    params_imp.m_test = args["Z"].AsBoolean();
    if (!params_imp.m_test) {
        
        if (args["d"].HasValue()) {
            params_imp.m_outdir = args["d"].AsString();
            if (!params_imp.m_outdir.empty() && (params_imp.m_outdir.back() == '/' || params_imp.m_outdir.back() == '\\')) {
                params_imp.m_outdir.pop_back();
            }
        }

        if (params_imp.m_outdir.empty())
        {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_NoOutputDir, Fatal << "The name of top-level directory for output ASN.1s and master Bioseq must be provided with \"-d\" command line option.");
            return false;
        }
    }

    params_imp.m_update_mode = static_cast<EUpdateMode>(args["u"].AsInteger());
    params_imp.m_keep_refs = args["R"].AsBoolean();
    if (params_imp.m_keep_refs && params_imp.m_update_mode != eUpdateExtraContigs) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "Command line option \"-R\" is allowed to set to TRUE for \"EXTRA CONTIGS\" mode only (\"-u 6\").");
        return false;
    }

    params_imp.m_copy_cit_art_from_master = args["A"].AsBoolean();

    if (args["L"].HasValue()) {
        params_imp.m_id_acc_file = args["L"].AsString();
    }

    if (args["O"].HasValue()) {
        params_imp.m_load_order_file = args["O"].AsString();
    }

    if (args["e"].HasValue()) {
        params_imp.m_acc_file = args["e"].AsString();
        BuildFilenameWithPath(params_imp.m_outdir, params_imp.m_acc_file);
    }

    if (!params_imp.m_acc_file.empty() && params_imp.m_update_mode != eUpdateAssembly) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_AccsSaveWithNoAssemblyUpdate, Error << "File with the list of accessions of previous assembly version can be generated in UPDATE ASSEMBLY mode only.");
        return false;
    }

    if (!args["a"].HasValue()) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_NoInputAccession, Fatal << "Accession prefix+version must be provided with \"-a\" command line option.");
        return false;
    }

    string accession = args["a"].AsString();
    params_imp.SetAccession(NStr::ToUpper(accession));

    EAccInvalid validity = IsValidAccession(params_imp.m_accession);
    if (validity != eValidAcc) {

        if (validity == eInvalidAcc) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectInputAccession, Fatal << "Incorrect accession provided on input: \"" << params_imp.m_accession << "\". Must be 4 or 6 letters + 2 digits (Not 00) or 2 letters + underscore + 4 or 6 letters + 2 digits.");
        }
        else if (validity == eInvalidNewAccFirstChar) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectInputAccession, Fatal << "Incorrect new format (6+2) accession prefix provided on input: \"" << params_imp.m_accession << "\". Must begin with 'A', 'B', 'C' or 'D' letter.");
        }
        return false;
    }

    params_imp.m_accs_assigned = args["c"].AsBoolean();

    if ((params->GetSource() == eDDBJ || params->GetSource() == eEMBL) && !params_imp.m_accs_assigned) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "For DDBJ and EMBL data must use \"-c T\" switch because they always have accessions pre-assigned.");
        return false;
    }

    if (args["K"].HasValue()) {
        params_imp.m_tpa_keyword = args["K"].AsString();
    }

    static const string TPA_FIRST_LETTER = "DE";
    params_imp.m_tpa = TPA_FIRST_LETTER.find_first_of(params_imp.m_accession.front()) != string::npos;

    if (!params_imp.m_tpa_keyword.empty()) {
        if (!params->IsTpa()) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "TPA keyword may be entered with \"-K\" switch for TPA projects only.");
            return false;
        }

        if (params_imp.m_tpa_keyword != "TPA:assembly" && params_imp.m_tpa_keyword != "TPA:experimental") {
            ERR_POST_EX(ERR_TPA, ERR_TPA_InvalidKeyword, Fatal << "Invalid TPA keyword provided via \"-K\" switch: \"" << params_imp.m_tpa_keyword << "\".");
            return false;
        }
    }

    string bioproject_ids;
    if (args["B"].HasValue()) {
        bioproject_ids = args["B"].AsString();
    }

    char first_accession_char = params_imp.m_accession[0];
    if (!bioproject_ids.empty()) {

        if (!IsValidIdParam(bioproject_ids)) {
            ReportBioProjectProblem(bioproject_ids);
        }
        else {
            list<string> ids;
            NStr::Split(bioproject_ids, ",", ids);

            if (!SetBioProjectIds(ids, params_imp.m_bioproject_ids, first_accession_char)) {
                ReportBioProjectProblem(bioproject_ids);
                return false;
            }
        }
    }

    string biosample_ids;
    if (args["C"].HasValue()) {
        biosample_ids = args["C"].AsString();
    }

    if (!biosample_ids.empty()) {

        if (!IsValidIdParam(biosample_ids)) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBioSampleId, Error << "One or more BioSample/SRA id numbers for this WGS/TSA project, provided in command line, is incorrect: \"" << biosample_ids << "\".");
        }
        else {
            list<string> ids;
            NStr::Split(biosample_ids, ",", ids);

            if (!SetBioSampleSRAIds(ids, params_imp.m_biosample_ids, params_imp.m_sra_ids) ||
                (params_imp.m_biosample_ids.empty() && params_imp.m_sra_ids.empty())) {
                return false;
            }
        }
    }

    params_imp.m_different_biosamples = args["D"].AsBoolean();

    if (params_imp.m_different_biosamples) {

        if (!params_imp.m_biosample_ids.empty()) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_ConflictingArguments, Warning << "Ignoring Biosample ids entered via \"-C\" command line switch because \"-D T\" is set.");
        }
        params_imp.m_biosample_ids.clear();

        if (!params->IsTls() && params->GetUpdateMode() != eUpdateNew) {

            ERR_POST_EX(ERR_INPUT, ERR_INPUT_ConflictingArguments, Fatal << "Different DBLinks (switch \"-D\") are allowed for brand new TLS projects only.");
            return false;
        }
    }

    params_imp.m_scaffold_type = static_cast<EScaffoldType>(args["j"].AsInteger());

    if (params_imp.m_scaffold_type != eRegularGenomic && params_imp.m_update_mode != eUpdateScaffoldsNew &&
        params_imp.m_update_mode != eUpdateScaffoldsUpd) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "Command line option \"-j\" is allowed to set to non-zero value for scaffold modes only (\"-u 3\" or \"-u 5\").");
        return false;
    }

    switch (params_imp.m_scaffold_type) {
        case eRegularChromosomal:
        case eGenColGenomic:
            if (params->IsTpa()) {
                ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "Incorrect \"-j\" selection for non-TPA scaffolds.");
                return false;
            }
            break;

        case eTPAGenomic:
        case eTPAChromosomal:
            if (!params->IsTpa()) {
                ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "Incorrect \"-j\" selection for TPA scaffolds.");
                return false;
            }
            break;

        default:; // do nothing
    }

    params_imp.m_trust_version = args["V"].AsBoolean();
    if (params_imp.m_trust_version) {
        ERR_POST_EX(ERR_ACCESSION, ERR_ACCESSION_ForcedAssemblyVersion, Warning << "Forcing the use of assembly-version number \"" << params->GetMajorAssemblyVersion() << params->GetMinorAssemblyVersion() <<
                    "\", regardless of what is (or is not) currently in ID. Hopefully you have a very good reason to do this!");
    }
    else {
        if (params->GetUpdateMode() == eUpdateNew && params->GetAssemblyVersion() != 1) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectInputAccession, Fatal << "Incorrect accession version provided on input: \"" << params_imp.m_accession << "\". Must be \"01\" for brand new projects.");
            return false;
        }
    }

    params_imp.m_allow_diff_citsubs = args["v"].AsBoolean();

    string submission_date;
    if (args["s"].HasValue()) {
        submission_date = args["s"].AsString();
    }

    if (!submission_date.empty()) {
        if (params_imp.m_allow_diff_citsubs) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "It is not allowed to use \"-s\" and \"-v\" command line options altogether.");
            return false;
        }

        if (!ParseSubmissionDate(submission_date, params_imp.m_submission_date)) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_BadSubmissionDate, Fatal << "Incorrect date of submission, provided with \"-s\" command line option.");
            return false;
        }

    }

    params_imp.m_sort_order = static_cast<ESortOrder>(args["o"].AsInteger());
    params_imp.m_vdb_mode = args["U"].AsBoolean();
    if (params_imp.m_vdb_mode) {
        params_imp.m_sort_order = eUnsorted;
    }

    params_imp.m_preserve_input_path = args["I"].AsBoolean();

    if (args["i"].HasValue() && args["f"].HasValue()) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_InputFileArgsConflict, Fatal << "Command line agruments \"-i\" and \"-f\" cannot be used together. Only one of them is allowed.");
        return false;
    }

    string input_mask;
    if (args["i"].HasValue()) {
        input_mask = args["i"].AsString();

#ifndef WIN32
        if (!input_mask.empty() && input_mask.front() != '/' && input_mask.front() != '~') {
            input_mask = "./" + input_mask;
        }
#endif
    }

    string file_list;
    if (args["f"].HasValue()) {
        file_list = args["f"].AsString();
    }

    if (input_mask.empty() && file_list.empty()) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_NoInputFiles, Fatal << "Input file names are missing from command line or empty. Please use \"-i\" or \"-f\" arguments.");
        return false;
    }

    if (!input_mask.empty()) {
        if (!GetFilesFromDir(input_mask, params_imp.m_file_list)) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_NoMatchingInputData, Fatal << "No input files matching input \"" << input_mask << "\" have been found.");
            return false;
        }

        if (params_imp.m_sort_order == eUnsorted) {
            params_imp.m_file_list.sort(FileNameCmp);
        }
    }
    else {
        if (!GetFilesFromFile(file_list, params_imp.m_file_list)) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_NoInputNamesInFile, Fatal << "File with input SeqSubmit names, given by \"-f\" command line option, is not readable or empty: \"" << file_list << "\".");
            return false;
        }
    }

    string dup_name;
    if (!params_imp.m_preserve_input_path && IsDupFileNames(params_imp.m_file_list, dup_name)) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_DuplicatedInputFileNames, Fatal << "Found duplicated names of input files to be processed: \"" << dup_name << "\". Cannot proceed.");
        return false;
    }


    if (!params_imp.m_test && !MakeDir(params_imp.m_outdir)) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_CreateDirFail, Fatal << "Failed to create top-level directory \"" << params_imp.m_outdir << "\" for output ASN.1s and master Bioseq.");
        return false;
    }

    string input_type = args["y"].AsString();
    if (!GetInputType(input_type, params_imp.m_input_type)) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectInputDataType, Fatal << "Unknown type of input data provided: \"" << input_type << "\".");
        return false;
    }

    params_imp.m_gap_size = args["q"].AsInteger();

    params_imp.m_ignore_general_ids = args["g"].AsBoolean();
    params_imp.m_binary_in = args["b"].AsBoolean();
    params_imp.m_binary_out = args["p"].AsBoolean();
    params_imp.m_override_existing = args["w"].AsBoolean();
    params_imp.m_taxonomy_lookup = args["t"].AsBoolean();
    params_imp.m_replace_dbname = args["r"].AsBoolean();

    params_imp.m_tpa_tsa = args["J"].AsBoolean();
    if(params_imp.m_tpa_tsa && params_imp.m_accession.front() != 'D')
    {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_CommandLineOptionsMisuse, Fatal << "For TPA-TSA projects (\"-J T\" setting) accession prefix MUST start with \"D\", not \"" << params_imp.m_accession.front() << "\".");
        return false;
    }

    params_imp.m_vdb_mode = args["U"].AsBoolean();
    if (params_imp.m_vdb_mode &&
        params_imp.m_update_mode != eUpdateAssembly &&
        params_imp.m_update_mode != eUpdateNew &&
        params_imp.m_update_mode != eUpdatePartial &&
        params_imp.m_update_mode != eUpdateFull) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_ConflictingArguments, Fatal << "VDB parsing mode (\"-U T\") can be used for brand new projects (\"-u 0\") or reassemblies (\"-u 2\") only.");
        return false;
    }

    if (args["x"].AsBoolean()) {
        params_imp.m_fix_tech = eFixMolBiomol;
    }

    string tsa_biomol;
    
    if (args["T"].HasValue()) {
        tsa_biomol = args["T"].AsString();
    }

    if (!tsa_biomol.empty()) {

        if (!params->IsTsa() && !params->IsWgs()) {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_BiomolNotSupported, Fatal << "Supplying a Biomol value (mRNA, ncRNA, etc) via the \"-T\" command line switch is supported for TSA and WGS projects only.");
            return false;
        }

        if(params->IsTsa())
        {
            if(NStr::EqualNocase(tsa_biomol, "mRNA")) {
                params_imp.m_fix_tech |= eFixBiomol_mRNA;
            }
            else if(NStr::EqualNocase(tsa_biomol, "rRNA"))  {
                params_imp.m_fix_tech |= eFixBiomol_rRNA;
            }
            else if(NStr::EqualNocase(tsa_biomol, "ncRNA")) {
                params_imp.m_fix_tech |= eFixBiomol_ncRNA;
            }
            else {
                ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBiomolTypeSupplied, Fatal << "Incorrect biomol type for TSA project provided via \"-T\" command line switch. Valid ones are (case sensitive): \"mRNA\", \"rRNA\" and \"ncRNA\".");
                return false;
            }
        }
        else
        {
            if(NStr::EqualNocase(tsa_biomol, "cRNA"))
            {
                params_imp.m_fix_tech |= eFixBiomol_cRNA;
            }
            else
            {
                ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectBiomolTypeSupplied, Fatal << "Incorrect biomol type for WGS project provided via \"-T\" command line switch. Valid one is (case insensitive): \"cRNA\".");
                return false;
            }
        }
    }

    string moltype;
    
    if(args["E"].HasValue())
    {
        moltype = args["E"].AsString();
    }

    if(!moltype.empty())
    {
        if(!params->IsWgs())
        {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_MolTypeNotSupported, Fatal << "Supplying a Biomol value (cRNA) via the \"-E\" command line switch is supported for WGS projects only.");
            return false;
        }
        if(NStr::EqualNocase(moltype, "rna"))
        {
            params_imp.m_fix_tech |= eFixInstMolRNA;
        }
        else
        {
            ERR_POST_EX(ERR_INPUT, ERR_INPUT_IncorrectMolTypeSupplied, Fatal << "Incorrect Seq-inst.mol type for WGS project provided via \"-E\" command line switch. Valid one is (case sensitive): \"rna\".");
            return false;
        }
    }

    if (args["n"].HasValue()) {
        params_imp.m_change_nuc_title = true;
        params_imp.m_new_nuc_title = args["n"].AsString();
    }

    params_imp.m_accessions_sorted_in_file = args["k"].AsBoolean();
    params_imp.m_scfld_test_mode = args["z"].AsBoolean();
    params_imp.m_force_gcode = args["h"].AsBoolean();
    params_imp.m_strip_authors = args["P"].AsBoolean();
    params_imp.m_allow_secondary_accession = args["S"].AsBoolean();
    params_imp.m_dblink_override = args["X"].AsBoolean();

    if (params_imp.m_vdb_mode) {
        params_imp.m_accessions_sorted_in_file = false;
        params_imp.m_id_acc_file.clear();
    }

    if (params_imp.m_sort_order == eById && params_imp.m_ignore_general_ids) {
        ERR_POST_EX(ERR_INPUT, ERR_INPUT_ConflictingArguments, Fatal << "Cannot assign accessions in sorted by contig/scaffold id order (\"-o 3\") while ignoring general ids flag is set (\"-g T\").");
        return false;
    }

    if (params_imp.m_update_mode == eUpdateScaffoldsUpd) {
        params_imp.m_accs_assigned = true;
    }

    params_imp.m_medline_lookup = args["m"].AsBoolean();

    if (args["F"].HasValue()) {
        params_imp.m_master_file = args["F"].AsString();

        if (!input_mask.empty()) {
            auto slash = input_mask.find_last_of('/');
            if (slash == string::npos) {
                slash = input_mask.find_last_of('\\');
            }

            if (slash != string::npos) {
                BuildFilenameWithPath(input_mask.substr(0, slash), params_imp.m_master_file);
            }
        }
    }

    params_imp.m_enforce_new = args["N"].AsBoolean();

    return true;
}

void SetScaffoldPrefix(const string& scaffold_prefix)
{
    CParams_imp& params_imp = *params->m_imp;
    if (params_imp.m_scaffold_prefix.empty()) {
        params_imp.m_scaffold_prefix = scaffold_prefix;
    }
}

void SetAssemblyVersion(int version)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_accession[params_imp.m_minor_version_pos] = (version % 10) + '0';
    params_imp.m_accession[params_imp.m_major_version_pos] = (version / 10) + '0';

    params_imp.m_proj_acc.clear();
    params_imp.m_proj_acc_ver.clear();
}

void SetUpdateMode(EUpdateMode mode)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_update_mode = mode;
}

void AddBioSample(const string& biosample)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_biosample_ids.insert(biosample);
}

void AddSRA(const string& sra)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_sra_ids.insert(sra);
}

void SetTpa(bool tpa)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_tpa = tpa;
}

void SetReplaceDBName(bool replace_db_name)
{
    CParams_imp& params_imp = *params->m_imp;
    params_imp.m_replace_dbname = replace_db_name;
}

}
