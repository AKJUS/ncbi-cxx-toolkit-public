
#include <ncbi_pch.hpp>

#include <vector>
#include <sstream>
#include <climits>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

enum class job_t {
    jb_lookup_bi_primary,
    jb_lookup_bi_secondary,
    jb_lookup_primary_secondary,
    jb_lookup_blob_prop,
    jb_unpack_bi_key,
    jb_unpack_si_key,
    jb_unpack_bp_key,
};

class CTestPsgCache
    : public CNcbiApplication
{
 public:
    CTestPsgCache()
        : m_job(job_t::jb_lookup_bi_primary)
        , m_force_version{}
        , m_force_seq_id_type{}
    {}
    virtual void Init();
    virtual int Run();

 protected:
    void ParseArgs();

 private:
    bool ParsePrimarySeqId(const string& fasta_seqid, string& accession, int& version, int& seq_id_type);
    bool ParseSecondarySeqId(const string& fasta_seqid, string& seq_id_str, int& seq_id_type);
    void PrintBioseqInfo(
        bool lookup_res, const string& accession, int version, int seq_id_type, int64_t gi, const string& data);
    void PrintPrimaryId(bool lookup_res, const string& seq_id, int seq_id_type, const string& data);
    void PrintBlobProp(bool lookup_res, int sat, int sat_key, int64_t last_modified, const string& data);
    void LookupBioseqInfoByPrimary(const string& fasta_seqid, int force_version, int force_seq_id_type);
    void LookupBioseqInfoByPrimaryAVT(const string& accession, int version, int seq_id_type);
    void LookupBioseqInfoBySecondary(const string& fasta_seqid, int force_seq_id_type);
    void LookupPrimaryBySecondary(const string& fasta_seqid, int force_seq_id_type);
    void LookupBlobProp(int sat, int sat_key, int64_t last_modified = -1);
    vector<string> m_SatNames;
    string m_BioseqInfoDbFile;
    string m_Si2csiDbFile;
    string m_BlobPropDbFile;
    unique_ptr<CPubseqGatewayCache> m_LookupCache;
    job_t m_job;
    string m_query;
    int m_force_version;
    int m_force_seq_id_type;
};

bool IsHex(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

char HexToBin(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

string PrintableToHext(const string& str)
{
    stringstream ss;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 2 < str.size() && IsHex(str[i + 1]) && IsHex(str[i + 2])) {
            char ch = HexToBin(str[i + 1]) * 16 + HexToBin(str[i + 2]);
            i += 2;
            ss << ch;
        } else {
            ss << str[i];
        }
    }
    return ss.str();
}


void CTestPsgCache::Init()
{
	unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
	argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "test PSG cache");

	argdesc->AddDefaultKey("ini", "IniFile", "File with configuration information", CArgDescriptions::eString, "test_psg_cache.ini");
    argdesc->AddOptionalKey("j", "job", "Job type (bi_pri|bi_sec|si2csi|blob_prop|unp_bi|unp_si|unp_bp)", CArgDescriptions::eString);
    argdesc->AddKey("q", "query", "Query string (depends on job type)", CArgDescriptions::eString);
    argdesc->AddDefaultKey("v", "ver", "Force version", CArgDescriptions::eInteger, to_string(INT_MIN));
    argdesc->AddDefaultKey("t", "seqidtype", "Force seq_id_type", CArgDescriptions::eInteger, to_string(INT_MIN));
    SetupArgDescriptions(argdesc.release());
}

void CTestPsgCache::ParseArgs() {
    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   registry = GetConfig();

    map<string, job_t> jm({
        { "bi_pri",    job_t::jb_lookup_bi_primary },
        { "bi_sec",    job_t::jb_lookup_bi_secondary },
        { "si2csi",    job_t::jb_lookup_primary_secondary },
        { "blob_prop", job_t::jb_lookup_blob_prop },
        { "unp_bi",    job_t::jb_unpack_bi_key    },
        { "unp_si",    job_t::jb_unpack_si_key    },
        { "unp_bp",    job_t::jb_unpack_bp_key    },
    });

    m_Si2csiDbFile = registry.GetString("LMDB_CACHE", "dbfile_si2csi", "");
    m_BioseqInfoDbFile = registry.GetString("LMDB_CACHE", "dbfile_bioseq_info", "");
    m_BlobPropDbFile = registry.GetString("LMDB_CACHE", "dbfile_blob_prop", "");

    if (args["j"]) {
        auto it = jm.find(args["j"].AsString());
        if (it == jm.end())
            NCBI_THROW(CException, eInvalid, "Unsupported job argument");
        m_job = it->second;
    }
    m_query = args["q"].AsString();
    m_force_version = args["v"].AsInteger();
    m_force_seq_id_type = args["t"].AsInteger();

}

