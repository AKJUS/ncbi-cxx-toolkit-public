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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "bulkinfo_tester.hpp"
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <util/checksum.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

USING_SCOPE(sequence);

IBulkTester::IBulkTester(void)
    : get_flags(0),
      report_all_errors(false)
{
}


IBulkTester::~IBulkTester(void)
{
}


void IBulkTester::SetParams(const TIds& ids, CScope::TGetFlags get_flags)
{
    this->ids = ids;
    this->get_flags = get_flags;
}


void IBulkTester::Display(CNcbiOstream& out, bool verify) const
{
    CMutexGuard guard(sm_DisplayMutex);
    for ( size_t i = 0; i < ids.size(); ++i ) {
        Display(out, i, verify);
    }
}


DEFINE_CLASS_STATIC_MUTEX(IBulkTester::sm_DisplayMutex);


void IBulkTester::Display(CNcbiOstream& out, size_t i, bool verify) const
{
    CMutexGuard guard(sm_DisplayMutex);
    out << GetType() << "("<<ids[i]<<") -> ";
    DisplayData(out, i);
    if ( verify && !Correct(i) ) {
        out << " expected: ";
        DisplayDataVerify(out, i);
    }
    out << endl;
}


vector<bool> IBulkTester::GetErrors(void) const
{
    vector<bool> errors(ids.size());
    for ( size_t i = 0; i < ids.size(); ++i ) {
        errors[i] = !Correct(i);
    }
    return errors;
}


void IBulkTester::VerifyWhatShouldBeNotLoaded(CScope& scope) const
{
    // should not load CBioseq_Handle by default
    VerifyBioseqShouldBeNotLoaded(scope);
}


void IBulkTester::VerifyBioseqShouldBeNotLoaded(CScope& scope) const
{
    // should not be loaded
    ITERATE ( TIds, it, ids ) {
        _VERIFY(!scope.GetBioseqHandle(*it, CScope::eGetBioseq_Loaded));
    }
}