static string GetListOfSeqIds(
    const ::google::protobuf::RepeatedPtrField<::psg::retrieval::BioseqInfoValue_SecondaryId>& seq_ids
)
{
    stringstream ss;
    bool empty = true;
    for (const auto& it : seq_ids) {
        if (!empty) {
            ss << ", ";
        }
        ss << "{" << it.sec_seq_id_type() << ", " << it.sec_seq_id() << "}";
        empty = false;
    }
    return ss.str();
}

int CTestPsgCache::Run() {
    ParseArgs();

    m_SatNames.push_back("");                  // 0
    m_SatNames.push_back("");                  // 1
    m_SatNames.push_back("");                  // 2
    m_SatNames.push_back("");                  // 3
    m_SatNames.push_back("satncbi_extended");  // 4
    m_LookupCache.reset(new CPubseqGatewayCache(m_BioseqInfoDbFile, m_Si2csiDbFile, m_BlobPropDbFile));
    m_LookupCache->Open(m_SatNames);


    switch (m_job) {
        case job_t::jb_lookup_bi_primary: {
            LookupBioseqInfoByPrimary(m_query, m_force_version, m_force_seq_id_type);
            break;
        }
        case job_t::jb_lookup_bi_secondary: {
            LookupBioseqInfoBySecondary(m_query, m_force_seq_id_type);
            break;
        }
        case job_t::jb_lookup_primary_secondary: {
            LookupPrimaryBySecondary(m_query, m_force_seq_id_type);
            break;
        }
        case job_t::jb_lookup_blob_prop: {
            int sat = -1;
            int sat_key = -1;
            int64_t last_modified = -1;
            list<string> list;
            NStr::Split(m_query, ", :", list, NStr::ESplitFlags::fSplit_MergeDelimiters | NStr::ESplitFlags::fSplit_Tokenize);
            if (list.size() >= 2) {
                auto it = list.begin();
                sat = stoi(*it);
                ++it;
                sat_key = stoi(*it);
                if (list.size() >= 3) {
                    ++it;
                    last_modified = stoi(*it);
                }
                LookupBlobProp(sat, sat_key, last_modified);
            } else {
                ERR_POST(Error << "Query parameter expected: sat,sat_key(,last_modified) ");
            }

            break;
        }
        case job_t::jb_unpack_bi_key: {
            string s = PrintableToHext(m_query);
            int version = -1;
            int seq_id_type = -1;
            int64_t gi = -1;
            string accession;
            CPubseqGatewayCache::UnpackBioseqInfoKey(s.c_str(), s.size(), accession, version, seq_id_type, gi);
            cout << accession << "." << version << "/" << seq_id_type << ":" << gi << endl;
            break;
        }
        case job_t::jb_unpack_si_key: {
            string s = PrintableToHext(m_query);
            int seq_id_type = -1;
            string seq_id;
            CPubseqGatewayCache::UnpackSiKey(s.c_str(), s.size(), seq_id_type);
            cout << seq_id << "/" << seq_id_type << endl;
            break;
        }
        case job_t::jb_unpack_bp_key: {
            string s = PrintableToHext(m_query);
            int64_t last_modified = -1;
            int32_t sat_key = -1;
            CPubseqGatewayCache::UnpackBlobPropKey(s.c_str(), s.size(), last_modified, sat_key);
            cout << sat_key << "/" << last_modified << endl;
            break;
        }
    }
    return 0;
}

bool CTestPsgCache::ParsePrimarySeqId(const string& fasta_seqid, string& accession, int& version, int& seq_id_type) {
    const CTextseq_id *tx_id = nullptr;
    try {
        CSeq_id seq_id(fasta_seqid);
        seq_id_type = seq_id.Which() == CSeq_id::e_not_set  ? -1 : static_cast<int>(seq_id.Which());
        tx_id = seq_id.GetTextseq_Id();
        if (seq_id_type != static_cast<int>(CSeq_id::e_Gi)) {
            if (tx_id) {
                if (tx_id->IsSetAccession()) {
                    accession = tx_id->GetAccession();
                    if (tx_id->IsSetVersion()) {
                        version = tx_id->GetVersion();
                    } else {
                        version = -1;
                    }
                } else if (tx_id->IsSetName()) {
                    accession = tx_id->GetName();
                }
            }
        }
    }
    catch (const exception& e) {
        ERR_POST(Error << "Failed to parse seqid: " << fasta_seqid
            << ", exception thrown: " << e.what());
        return false;
    }

    if (accession.empty()) {
        ERR_POST(Error << "Provided SeqId \"" << fasta_seqid
             << "\" is not recognized as primary. A primary would have accession[dot version]. "
             << "In order to resolve secondary identifier, use -j=bi_secondary");
        return false;
    }

    return true;
}

bool CTestPsgCache::ParseSecondarySeqId(const string& fasta_seqid, string& seq_id_str, int& seq_id_type) {
    const CTextseq_id *tx_id = nullptr;
    try {
        CSeq_id seq_id(fasta_seqid);
        seq_id_type = seq_id.Which() == CSeq_id::e_not_set  ? -1 : static_cast<int>(seq_id.Which());
        if (seq_id_type != static_cast<int>(CSeq_id::e_Gi)) {
/*
            if (seq_id_type == static_cast<int>(CSeq_id::e_General)) {
                auto gen = seq_id.GetGeneral();
                if (gen) {
                    if (gen->IsSetName)
                        seq_id_str = gen->GetName();
                    if (gen->IsSetDb())  {
                        if (!seq_id_str.empty())
                            seq_id_str = "|" + seq_id_str;
                        seq_id_str = gen->GetDb() + seq_id_str;
                    }

                }
            }
            else {
*/
                tx_id = seq_id.GetTextseq_Id();
                if (tx_id) {
                    if (tx_id->IsSetAccession()) {
                        seq_id_str = tx_id->GetAccession();
                        if (tx_id->IsSetVersion())
                            seq_id_str = seq_id_str + "." + to_string(tx_id->GetVersion());
                    } else if (tx_id->IsSetName()) {
                        seq_id_str = tx_id->GetName();
                    }
                }
//            }
        } else {
            seq_id_str = NStr::NumericToString(seq_id.GetGi());
        }
    }
    catch (const exception& e) {
        ERR_POST(Error << "Failed to parse seqid: " << fasta_seqid
            << ", exception thrown: " << e.what());
        return false;
    }

    if (seq_id_str.empty()) {
        ERR_POST(Error << "Provided SeqId \"" << fasta_seqid << "\" is not recognized as secondary. A secondary would be numeric GI or fasta name");
        return false;
    }

    return true;

}

void CTestPsgCache::PrintBioseqInfo(bool lookup_res, const string& accession, int version, int seq_id_type, int64_t gi, const string& data) {
    if (lookup_res) {
        ::psg::retrieval::BioseqInfoValue value;
        bool b = value.ParseFromString(data);
        if (b) {
            cout << "result: bioseq_info cache hit" << endl
                 << "accession: " << accession << endl
                 << "version: " << version << endl
                 << "seq_id_type: " << seq_id_type << endl
                 << "gi: " << gi << endl
                 << "sat: " << value.blob_key().sat() << endl
                 << "sat_key: " << value.blob_key().sat_key() << endl
                 << "state: " << value.state() << endl
                 << "mol: " << value.mol() << endl
                 << "hash: " << value.hash() << endl
                 << "length: " << value.length() << endl
                 << "date_changed: " << value.date_changed() << endl
                 << "tax_id: " << value.tax_id() << endl
                 << "seq_ids: {" << GetListOfSeqIds(value.seq_ids()) << "}" << endl;
        } else {
            cout << "result: bioseq_info cache error: data corrupted" << endl;
        }
    } else {
        cout << "result: bioseq_info cache miss" << endl;
    }
}

void CTestPsgCache::PrintPrimaryId(bool lookup_res, const string& seq_id, int seq_id_type, const string& data) {
    if (lookup_res) {
        ::psg::retrieval::BioseqInfoKey value;
        bool b = value.ParseFromString(data);
        if (b) {
            cout << "result: si2csi cache hit" << endl
                 << "sec_seq_id: " << seq_id << endl
                 << "sec_seq_id_type: " << seq_id_type << endl
                 << "accession: " << value.accession() << endl
                 << "version: " << value.version() << endl
                 << "seq_id_type: " << value.seq_id_type() << endl
                 << "gi: " << value.gi() << endl;
        } else {
            cout << "result: si2csi cache error: data corrupted" << endl;
        }
    } else {
        cout << "result: si2csi cache miss" << endl;
    }
}