class CDataTesterGi : public IBulkTester
{
public:
    typedef TGi TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "gi";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetGis(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetGi(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    CSeq_id_Handle id = GetId(h, eGetId_ForceGi);
                    if ( id && id.IsGi() ) {
                        data_verify[i] = id.GetGi();
                    }
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = NStr::StringToNumeric<TGi>(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i] != ZERO_GI;
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterAcc : public IBulkTester
{
public:
    typedef CSeq_id_Handle TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "acc";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetAccVers(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetAccVer(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetId(h, eGetId_ForceAcc);
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = CSeq_id_Handle::GetHandle(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i];
        }
    bool Correct(size_t i) const
        {
            if ( data[i] == data_verify[i] ) {
                return true;
            }
            if ( report_all_errors ) {
                return false;
            }
            if ( !data_verify[i] ) {
                CConstRef<CSeq_id> id = data[i].GetSeqId();
                if ( const CTextseq_id* text_id = id->GetTextseq_Id() ) {
                    if ( !text_id->IsSetVersion() ) {
                        return true;
                    }
                }
                return false;
            }
            if ( !data[i] || !data_verify[i] ||
                 data[i].Which() != data_verify[i].Which() ) {
                return false;
            }
            CRef<CSeq_id> id1(SerialClone(*data[i].GetSeqId()));
            if ( const CTextseq_id* text_id = id1->GetTextseq_Id() ) {
                const_cast<CTextseq_id*>(text_id)->ResetName();
            }
            CRef<CSeq_id> id2(SerialClone(*data_verify[i].GetSeqId()));
            if ( const CTextseq_id* text_id = id2->GetTextseq_Id() ) {
                const_cast<CTextseq_id*>(text_id)->ResetName();
            }
            return id1->Equals(*id2);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterGeneral : public IBulkTester
{
public:
    typedef CSeq_id_Handle TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "gnl";
        }
    CSeq_id_Handle GetGeneral(const vector<CSeq_id_Handle>& ids) const
        {
            for ( auto& id : ids ) {
                if ( id.Which() == CSeq_id::e_General ) {
                    return id;
                }
            }
            return null;
        }
    void LoadBulk(CScope& scope)
        {
            return LoadSingle(scope);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetGeneral(scope.GetIds(ids[i], get_flags));
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetId(h, eGetId_ForceAcc);
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = CSeq_id_Handle::GetHandle(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i];
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]);
    }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterBioseq : public IBulkTester
{
public:
    typedef string TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "bioseq";
        }
    virtual string GetData(CBioseq_Handle bh) const
        {
            CNcbiOstrstream s;
            if ( bh ) {
                set<CSeq_id_Handle, CSeq_id_Handle::PLessOrdered> ids;
                for ( auto& id : bh.GetId() ) {
                    ids.insert(id);
                }
                for ( auto& id : ids ) {
                    if ( id.Which() == CSeq_id::e_General ) continue;
                    s << id << ' ';
                }
                s << bh.GetBioseqLength();
            }
            return CNcbiOstrstreamToString(s);
        }
    void LoadBulk(CScope& scope)
        {
            data.resize(ids.size());
            vector<CBioseq_Handle> bhs = scope.GetBioseqHandles(ids);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetData(bhs[i]);
            }
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetData(scope.GetBioseqHandle(ids[i]));
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                data_verify[i] = GetData(h);
                if ( h ) {
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify = lines;
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                out << data[i] << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return !data[i].empty();
        }
    bool Correct(size_t i) const
        {
            return NStr::EqualNocase(data[i], data_verify[i]);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
    void VerifyWhatShouldBeNotLoaded(CScope& scope) const
        {
            // CBioseq_Handle are loaded
        }
};


class CDataTesterSequence : public CDataTesterBioseq
{
public:
    const bool kPreloadBulkSequence = true;
    const TSeqPos kRangeFrom = 0;
    const TSeqPos kRangeTo = 20;
    const char* GetType(void) const
        {
            return "sequence";
        }
    string GetData(CBioseq_Handle bh) const
        {
            string seq;
            if ( bh ) {
                bh.GetSeqVector(bh.eCoding_Iupac).GetSeqData(kRangeFrom, kRangeTo, seq);
            }
            return CDataTesterBioseq::GetData(bh) + ": " + seq;
        }
    void LoadBulk(CScope& scope)
        {
            data.resize(ids.size());
            vector<CBioseq_Handle> bhs = scope.GetBioseqHandles(ids);
            if ( kPreloadBulkSequence ) {
                CTSE_Handle root_tse;
                CRef<CSeq_loc> all_loc(new CSeq_loc);
                auto& intervals = all_loc->SetPacked_int().Set();
                for ( size_t i = 0; i < ids.size(); ++i ) {
                    if ( !bhs[i] ) {
                        continue;
                    }
                    TSeqPos len = bhs[i].GetBioseqLength();
                    if ( len <= kRangeTo ) {
                        continue;
                    }
                    if ( !root_tse ) {
                        root_tse = bhs[i].GetTSE_Handle();
                    }
                    intervals.push_back(Ref(new CSeq_interval(*SerialClone(*ids[i].GetSeqId()), kRangeFrom, kRangeTo)));
                }
                if ( root_tse ) { // bulk loading only works if there's a TSE to attach locks to
                    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
                    sel.SetLinkUsedTSE(root_tse);
                    CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForSeq_loc(*all_loc, &scope);
                    seq_map->CanResolveRange(&scope, sel); // segment pre-loading call
                }
            }
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetData(bhs[i]);
            }
        }
};


class CDataTesterLabel : public IBulkTester
{
public:
    typedef string TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "label";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetLabels(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetLabel(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = GetLabel(h.GetId());
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                data_verify[i] = line;
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return !data[i].empty();
        }
    bool Correct(size_t i) const
        {
            if ( data[i] == data_verify[i] ) {
                return true;
            }
            if ( report_all_errors ) {
                return false;
            }
            if ( data_verify[i].find('|') == NPOS ||
                 NStr::StartsWith(data_verify[i], "gi|") ||
                 NStr::StartsWith(data_verify[i], "lcl|") ) {
                return true;
            }
            return false;
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterTaxId : public IBulkTester
{
public:
    typedef TTaxId TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
    {
        return "taxid";
    }
    void LoadBulk(CScope& scope)
    {
        data = scope.GetTaxIds(ids, get_flags);
    }
    void LoadSingle(CScope& scope)
    {
        data.resize(ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            data[i] = scope.GetTaxId(ids[i], get_flags);
        }
    }
    void LoadVerify(CScope& scope)
    {
        data_verify.resize(ids.size(), INVALID_TAX_ID);
        for (size_t i = 0; i < ids.size(); ++i) {
            CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
            if (h) {
                data_verify[i] = GetTaxId(h);
                scope.RemoveFromHistory(h);
            }
        }
    }
    void LoadVerify(const vector<string>& lines)
    {
        data_verify.resize(ids.size(), INVALID_TAX_ID);
        for (size_t i = 0; i < ids.size(); ++i) {
            const string& line = lines[i];
            if (!line.empty()) {
                data_verify[i] = NStr::StringToNumeric<TTaxId>(line);
            }
        }
    }
    void SaveResults(CNcbiOstream& out) const
    {
        for (size_t i = 0; i < ids.size(); ++i) {
            if (Valid(i)) {
                out << data[i];
            }
            out << NcbiEndl;
        }
    }
    bool Valid(size_t i) const
    {
        return data[i] != INVALID_TAX_ID;
    }
    bool Correct(size_t i) const
    {
        return (data[i] == data_verify[i]);
    }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterHash : public IBulkTester
{
public:
    typedef int TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "hash";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceHashes(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceHash(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( CBioseq_Handle h = scope.GetBioseqHandle(ids[i]) ) {
                    CChecksum sum(CChecksum::eCRC32INSD);
                    CSeqVector sv(h, h.eCoding_Iupac);
                    for ( CSeqVector_CI it(sv); it; ) {
                        TSeqPos size = it.GetBufferSize();
                        sum.AddChars(it.GetBufferPtr(), size);
                        it += size;
                    }
                    data_verify[i] = sum.GetChecksum();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = NStr::StringToNumeric<int>(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i];
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]) || !data[i];
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
    void VerifyWhatShouldBeNotLoaded(CScope& scope) const
        {
            // CBioseq_Handle may be loaded for recalculation
            if ( get_flags & CScope::fDoNotRecalculate ) {
                VerifyBioseqShouldBeNotLoaded(scope);
            }
        }
};


class CDataTesterLength : public IBulkTester
{
public:
    typedef TSeqPos TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "length";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceLengths(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceLength(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), kInvalidSeqPos);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = h.GetBioseqLength();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size(), kInvalidSeqPos);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = NStr::StringToNumeric<TSeqPos>(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i] != kInvalidSeqPos;
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterType : public IBulkTester
{
public:
    typedef CSeq_inst::EMol TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "type";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceTypes(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceType(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), CSeq_inst::eMol_not_set);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                if ( h ) {
                    data_verify[i] = h.GetSequenceType();
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size(), CSeq_inst::eMol_not_set);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = CSeq_inst::EMol(NStr::StringToNumeric<int>(line));
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i] != CSeq_inst::eMol_not_set;
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterState : public IBulkTester
{
public:
    typedef int TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "state";
        }
    void LoadBulk(CScope& scope)
        {
            data = scope.GetSequenceStates(ids, get_flags);
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = scope.GetSequenceState(ids[i], get_flags);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size(), -1);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                CBioseq_Handle h = scope.GetBioseqHandle(ids[i]);
                data_verify[i] = h.GetState();
                if ( h ) {
                    scope.RemoveFromHistory(h);
                }
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size(), -1);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = NStr::StringToNumeric<int>(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                if ( Valid(i) ) {
                    out << data[i];
                }
                out << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i] != -1;
        }
    bool Correct(size_t i) const
        {
            int data_sup = data[i] & CBioseq_Handle::fState_suppress;
            int verify_sup = data_verify[i] & CBioseq_Handle::fState_suppress;
            if ((data_sup == 0) != (verify_sup == 0)) return false;
            return ((data[i] & ~CBioseq_Handle::fState_suppress ) == (data_verify[i] & ~CBioseq_Handle::fState_suppress));
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
};


class CDataTesterCDD : public IBulkTester
{
public:
    typedef size_t TDataValue;
    typedef vector<TDataValue> TDataSet;
    TDataSet data, data_verify;

    const char* GetType(void) const
        {
            return "cdd";
        }
    TDataValue GetData(const CTSE_Handle& tse) const
        {
            TDataValue ret = 0;
            if ( tse ) {
                if ( auto se = tse.GetObjectCore() ) {
                    if ( se->IsSetAnnot() ) {
                        for ( auto& annot : se->GetAnnot() ) {
                            if ( annot->GetData().IsFtable() ) {
                                ret += annot->GetData().GetFtable().size();
                            }
                            else {
                                ERR_POST("no ftable: "<<tse.x_GetTSE_Info().GetDescription());
                            }
                        }
                        if ( se->GetAnnot().empty() ) {
                            ERR_POST("no annot 2: "<<tse.x_GetTSE_Info().GetDescription());
                        }
                    }
                    else {
                        ERR_POST("no annot: "<<tse.x_GetTSE_Info().GetDescription());
                    }
                }
                else {
                    ERR_POST("no core: "<<tse.x_GetTSE_Info().GetDescription());
                }
            }
            return ret;
        }
    TDataValue GetData(CScope& scope, const CSeq_id_Handle& id) const
        {
            TDataValue ret = 0;
            CBioseq_Handle bh = scope.GetBioseqHandle(id);
            SAnnotSelector sel;
            sel.AddNamedAnnots("CDD");
            if ( bh ) {
                ret = CFeat_CI(bh, sel).GetSize();
                scope.RemoveFromHistory(bh);
            }
            return ret;
        }
    void LoadBulk(CScope& scope)
        {
            auto tmp = scope.GetCDDAnnots(ids);
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetData(tmp[i]);
            }
        }
    void LoadSingle(CScope& scope)
        {
            data.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data[i] = GetData(scope, ids[i]);
            }
        }
    void LoadVerify(CScope& scope)
        {
            data_verify.resize(ids.size());
            for ( size_t i = 0; i < ids.size(); ++i ) {
                data_verify[i] = GetData(scope, ids[i]);
            }
        }
    void LoadVerify(const vector<string>& lines)
        {
            data_verify.resize(ids.size(), -1);
            for ( size_t i = 0; i < ids.size(); ++i ) {
                const string& line = lines[i];
                if ( !line.empty() ) {
                    data_verify[i] = NStr::StringToNumeric<TDataValue>(line);
                }
            }
        }
    void SaveResults(CNcbiOstream& out) const
        {
            for ( size_t i = 0; i < ids.size(); ++i ) {
                out << data[i] << NcbiEndl;
            }
        }
    bool Valid(size_t i) const
        {
            return data[i] != TDataValue(-1);
        }
    bool Correct(size_t i) const
        {
            return (data[i] == data_verify[i]);
        }
    void DisplayData(CNcbiOstream& out, size_t i) const
        {
            out << data[i];
        }
    void DisplayDataVerify(CNcbiOstream& out, size_t i) const
        {
            out << data_verify[i];
        }
    void VerifyWhatShouldBeNotLoaded(CScope& scope) const
        {
            // CBioseq_Handle are loaded
        }
};


void IBulkTester::AddArgs(CArgDescriptions& args)
{
    args.AddDefaultKey("type", "Type",
                       "Type of bulk request",
                       CArgDescriptions::eString, "gi");
    args.SetConstraint("type",
                       &(*new CArgAllow_Strings,
                         "gi", "acc", "label", "taxid", "hash",
                         "length", "type", "state", "general", "bioseq", "sequence", "cdd"));
    args.AddFlag("no-force", "Do not force info loading");
    args.AddFlag("throw-on-missing-seq", "Throw exception for missing sequence");
    args.AddFlag("throw-on-missing-data", "Throw exception for missing data");
    args.AddFlag("no-recalc", "Avoid data recalculation");
}


IBulkTester::EBulkType IBulkTester::ParseType(const CArgs& args)
{
    string s = args["type"].AsString();
    if ( s == "gi" ) {
        return eBulk_gi;
    }
    if ( s == "acc" ) {
        return eBulk_acc;
    }
    if ( s == "label" ) {
        return eBulk_label;
    }
    if ( s == "taxid" ) {
        return eBulk_taxid;
    }
    if ( s == "hash" ) {
        return eBulk_hash;
    }
    if ( s == "length" ) {
        return eBulk_length;
    }
    if ( s == "type" ) {
        return eBulk_type;
    }
    if ( s == "state" ) {
        return eBulk_state;
    }
    if ( s == "general" ) {
        return eBulk_general;
    }
    if ( s == "bioseq" ) {
        return eBulk_bioseq;
    }
    if ( s == "sequence" ) {
        return eBulk_sequence;
    }
    if ( s == "cdd" ) {
        return eBulk_cdd;
    }
    return eBulk_gi;
}


CScope::TGetFlags IBulkTester::ParseGetFlags(const CArgs& args)
{
    CScope::TGetFlags flags = 0;
    if ( !args["no-force"] ) {
        flags |= CScope::fForceLoad;
    }
    if ( args["throw-on-missing-seq"] ) {
        flags |= CScope::fThrowOnMissingSequence;
    }
    if ( args["throw-on-missing-data"] ) {
        flags |= CScope::fThrowOnMissingData;
    }
    if ( args["no-recalc"] ) {
        flags |= CScope::fDoNotRecalculate;
    }
    return flags;
}


IBulkTester* IBulkTester::CreateTester(EBulkType type)
{
    switch ( type ) {
    case eBulk_gi:     return new CDataTesterGi();
    case eBulk_acc:    return new CDataTesterAcc();
    case eBulk_label:  return new CDataTesterLabel();
    case eBulk_taxid:  return new CDataTesterTaxId();
    case eBulk_hash:   return new CDataTesterHash();
    case eBulk_length: return new CDataTesterLength();
    case eBulk_type:   return new CDataTesterType();
    case eBulk_state:  return new CDataTesterState();
    case eBulk_general:return new CDataTesterGeneral();
    case eBulk_bioseq: return new CDataTesterBioseq();
    case eBulk_sequence:return new CDataTesterSequence();
    case eBulk_cdd:    return new CDataTesterCDD();
    default: return 0;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