void CTestPsgCache::PrintBlobProp(bool lookup_res, int sat, int sat_key, int64_t last_modified, const string& data) {
    if (lookup_res) {
        ::psg::retrieval::BlobPropValue value;
        bool b = value.ParseFromString(data);
        if (b) {
            cout << "result: blob_prop cache hit" << endl
                 << "sat: " << sat << endl
                 << "sat_key: " << sat_key << endl
                 << "last_modified: " << last_modified << endl
                 << "class: " << value.class_() << endl
                 << "date_asn1: " << value.date_asn1() << endl
                 << "div: " << value.div() << endl
                 << "flags: " << value.flags() << endl
                 << "hup_date: " << value.hup_date() << endl
                 << "id2_info: " << value.id2_info() << endl
                 << "n_chunks: " << value.n_chunks() << endl
                 << "owner: " << value.owner() << endl
                 << "size: " << value.size() << endl
                 << "size_unpacked: " << value.size_unpacked() << endl
                 << "username: " << value.username() << endl;
        } else {
            cout << "result: blob_prop cache error: data corrupted" << endl;
        }
    } else {
        cout << "result: blob_prop cache miss" << endl;
    }
}

void CTestPsgCache::LookupBioseqInfoByPrimary(const string& fasta_seqid, int force_version, int force_seq_id_type) {
    int version = -1;
    int seq_id_type = -1;
    string accession;

    bool res = ParsePrimarySeqId(fasta_seqid, accession, version, seq_id_type);
    cout << "Accession: '" << accession << "' , version: " << version << ", seq_id_type: " << seq_id_type << endl;
    if (!res) {
        return;
    }

    if (force_version != INT_MIN) {
        version = force_version;
    }

    if (force_seq_id_type != INT_MIN) {
        seq_id_type = force_seq_id_type;
    }

    LookupBioseqInfoByPrimaryAVT(accession, version, seq_id_type);
}

void CTestPsgCache::LookupBioseqInfoByPrimaryAVT(const string& accession, int version, int seq_id_type) {
    string data;
    bool res = false;
    int64_t gi = -1;
    if (version >= 0) {
        if (seq_id_type >= 0) {
            res = m_LookupCache->LookupBioseqInfoByAccessionVersionSeqIdType(accession, version, seq_id_type, data, version, seq_id_type, gi);
        } else {
            res = m_LookupCache->LookupBioseqInfoByAccessionVersion(accession, version, data, seq_id_type, gi);
        }
    } else {
        if (seq_id_type >= 0)
            res = m_LookupCache->LookupBioseqInfoByAccessionVersionSeqIdType(accession, -1, seq_id_type, data, version, seq_id_type, gi);
        else
            res = m_LookupCache->LookupBioseqInfoByAccession(accession, data, version, seq_id_type, gi);
    }
    PrintBioseqInfo(res, accession, version, seq_id_type, gi, data);
}

void CTestPsgCache::LookupBioseqInfoBySecondary(const string& fasta_seqid, int force_seq_id_type) {
    int seq_id_type = -1;
    string seq_id;
    string data;

    bool res = ParseSecondarySeqId(fasta_seqid, seq_id, seq_id_type);
    if (!res) {
        // fallback to direct lookup
        seq_id = fasta_seqid;
        seq_id_type = -1;
    }

    if (force_seq_id_type != INT_MIN) {
        seq_id_type = force_seq_id_type;
    }

    if (seq_id_type >= 0) {
        res = m_LookupCache->LookupCsiBySeqIdSeqIdType(seq_id, seq_id_type, data);
    } else {
        res = m_LookupCache->LookupCsiBySeqId(seq_id, seq_id_type, data);
    }

    if (!res) {
        cout << "result: si2csi cache miss" << endl;
    } else {
        ::psg::retrieval::BioseqInfoKey value;
        bool b = value.ParseFromString(data);
        if (b) {
            LookupBioseqInfoByPrimaryAVT(value.accession(), value.version(), value.seq_id_type());
        } else {
            cout << "result: si2csi cache error: data corrupted" << endl;
        }
    }

}

void CTestPsgCache::LookupPrimaryBySecondary(const string& fasta_seqid, int force_seq_id_type) {
    int seq_id_type = -1;
    string seq_id;
    string data;

    bool res = ParseSecondarySeqId(fasta_seqid, seq_id, seq_id_type);
    if (!res) {
        // fallback to direct lookup
        seq_id = fasta_seqid;
        seq_id_type = -1;
    }

    if (force_seq_id_type != INT_MIN)
        seq_id_type = force_seq_id_type;


    if (seq_id_type >= 0)
        res = m_LookupCache->LookupCsiBySeqIdSeqIdType(seq_id, seq_id_type, data);
    else
        res = m_LookupCache->LookupCsiBySeqId(seq_id, seq_id_type, data);

    PrintPrimaryId(res, seq_id, seq_id_type, data);
}

void CTestPsgCache::LookupBlobProp(int sat, int sat_key, int64_t last_modified) {
    bool res;
    string data;
    if (last_modified > 0)
        res = m_LookupCache->LookupBlobPropBySatKeyLastModified(sat, sat_key, last_modified, data);
    else
        res = m_LookupCache->LookupBlobPropBySatKey(sat, sat_key, last_modified, data);
    PrintBlobProp(res, sat, sat_key, last_modified, data);
}

int main(int argc, const char* argv[]) {
    return CTestPsgCache().AppMain(argc, argv);
}

/*
-j=bi_pri -q=NC_000852
    result: bioseq_info cache hit
    accession: NC_000852
    version: -1
    seq_id_type: 10
    sat: 4
    sat_key: 79895203
    state: 10
    mol: 1
    hash: -1714995068
    length: 330611
    date_changed: 1345755420000
    tax_id: 10506
    seq_ids: {{11, 14116}, {12, 340025671}}


-j=bi_pri -q=NC_000852.3
    result: bioseq_info cache hit
    accession: NC_000852
    version: 3
    seq_id_type: 10
    sat: 4
    sat_key: 13131352
    state: 0
    mol: 1
    hash: -69310498
    length: 330743
    date_changed: 1176933360000
    tax_id: 10506
    seq_ids: {{12, 52353967}}

-j=bi_pri -q=NC_000852.4
    result: bioseq_info cache hit
    accession: NC_000852
    version: 4
    seq_id_type: 10
    sat: 4
    sat_key: 47961402
    state: 0
    mol: 1
    hash: -1254382679
    length: 330743
    date_changed: 1310747580000
    tax_id: 10506
    seq_ids: {{12, 145309287}}

-j=bi_pri -q=NC_000852.5
    result: bioseq_info cache hit
    accession: NC_000852
    version: 5
    seq_id_type: 10
    sat: 4
    sat_key: 79895203
    state: 10
    mol: 1
    hash: -1714995068
    length: 330611
    date_changed: 1345755420000
    tax_id: 10506
    seq_ids: {{12, 340025671}, {11, 14116}}

-j=bi_pri -q="ref|NC_000852.4"
    result: bioseq_info cache hit
    accession: NC_000852
    version: 4
    seq_id_type: 10
    sat: 4
    sat_key: 47961402
    state: 0
    mol: 1
    hash: -1254382679
    length: 330743
    date_changed: 1310747580000
    tax_id: 10506
    seq_ids: {{11, NCBI_GENOMES|14116}, {12, 145309287}}


    CPsgSi2CsiCache si_cache("/data/cassandra/NEWRETRIEVAL/si2csi.db");
    rv = si_cache.LookupBySeqId("1.METASSY.1|METASSY_00010", id_type, data);
    cout << "LookupBySeqId: 1.METASSY.1|METASSY_00010: " << id_type << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqId("14116", id_type, data);
    cout << "LookupBySeqId: 14116: " << id_type << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("11693124", 12, data);
    cout << "LookupBySeqIdIdType: 11693124-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("145309287", 12, data);
    cout << "LookupBySeqIdIdType: 145309287-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("52353967", 12, data);
    cout << "LookupBySeqIdIdType: 52353967-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    cout << endl << "===bp_cache===" << endl;

    int64_t last_modified;
    CPsgBlobPropCache bp_cache("/data/cassandra/NEWRETRIEVAL/blob_prop_sat_ncbi.db");

    rv = bp_cache.LookupBySatKey(142824422, last_modified, data);
    cout << "LookupBySatKey: 142824422: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKey(189880732, last_modified, data);
    cout << "LookupBySatKey: 189880732: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKey(24084221, last_modified, data);
    cout << "LookupBySatKey: 24084221: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKeyLastModified(24084221, 1530420864303, data);
    cout << "LookupBySatKeyLastModified: 24084221 " << time2str(1530420864303 / 1000) << ": rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKeyLastModified(24084221, 1436069133170, data);
    cout << "LookupBySatKeyLastModified: 24084221 " << time2str(1436069133170 / 1000) << ": rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);
*/
