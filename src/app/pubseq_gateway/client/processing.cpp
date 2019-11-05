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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <numeric>

#include <serial/enumvalues.hpp>

#include "performance.hpp"
#include "processing.hpp"

BEGIN_NCBI_SCOPE

struct SNewRequestContext
{
    SNewRequestContext() :
        m_RequestContext(new CRequestContext)
    {
        m_RequestContext->SetRequestID();
        CDiagContext::SetRequestContext(m_RequestContext);
    }

    ~SNewRequestContext()
    {
        CDiagContext::SetRequestContext(nullptr);
    }

    CRef<CRequestContext> Clone() { return m_RequestContext->Clone(); }

private:
    SNewRequestContext(const SNewRequestContext&) = delete;
    void operator=(const SNewRequestContext&) = delete;

    CRef<CRequestContext> m_RequestContext;
};

struct SInteractiveNewRequestStart : SNewRequestContext
{
    SInteractiveNewRequestStart(CJson_ConstNode params);

private:
    struct SExtra : private CDiagContext_Extra
    {
        SExtra() : CDiagContext_Extra(GetDiagContext().PrintRequestStart()) {}

        void Print(const string& prefix, CJson_ConstValue  json);
        void Print(const string& prefix, CJson_ConstArray  json);
        void Print(const string& prefix, CJson_ConstObject json);
        void Print(const string& prefix, CJson_ConstNode   json);

    private:
        using CDiagContext_Extra::Print;
    };
};

string s_GetId(const CJson_Document& req_doc);
CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status);

SJsonOut& SJsonOut::operator<<(const CJson_Document& doc)
{
    stringstream ss;
    ss << doc;

    unique_lock<mutex> lock(m_Mutex);

    if (m_Pipe) {
        cout << ss.rdbuf() << endl;
    } else {
        cout << m_Separator << '\n' << ss.rdbuf() << flush;
        m_Separator = ',';
    }

    return *this;
}

SJsonOut::~SJsonOut()
{
    // If not in pipe mode and printed some JSON
    if (!m_Pipe && (m_Separator == ',')) {
        cout << "\n]" << endl;
    }
}

template <class TItem>
CJsonResponse::CJsonResponse(EPSG_Status status, TItem item, bool set_reply_type) :
    m_JsonObj(SetObject()),
    m_SetReplyType(set_reply_type)
{
    if (auto request_id = s_GetReply(item)->GetRequest()->template GetUserContext<string>()) {
        m_JsonObj["request_id"].SetValue().SetString(*request_id);
    }

    Fill(status, item);
}

CJsonResponse::CJsonResponse(const string& id, bool result) :
    CJsonResponse(id)
{
    m_JsonObj["result"].SetValue().SetBool(result);
}

CJsonResponse::CJsonResponse(const string& id, const CJson_Document& result) :
    CJsonResponse(id)
{
    m_JsonObj["result"].AssignCopy(result);
}

CJsonResponse::CJsonResponse(const string& id, int code, const string& message) :
    CJsonResponse(id)
{
    CJson_Object error_obj = m_JsonObj.insert_object("error");
    error_obj["code"].SetValue().SetInt4(code);
    error_obj["message"].SetValue().SetString(message);
}

CJsonResponse::CJsonResponse(const string& id) :
    m_JsonObj(SetObject())
{
    m_JsonObj["jsonrpc"].SetValue().SetString("2.0");

    auto id_value = m_JsonObj["id"].SetValue();

    if (id.empty()) {
        id_value.SetNull();
    } else {
        id_value.SetString(id);
    }
}

void CJsonResponse::Fill(EPSG_Status reply_status, shared_ptr<CPSG_Reply> reply)
{
    switch (reply_status) {
        case EPSG_Status::eNotFound: m_JsonObj["reply"].SetValue().SetString("NotFound"); return;
        case EPSG_Status::eCanceled: m_JsonObj["reply"].SetValue().SetString("Canceled"); return;
        case EPSG_Status::eError:    Fill(reply, "Failure");                              return;
        default: _TROUBLE;
    }
}

void CJsonResponse::Fill(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
{
    auto reply_item_type = reply_item->GetType();

    if (reply_item_status == EPSG_Status::eError) {
        switch (reply_item_type) {
            case CPSG_ReplyItem::eBlobData:       return Fill(reply_item, "BlobData");
            case CPSG_ReplyItem::eBlobInfo:       return Fill(reply_item, "BlobInfo");
            case CPSG_ReplyItem::eSkippedBlob:    return Fill(reply_item, "SkippedBlob");
            case CPSG_ReplyItem::eBioseqInfo:     return Fill(reply_item, "BioseqInfo");
            case CPSG_ReplyItem::eNamedAnnotInfo: return Fill(reply_item, "NamedAnnotInfo");
            case CPSG_ReplyItem::eEndOfReply:     _TROUBLE; return;
        }
    }

    switch (reply_item_type) {
        case CPSG_ReplyItem::eBlobData:
            return Fill(static_pointer_cast<CPSG_BlobData>(reply_item));

        case CPSG_ReplyItem::eBlobInfo:
            return Fill(static_pointer_cast<CPSG_BlobInfo>(reply_item));

        case CPSG_ReplyItem::eSkippedBlob:
            return Fill(static_pointer_cast<CPSG_SkippedBlob>(reply_item));

        case CPSG_ReplyItem::eBioseqInfo:
            return Fill(static_pointer_cast<CPSG_BioseqInfo>(reply_item));

        case CPSG_ReplyItem::eNamedAnnotInfo:
            return Fill(static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item));

        case CPSG_ReplyItem::eEndOfReply:
            _TROUBLE;
            return;
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobData> blob_data)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString("BlobData");
    m_JsonObj["id"].SetValue().SetString(blob_data->GetId().Get());
    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    m_JsonObj["data"].SetValue().SetString(NStr::JsonEncode(os.str()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobInfo> blob_info)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString("BlobInfo");
    m_JsonObj["id"].SetValue().SetString(blob_info->GetId().Get());
    m_JsonObj["compression"].SetValue().SetString(blob_info->GetCompression());
    m_JsonObj["format"].SetValue().SetString(blob_info->GetFormat());
    m_JsonObj["version"].SetValue().SetUint8(blob_info->GetVersion());
    m_JsonObj["storage_size"].SetValue().SetUint8(blob_info->GetStorageSize());
    m_JsonObj["size"].SetValue().SetUint8(blob_info->GetSize());
    m_JsonObj["is_dead"].SetValue().SetBool(blob_info->IsDead());
    m_JsonObj["is_suppressed"].SetValue().SetBool(blob_info->IsSuppressed());
    m_JsonObj["is_withdrawn"].SetValue().SetBool(blob_info->IsWithdrawn());
    m_JsonObj["hup_release_date"].SetValue().SetString(blob_info->GetHupReleaseDate().AsString());
    m_JsonObj["owner"].SetValue().SetUint8(blob_info->GetOwner());
    m_JsonObj["original_load_date"].SetValue().SetString(blob_info->GetOriginalLoadDate().AsString());
    m_JsonObj["class"].SetValue().SetString(objects::CBioseq_set::ENUM_METHOD_NAME(EClass)()->FindName(blob_info->GetClass(), true));
    m_JsonObj["division"].SetValue().SetString(blob_info->GetDivision());
    m_JsonObj["username"].SetValue().SetString(blob_info->GetUsername());
    m_JsonObj["split_info_blob_id"].SetValue().SetString(blob_info->GetSplitInfoBlobId().Get());

    for (int i = 1; ; ++i) {
        auto blob_id = blob_info->GetChunkBlobId(i).Get();
        if (blob_id.empty()) break;
        if (i == 1) CJson_Array ar = m_JsonObj.insert_array("chunk_blob_id");
        m_JsonObj["chunk_blob_id"].SetArray().push_back(blob_id);
    }

    m_JsonObj["split_version"].SetValue().SetInt8(blob_info->GetSplitVersion());
}

string s_ReasonToString(CPSG_SkippedBlob::EReason reason)
{
    switch (reason) {
        case CPSG_SkippedBlob::eExcluded:   return "Excluded";
        case CPSG_SkippedBlob::eInProgress: return "InProgress";
        case CPSG_SkippedBlob::eSent:       return "Sent";
        case CPSG_SkippedBlob::eUnknown:    return "Unknown";
    };

    _TROUBLE;
    return "";
}

void CJsonResponse::Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString("SkippedBlob");
    m_JsonObj["id"].SetValue().SetString(skipped_blob->GetId().Get());
    m_JsonObj["reason"].SetValue().SetString(s_ReasonToString(skipped_blob->GetReason()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString("BioseqInfo");

    const auto included_info = bioseq_info->IncludedInfo();

    if (included_info & CPSG_Request_Resolve::fCanonicalId)  m_JsonObj["canonical_id"].SetValue().SetString(bioseq_info->GetCanonicalId().Get());

    if (included_info & CPSG_Request_Resolve::fOtherIds) {
        CJson_Array ar = m_JsonObj.insert_array("other_ids");

        for (const auto& bio_id : bioseq_info->GetOtherIds()) {
            m_JsonObj["other_ids"].SetArray().push_back(bio_id.Get());
        }
    }

    if (included_info & CPSG_Request_Resolve::fMoleculeType) m_JsonObj["molecule_type"].SetValue().SetString(objects::CSeq_inst::ENUM_METHOD_NAME(EMol)()->FindName(bioseq_info->GetMoleculeType(), true));
    if (included_info & CPSG_Request_Resolve::fLength)       m_JsonObj["length"].SetValue().SetUint8(bioseq_info->GetLength());
    if (included_info & CPSG_Request_Resolve::fState)        m_JsonObj["state"].SetValue().SetInt8(bioseq_info->GetState());
    if (included_info & CPSG_Request_Resolve::fBlobId)       m_JsonObj["blob_id"].SetValue().SetString(bioseq_info->GetBlobId().Get());
    if (included_info & CPSG_Request_Resolve::fTaxId)        m_JsonObj["tax_id"].SetValue().SetInt8(bioseq_info->GetTaxId());
    if (included_info & CPSG_Request_Resolve::fHash)         m_JsonObj["hash"].SetValue().SetInt8(bioseq_info->GetHash());
    if (included_info & CPSG_Request_Resolve::fDateChanged)  m_JsonObj["date_changed"].SetValue().SetString(bioseq_info->GetDateChanged().AsString());
}

void CJsonResponse::Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString("NamedAnnotInfo");
    m_JsonObj["canonical_id"].SetValue().SetString(named_annot_info->GetCanonicalId().Get());
    m_JsonObj["name"].SetValue().SetString(named_annot_info->GetName());

    const auto range = named_annot_info->GetRange();
    CJson_Object range_obj = m_JsonObj.insert_object("range");
    range_obj["from"].SetValue().SetInt8(range.GetFrom());
    range_obj["to"].SetValue().SetInt8(range.GetTo());

    m_JsonObj["blob_id"].SetValue().SetString(named_annot_info->GetBlobId().Get());
    m_JsonObj["version"].SetValue().SetUint8(named_annot_info->GetVersion());

    CJson_Array zoom_level_array = m_JsonObj.insert_array("zoom_levels");
    for (const auto zoom_level : named_annot_info->GetZoomLevels()) {
        zoom_level_array.push_back(zoom_level);
    }

    CJson_Array annot_info_array = m_JsonObj.insert_array("annot_info_list");
    for (const auto annot_info : named_annot_info->GetAnnotInfoList()) {
        CJson_Object annot_info_obj = annot_info_array.push_back_object();
        annot_info_obj["annot_type"].SetValue().SetInt8(annot_info.annot_type);
        annot_info_obj["feat_type"].SetValue().SetInt8(annot_info.feat_type);
        annot_info_obj["feat_subtype"].SetValue().SetInt8(annot_info.feat_subtype);
    }
}

template <class TItem>
void CJsonResponse::Fill(TItem item, string type)
{
    if (m_SetReplyType) m_JsonObj["reply"].SetValue().SetString(type);

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) return;

        m_JsonObj.insert_array("errors").push_back(message);
    }
}

int CProcessing::OneRequest(const string& service, shared_ptr<CPSG_Request> request)
{
    CPSG_Queue queue(service);
    SJsonOut json_out;

    queue.SendRequest(request, CDeadline::eInfinite);
    auto reply = queue.GetNextReply(CDeadline::eInfinite);

    _ASSERT(reply);

    const CTimeout kTryTimeout(0.1);
    EPSG_Status status = EPSG_Status::eInProgress;
    bool end_of_reply = false;
    list<shared_ptr<CPSG_ReplyItem>> reply_items;

    while ((status == EPSG_Status::eInProgress) || !end_of_reply || !reply_items.empty()) {
        if (status == EPSG_Status::eInProgress) {
            status = reply->GetStatus(kTryTimeout);

            switch (status) {
                case EPSG_Status::eSuccess:    continue;
                case EPSG_Status::eInProgress: continue;
                default: json_out << CJsonResponse(status, reply);
            }
        }

        if (!end_of_reply) {
            if (auto reply_item = reply->GetNextItem(kTryTimeout)) {
                if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                    end_of_reply = true;
                } else {
                    reply_items.emplace_back(move(reply_item));
                }
            }
        }

        for (auto it = reply_items.begin(); it != reply_items.end();) {
            auto reply_item = *it;
            auto reply_item_status = reply_item->GetStatus(kTryTimeout);

            if (reply_item_status != EPSG_Status::eInProgress) {
                it = reply_items.erase(it);
                json_out << CJsonResponse(reply_item_status, reply_item);
            } else {
                ++it;
            }
        }
    }

    return 0;
}

CParallelProcessing::CParallelProcessing(const string& service, bool pipe, const CArgs& args, bool echo, bool batch_resolve) :
    m_PsgQueue(service),
    m_JsonOut(pipe)
{
    enum EType : size_t { eReporter, eRetriever, eSubmitter };
    vector<CTempString> threads;
    NStr::Split(args["worker-threads"].AsString(), ":", threads);

    auto threads_number = [&threads](EType type, size_t default_value)
    {
        const size_t kMin = 1;
        const size_t kMax = 10;
        const size_t n = threads.size() <= type ? default_value : NStr::StringToNumeric<size_t>(threads[type]);
        return max(min(n, kMax), kMin);
    };

    for (size_t n = threads_number(eReporter, 7); n; --n) {
        if (batch_resolve) {
            m_Threads.emplace_back(&BatchResolve::Reporter, ref(m_ReplyQueue), ref(m_JsonOut));
        } else {
            m_Threads.emplace_back(&Interactive::Reporter, ref(m_ReplyQueue), ref(m_JsonOut));
        }
    }

    for (size_t n = threads_number(eRetriever, 2); n; --n) {
        m_Threads.emplace_back([&]{ Retriever(m_PsgQueue, m_ReplyQueue); });
    }

    for (size_t n = threads_number(eSubmitter, 2); n; --n) {
        if (batch_resolve) {
            m_Threads.emplace_back(&BatchResolve::Submitter, ref(m_InputQueue), ref(m_PsgQueue), cref(args));
        } else {
            m_Threads.emplace_back(&Interactive::Submitter, ref(m_InputQueue), ref(m_PsgQueue), ref(m_JsonOut), echo);
        }
    }
}

CParallelProcessing::~CParallelProcessing()
{
    m_InputQueue.Stop(m_InputQueue.eDrain);
}

void CParallelProcessing::BatchResolve::Submitter(TInputQueue& input, CPSG_Queue& output, const CArgs& args)
{
    static atomic_size_t instances(0);
    instances++;

    auto request_context = CDiagContext::GetRequestContext().Clone();
    auto type(args["type"].HasValue() ? SRequestBuilder::GetBioIdType(args["type"].AsString()) : CPSG_BioId::TType());
    auto include_info(SRequestBuilder::GetIncludeInfo(SRequestBuilder::GetSpecified<CPSG_Request_Resolve>(args)));

    string id;

    while (input.Pop(id)) {
        _ASSERT(!id.empty()); // ReadLine makes sure it's not empty
        auto bio_id = CPSG_BioId(id, type);
        auto user_context = make_shared<string>(move(id));
        auto request = make_shared<CPSG_Request_Resolve>(move(bio_id), move(user_context), move(request_context));

        request->IncludeInfo(include_info);

        if (!output.SendRequest(move(request), CDeadline::eInfinite)) {
            _TROUBLE;
        }
    }

    if (--instances == 0) {
        output.Stop();
    }
}

void CParallelProcessing::Interactive::Submitter(TInputQueue& input, CPSG_Queue& output, SJsonOut& json_out, bool echo)
{
    static atomic_size_t instances(0);
    instances++;

    CJson_Schema json_schema(CProcessing::RequestSchema());
    string line;

    while (input.Pop(line)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty

        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            json_out << CJsonResponse(s_GetId(json_doc), -32700, json_doc.GetReadError());
        } else if (!json_schema.Validate(json_doc)) {
            json_out << CJsonResponse(s_GetId(json_doc), -32600, json_schema.GetValidationError());
        } else {
            if (echo) json_out << json_doc;

            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto id = json_obj["id"].GetValue().GetString();
            auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();
            auto user_context = make_shared<string>(id);

            SInteractiveNewRequestStart new_request_start(params);
            auto request_context = new_request_start.Clone();

            if (auto request = SRequestBuilder::Build(method, params.GetObject(), move(user_context), move(request_context))) {
                if (!output.SendRequest(move(request), CDeadline::eInfinite)) {
                    _TROUBLE;
                }
            }
        }
    }

    if (--instances == 0) {
        output.Stop();
    }
}

void CParallelProcessing::Retriever(CPSG_Queue& input, TReplyQueue& output)
{
    static atomic_size_t instances(0);
    instances++;

    do {
        if (auto reply = input.GetNextReply(CDeadline::eInfinite)) {
            output.Push(move(reply));
        }
    }
    while (!input.IsEmpty());

    if (--instances == 0) {
        output.Stop(output.eDrain);
    }
}

void CParallelProcessing::BatchResolve::Reporter(TReplyQueue& input, SJsonOut& output)
{
    shared_ptr<CPSG_Reply> reply;

    while (input.Pop(reply)) {
        _ASSERT(reply);

        for (;;) {
            auto item = reply->GetNextItem(CDeadline::eInfinite);
            _ASSERT(item);

            if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                break;
            }

            auto status = item->GetStatus(CDeadline::eInfinite);
            _ASSERT(status != EPSG_Status::eInProgress);

            CJsonResponse result_doc(status, item, false);
            output << result_doc;
        }

        auto status = reply->GetStatus(CDeadline::eInfinite);
        _ASSERT(status != EPSG_Status::eInProgress);

        if (status != EPSG_Status::eSuccess) {
            CJsonResponse result_doc(status, reply, false);
            output << result_doc;
        }
    }
}

void CParallelProcessing::Interactive::Reporter(TReplyQueue& input, SJsonOut& output)
{
    shared_ptr<CPSG_Reply> reply;

    while (input.Pop(reply)) {
        _ASSERT(reply);

        const auto request = reply->GetRequest();
        const auto& request_id = *request->GetUserContext<string>();

        for (;;) {
            auto item = reply->GetNextItem(CDeadline::eInfinite);
            _ASSERT(item);

            if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                break;
            }

            auto status = item->GetStatus(CDeadline::eInfinite);
            _ASSERT(status != EPSG_Status::eInProgress);

            CJsonResponse result_doc(status, item);
            output << CJsonResponse(request_id, result_doc);
        }

        auto status = reply->GetStatus(CDeadline::eInfinite);
        _ASSERT(status != EPSG_Status::eInProgress);

        CRequestContextGuard_Base guard(request->GetRequestContext());
        guard.SetStatus(s_PsgStatusToRequestStatus(status));

        if (status != EPSG_Status::eSuccess) {
            CJsonResponse result_doc(status, reply);
            output << CJsonResponse(request_id, result_doc);
        }
    }
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_ReplyItem>& item)
{
    return item->GetReply();
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_Reply>& reply)
{
    return reply;
}

CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status)
{
    switch (psg_status) {
        case EPSG_Status::eSuccess:  return CRequestStatus::e200_Ok;
        case EPSG_Status::eNotFound: return CRequestStatus::e404_NotFound;
        case EPSG_Status::eCanceled: return CRequestStatus::e499_BrokenConnection;
        case EPSG_Status::eError:    return CRequestStatus::e400_BadRequest;
        case EPSG_Status::eInProgress: _TROUBLE; break;
    }

    return CRequestStatus::e500_InternalServerError;
}

string s_GetId(const CJson_Document& req_doc)
{
    string id;

    if (req_doc.IsObject()) {
        auto req_obj = req_doc.GetObject();

        if (req_obj.has("id")) {
            auto id_node = req_obj["id"];

            if (id_node.IsValue()) {
                auto id_value = id_node.GetValue();

                if (id_value.IsString()) {
                    id = id_value.GetString();
                }
            }
        }
    }

    return id;
}

template <class TCreateContext>
vector<shared_ptr<CPSG_Request>> CProcessing::ReadCommands(TCreateContext create_context)
{
    static CJson_Schema json_schema(RequestSchema());
    string line;
    vector<shared_ptr<CPSG_Request>> requests;

    // Read requests from cin
    while (ReadLine(line)) {
        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            cerr << "Error in request '" << s_GetId(json_doc) << "': " << json_doc.GetReadError() << endl;
            return {};
        } else if (!json_schema.Validate(json_doc)) {
            cerr << "Error in request '" << s_GetId(json_doc) << "': " << json_schema.GetValidationError() << endl;
            return {};
        } else {
            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto params = json_obj.has("params") ? json_obj["params"] : CJson_Document();
            auto user_context = create_context(json_doc, params);

            if (!user_context) return {};

            if (auto request = SRequestBuilder::Build(method, params.GetObject(), move(user_context))) {
                requests.emplace_back(move(request));
                if (requests.size() % 2000 == 0) cerr << '.';
            }
        }
    }

    return requests;
}

int CProcessing::Performance(const string& service, size_t user_threads, bool local_queue, ostream& os)
{
    SIoRedirector io_redirector(cout, os);

    CPSG_Queue global_queue(service);

    cerr << "Preparing requests: ";
    auto requests = ReadCommands([](CJson_Document&, CJson_ConstNode&){ return make_shared<SMetrics>(); });

    if (requests.empty()) return -1;

    atomic_int start(user_threads);
    atomic_int to_submit(requests.size());
    auto wait = [&]() { while (start > 0) this_thread::sleep_for(chrono::microseconds(1)); };

    auto l = [&]() {
        shared_ptr<CPSG_Queue> queue;
        deque<shared_ptr<CPSG_Reply>> replies;
        
        if (service.empty()) {
            queue = shared_ptr<CPSG_Queue>(shared_ptr<CPSG_Queue>(), &global_queue);
        } else {
            queue = make_shared<CPSG_Queue>(service);
        }

        start--;
        wait();

        for (;;) {
            auto i = to_submit--;

            if (i <= 0) break;

            // Submit
            {
                auto& request = requests[requests.size() - i];
                auto metrics = request->GetUserContext<SMetrics>();

                metrics->Set(SMetricType::eStart);
                _VERIFY(queue->SendRequest(request, CDeadline::eInfinite));
                metrics->Set(SMetricType::eSubmit);
            }

            // Response
            auto reply = queue->GetNextReply(CDeadline::eInfinite);
            _ASSERT(reply);

            // Store the reply for now to prevent internal metrics from being written to cout (affects performance)
            replies.emplace_back(reply);

            auto request = reply->GetRequest();
            auto metrics = request->GetUserContext<SMetrics>();

            metrics->Set(SMetricType::eReply);
            bool success = reply->GetStatus(CDeadline::eInfinite) == EPSG_Status::eSuccess;
            metrics->Set(SMetricType::eDone);

            while (success) {
                auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
                _ASSERT(reply_item);

                if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

                metrics->NewItem();
                success = reply_item->GetStatus(CDeadline::eInfinite) == EPSG_Status::eSuccess;
            }

            if (success) metrics->SetSuccess();
        }
    };

    vector<thread> threads;
    threads.reserve(user_threads);

    // Start threads in advance so it won't affect metrics
    for (size_t i = 0; i < user_threads; ++i) {
        threads.emplace_back(l);
    }

    wait();

    // Start processing replies
    cerr << "\nSubmitting requests: ";
    int previous = requests.size() / 2000;

    while (to_submit > 0) {
        int current = to_submit / 2000;

        if (current < previous) {
            cerr << '.';
            previous = current;
        }
    }

    cerr << "\nWaiting for threads: " << user_threads << '\n';

    for (auto& t : threads) {
        t.join();
    }

    // Release any replies held in the queue
    global_queue = CPSG_Queue();

    // Output metrics
    cerr << "Outputting metrics: ";
    size_t output = 0;

    for (auto& request : requests) {
        auto metrics = request->GetUserContext<SMetrics>();
        cout << *metrics;
        if (++output % 2000 == 0) cerr << '.';
    }

    cerr << '\n';
    return 0;
}

int CProcessing::Report(istream& is, ostream& os, double percentage)
{
    SPercentiles::Report(is, os, percentage);
    return 0;
}

struct STestingContext : string
{
    enum EExpected { eSuccess, eReplyError, eReplyItemError };

    EExpected expected;

    STestingContext(string id, EExpected e) : string(id), expected(e) {}

    static shared_ptr<STestingContext> CreateContext(CJson_Document& json_doc, CJson_ConstNode& params);
};

shared_ptr<STestingContext> STestingContext::CreateContext(CJson_Document& json_doc, CJson_ConstNode& params)
{
    _ASSERT(params.IsObject());
    const auto id = s_GetId(json_doc);
    string error;

    try {
        auto params_obj = params.GetObject();

        if (params_obj.has("expected_result")) {
            auto expected = params_obj["expected_result"];

            if (expected.IsObject()) {
                auto expected_obj = expected.GetObject();
                auto result = eSuccess;

                if (expected_obj.has("fail")) {
                    result = expected_obj["fail"].GetValue().GetString() == "reply" ? eReplyError : eReplyItemError;
                }

                return make_shared<STestingContext>(id, result);
            } else {
                error = "'expected_result' is not of object type";
            }
        } else {
            error = "no 'expected_result' found";
        }
    }
    catch (exception& e) {
        error = e.what();
    }

    cerr << "Error in request '" << id << "': " << error << endl;
    return {};
}

NCBI_PARAM_DECL(string, PSG, service_name);
typedef NCBI_PARAM_TYPE(PSG, service_name) TPSG_ServiceName;
NCBI_PARAM_DEF(string, PSG, service_name, "PSG");

void s_ReportErrors(const string& request_id, shared_ptr<CPSG_Reply> reply)
{
    cerr << "Fail for request '" << request_id << "' expected to succeed";

    auto delimiter = ": ";

    for (;;) {
        auto message = reply->GetNextMessage();

        if (message.empty()) break;

        cerr << delimiter << message;
        delimiter = ", ";
    }

    cerr << endl;
}

struct SExitCode
{
    enum { eSuccess = 0, eRunError = -1, eTestFail = -2, };

    // eRunError has the highest priority and eSuccess - the lowest
    void operator =(int rv) { if ((m_RV != eRunError) && (rv != eSuccess)) m_RV = rv; }

    operator int() const { return m_RV; }

private:
    int m_RV = eSuccess;
};

int s_CheckItems(bool expect_errors, const string& request_id, shared_ptr<CPSG_Reply> reply)
{
    bool no_errors = true;

    for (;;) {
        auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
        _ASSERT(reply_item);

        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

        const auto status = reply_item->GetStatus(CDeadline::eInfinite);

        if (status == EPSG_Status::eSuccess) {
            try {
                CJsonResponse check(status, reply_item);
            }
            catch (exception& e) {
                cerr << "Error on reading reply item for request '" << request_id << "': " << e.what() << endl;
                return SExitCode::eRunError;
            }

        } else if (!expect_errors) {
            cerr << "Fail on getting item for request '" << request_id << "' expected to succeed" << endl;
            return SExitCode::eTestFail;

        } else {
            no_errors = false;
        }
    }

    if (expect_errors && no_errors) {
        cerr << "Success on getting all items for request '" << request_id << "' expected to fail" << endl;
        return SExitCode::eTestFail;
    }

    return SExitCode::eSuccess;
}

int CProcessing::Testing()
{
    CPSG_Queue queue(TPSG_ServiceName::GetDefault());
    ifstream input_file("psg_client_test.json");
    SIoRedirector ior(cin, input_file);

    if (!input_file) {
        cerr << "Failed to read 'psg_client_test.json'" << endl;
        return SExitCode::eRunError;
    }

    auto requests = ReadCommands(&STestingContext::CreateContext);

    if (requests.empty()) return SExitCode::eRunError;

    SExitCode rv;

    for (const auto& request : requests) {
        auto expected_result = request->GetUserContext<STestingContext>();
        const auto& request_id = *expected_result;

        _VERIFY(queue.SendRequest(request, CDeadline::eInfinite));

        auto reply = queue.GetNextReply(CDeadline::eInfinite);

        _ASSERT(reply);

        auto received_request = reply->GetRequest();

        _ASSERT(request.get() == received_request.get());

        const bool expect_reply_errors = expected_result->expected == STestingContext::eReplyError;

        if (reply->GetStatus(CDeadline::eInfinite) != EPSG_Status::eSuccess) {
            if (!expect_reply_errors) {
                rv = SExitCode::eTestFail;
                s_ReportErrors(request_id, move(reply));
            }

        } else if (expect_reply_errors) {
            rv = SExitCode::eTestFail;
            cerr << "Success for request '" << request_id << "' expected to fail" << endl;

        } else {
            const bool expect_item_errors = expected_result->expected == STestingContext::eReplyItemError;
            rv = s_CheckItems(expect_item_errors, request_id, reply);
        }
    }

    return rv;
}

bool CProcessing::ReadLine(string& line, istream& is)
{
    for (;;) {
        if (!getline(is, line)) {
            return false;
        } else if (!line.empty()) {
            return true;
        }
    }
}

SInteractiveNewRequestStart::SInteractiveNewRequestStart(CJson_ConstNode params)
{
    // All JSON types have already been validated with the scheme

    auto params_obj = params.GetObject();
    auto context = params_obj.find("context");
    auto& ctx = CDiagContext::GetRequestContext();

    if (context != params_obj.end()) {
        auto context_obj = context->value.GetObject();

        auto sid = context_obj.find("sid");

        if (sid != context_obj.end()) {
            ctx.SetSessionID(sid->value.GetValue().GetString());
        }

        auto phid = context_obj.find("phid");

        if (phid != context_obj.end()) {
            ctx.SetHitID(phid->value.GetValue().GetString());
        }

        auto client_ip = context_obj.find("client_ip");

        if (client_ip != context_obj.end()) {
            ctx.SetClientIP(client_ip->value.GetValue().GetString());
        }
    }

    if (!ctx.IsSetSessionID()) ctx.SetSessionID();
    if (!ctx.IsSetHitID())     ctx.SetHitID();

    SExtra extra;
    extra.Print("params", params);
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstValue json)
{
    _ASSERT(json.IsNumber());

    if (json.IsInt4()) {
        Print(prefix, json.GetInt4());
    } else if (json.IsUint4()) {
        Print(prefix, json.GetUint4());
    } else if (json.IsInt8()) {
        Print(prefix, json.GetInt8());
    } else if (json.IsUint8()) {
        Print(prefix, json.GetUint8());
    } else if (json.IsDouble()) {
        Print(prefix, json.GetDouble());
    } else {
        _TROUBLE;
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstArray json)
{
    for (size_t i = 0; i < json.size(); ++i) {
        Print(prefix + '[' + to_string(i) + ']', json[i]);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstObject json)
{
    for (const auto& pair : json) {
        Print(prefix + '.' + pair.name, pair.value);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstNode json)
{
    switch (json.GetType()) {
        case CJson_ConstNode::eNull:
            Print(prefix, "<null>");
            break;

        case CJson_ConstNode::eBool:
            Print(prefix, json.GetValue().GetBool() ? "true" : "false");
            break;

        case CJson_ConstNode::eString:
            Print(prefix, json.GetValue().GetString());
            break;

        case CJson_ConstNode::eNumber:
            Print(prefix, json.GetValue());
            break;

        case CJson_ConstNode::eArray:
            Print(prefix, json.GetArray());
            break;

        case CJson_ConstNode::eObject:
            Print(prefix, json.GetObject());
            break;
    };
}

int CProcessing::ParallelProcessing(const CArgs& args, bool batch_resolve, bool echo)
{
    const string input_file = batch_resolve ? "id-file" : "input-file";
    const auto& service = args["service"].AsString();
    const auto pipe = args[input_file].AsString() == "-";
    auto& is = pipe ? cin : args[input_file].AsInputFile();

    CParallelProcessing parallel_processing(service, pipe, args, echo, batch_resolve);
    string line;

    while (ReadLine(line, is)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty
        parallel_processing(move(line));
    }

    return 0;
}

class CPSG_Request_Io : public CPSG_Request
{
public:
    CPSG_Request_Io(size_t size) :
        m_AbsPathRef("/TEST/io?return_data_size=" + to_string(size))
    {}

private:
    string x_GetType() const override { return "io"; }
    string x_GetId() const override { return ""; }
    string x_GetAbsPathRef() const override { return m_AbsPathRef; }

    const string m_AbsPathRef;
};

struct SIoContext
{
    const string service;
    const size_t size;
    mutex m;
    condition_variable cv;

    SIoContext(const string& s, size_t z) : service(s), size(z), m_Work(true) {}

    bool Work() const { return m_Work; }
    void Stop() { m_Work = false; }

private:
    atomic<bool> m_Work;
};

struct SIoWorker
{
    size_t errors = 0;

    SIoWorker(SIoContext& context) :
        m_Context(context),
        m_Thread(&SIoWorker::Do, this)
    {}

    void Do();
    void Join() { m_Thread.join(); }

private:
    SIoContext& m_Context;
    thread m_Thread;
    deque<shared_ptr<CPSG_Reply>> m_Replies;
};

void SIoWorker::Do()
{
    const CDeadline kInfinite = CDeadline::eInfinite;

    CPSG_Queue queue(m_Context.service);
    auto request = make_shared<CPSG_Request_Io>(m_Context.size);
    ostringstream err_stream;

    // Wait
    {
        unique_lock<mutex> lock(m_Context.m);
        m_Context.cv.wait(lock);
    }

    // Submit requests and receive response
    while (m_Context.Work()) {
        // Submit
        _VERIFY(queue.SendRequest(request, kInfinite));

        // Response
        auto reply = queue.GetNextReply(kInfinite);
        _ASSERT(reply);

        // Store the reply for now to prevent internal metrics from being written to cout (affects performance)
        m_Replies.emplace_back(reply);

        auto reply_status = reply->GetStatus(kInfinite);
        bool success = reply_status == EPSG_Status::eSuccess;

        if (!success) {
            err_stream << "Warning: Reply error status " << (int)reply_status << "\n";

            for (;;) {
                auto message = reply->GetNextMessage();

                if (message.empty()) break;

                err_stream << "Warning: Reply error message '" << message << "'\n";
            }
        }

        // Items
        while (m_Context.Work()) {
            auto reply_item = reply->GetNextItem(kInfinite);
            _ASSERT(reply_item);

            if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

            auto item_status = reply_item->GetStatus(kInfinite);

            if (item_status != EPSG_Status::eSuccess) {
                success = false;
                err_stream << "Warning: Item error status " << (int)item_status << "\n";

                for (;;) {
                    auto message = reply->GetNextMessage();

                    if (message.empty()) break;

                    err_stream << "Warning: Item error message '" << message << "'\n";
                }
            }
        }

        if (!m_Context.Work()) break;

        if (!success) ++errors;
    }

    cerr << err_stream.str();
};

struct SIoOutput : stringstream, SIoRedirector
{
    SIoOutput() : SIoRedirector(cout, *this) {}

    void Reset() { SIoRedirector::Reset(); stringstream::seekg(0); }
    void Output(size_t errors);
};

void SIoOutput::Output(size_t errors)
{
    map<size_t, vector<SMessage>> raw_data;

    while (*this) {
        size_t request;
        SMessage message;

        if ((*this >> request >> message) && (message.type != SMetricType::eError)) {
            raw_data[request].emplace_back(std::move(message));
        }
    }

    cout << "Requests: " << raw_data.size() << endl;
    cout << "Errors: " << errors << endl;

    vector<double> stats;
    stats.reserve(raw_data.size());

    for (const auto& pair: raw_data) {
        const auto& messages = pair.second;
        auto send = find_if(messages.begin(), messages.end(), SMessage::IsSameType<SMetricType::eSend>);

        if (send == messages.end()) {
            cerr << "Warning: Cannot find event 'Send' for request '" << pair.first << endl;
            continue;
        }

        auto close = find_if(messages.begin(), messages.end(), SMessage::IsSameType<SMetricType::eClose>);

        if (close == messages.end()) {
            cerr << "Warning: Cannot find event 'Close' for request '" << pair.first << endl;
            continue;
        }

        stats.emplace_back(close->milliseconds - send->milliseconds);
    }


    const size_t size = stats.size();
    const double avg = accumulate(stats.begin(), stats.end(), 0.0) / size;
    sort(stats.begin(), stats.end());

    cout << "Avg: " << avg << endl;
    cout << "Min: " << stats.front() << endl;
    cout << " 1%: " << stats[size_t(max(0.0, 0.01 * size - 1))] << endl;
    cout << "10%: " << stats[size_t(max(0.0, 0.10 * size - 1))] << endl;
    cout << "25%: " << stats[size_t(max(0.0, 0.25 * size - 1))] << endl;
    cout << "50%: " << stats[size_t(max(0.0, 0.50 * size - 1))] << endl;
    cout << "75%: " << stats[size_t(max(0.0, 0.75 * size - 1))] << endl;
    cout << "90%: " << stats[size_t(max(0.0, 0.90 * size - 1))] << endl;
    cout << "99%: " << stats[size_t(max(0.0, 0.99 * size - 1))] << endl;
    cout << "Max: " << stats.back() << endl;
}

int CProcessing::Io(const string& service, time_t start_time, int duration, int user_threads, int download_size)
{
    SIoOutput io_output;

    // Half a second delay between threads start and actual work
    chrono::milliseconds kWarmUpDelay(500);

    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eIo);

    auto now = chrono::system_clock::now();
    auto start = chrono::system_clock::from_time_t(start_time);
    auto sleep = chrono::duration_cast<chrono::milliseconds>(start - now) - kWarmUpDelay;

    if (sleep.count() <= 0) {
        cerr << "Warning: Start time (" << start_time << ") has already passed or too close\n";
        sleep = chrono::milliseconds::zero();
    }

    this_thread::sleep_for(sleep);

    SIoContext context(service, download_size);

    vector<SIoWorker> threads;
    threads.reserve(user_threads);

    // Start threads in advance so it won't affect metrics
    for (int i = 0; i < user_threads; ++i) {
        threads.emplace_back(context);
    }

    this_thread::sleep_for(kWarmUpDelay);
    context.cv.notify_all();

    if (duration < 1) {
        cerr << "Warning: Duration (" << duration << ") is less that a second\n";
    } else {
        this_thread::sleep_for(chrono::seconds(duration));
    }

    context.Stop();

    size_t errors = 0;

    for (auto& t : threads) {
        t.Join();

        errors += t.errors;
    }

    // Make internal metrics be written to (redirected) cout
    threads.clear();

    // Report statistics
    auto start_format = CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeFrac);
    CTime start_ctime(start_time);
    auto start_ctime_str = start_ctime.GetLocalTime().AsString(start_format);

    io_output.Reset();

    cout << "Start: " << start_ctime_str << " = " << start_ctime.GetTimeT() << "." << setfill('0') << setw(3) << start_ctime.MilliSecond() << endl;
    cout << "Duration: " << static_cast<double>(duration) << endl;
    cout << "Threads: " << user_threads << endl;
    cout << "Size: " << download_size << endl;

    io_output.Output(errors);
    return 0;
}

CPSG_BioId::TType SRequestBuilder::GetBioIdType(string type)
{
    CObjectTypeInfo info(objects::CSeq_id::GetTypeInfo());

    if (auto index = info.FindVariantIndex(type)) return static_cast<CPSG_BioId::TType>(index);
    if (auto value = objects::CSeq_id::WhichInverseSeqId(type)) return value;

    return static_cast<CPSG_BioId::TType>(atoi(type.c_str()));
}

CPSG_BioId SRequestBuilder::GetBioId(const CArgs& input)
{
    const auto& id = input["ID"].AsString();

    if (!input["type"].HasValue()) return CPSG_BioId(id);

    const auto type = GetBioIdType(input["type"].AsString());
    return CPSG_BioId(id, type);
}

CPSG_BioId SRequestBuilder::GetBioId(const CJson_ConstObject& input)
{
    auto array = input["bio_id"].GetArray();
    auto id = array[0].GetValue().GetString();

    if (array.size() == 1) return CPSG_BioId(id);

    auto value = array[1].GetValue();
    auto type = value.IsString() ? GetBioIdType(value.GetString()) : static_cast<CPSG_BioId::TType>(value.GetInt4());
    return CPSG_BioId(id, type);
}

vector<string> SRequestBuilder::GetNamedAnnots(const CJson_ConstObject& input)
{
    auto na_array = input["named_annots"].GetArray();
    CPSG_Request_NamedAnnotInfo::TAnnotNames names;

    for (const auto& na : na_array) {
        names.push_back(na.GetValue().GetString());
    }

    return names;
}

CPSG_Request_Resolve::TIncludeInfo SRequestBuilder::GetIncludeInfo(TSpecified specified)
{
    const auto& info_flags = GetInfoFlags();

    auto i = info_flags.begin();
    bool all_info_except = specified(i->name);
    unsigned include_info = all_info_except ? CPSG_Request_Resolve::fAllInfo : 0u;

    for (++i; i != info_flags.end(); ++i) {
        if (specified(i->name)) {
            if (all_info_except) {
                include_info &= ~i->value;
            } else {
                include_info |= i->value;
            }
        }
    }

    // Provide all info if nothing is specified explicitly
    return include_info ? include_info : CPSG_Request_Resolve::fAllInfo;
}

void SRequestBuilder::ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CArgs& input)
{
    if (!input["exclude-blob"].HasValue()) return;

    auto blob_ids = input["exclude-blob"].GetStringList();

    for (const auto& blob_id : blob_ids) {
        request->ExcludeTSE(blob_id);
    }
}

void SRequestBuilder::ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CJson_ConstObject& input)
{
    if (!input.has("exclude_blobs")) return;

    auto blob_ids = input["exclude_blobs"].GetArray();

    for (const auto& blob_id : blob_ids) {
        request->ExcludeTSE(blob_id.GetValue().GetString());
    }
}

const initializer_list<SDataFlag> kDataFlags =
{
    { "no-tse",    "Return only the info",                                  CPSG_Request_Biodata::eNoTSE    },
    { "slim-tse",  "Return split info blob if available, or nothing",       CPSG_Request_Biodata::eSlimTSE  },
    { "smart-tse", "Return split info blob if available, or original blob", CPSG_Request_Biodata::eSmartTSE },
    { "whole-tse", "Return all split blobs if available, or original blob", CPSG_Request_Biodata::eWholeTSE },
    { "orig-tse",  "Return original blob",                                  CPSG_Request_Biodata::eOrigTSE  },
};

const initializer_list<SDataFlag>& SRequestBuilder::GetDataFlags()
{
    return kDataFlags;
}

const initializer_list<SInfoFlag> kInfoFlags =
{
    { "all-info-except", "Return all info except explicitly specified by other flags", CPSG_Request_Resolve::fAllInfo },
    { "canonical-id",    "Return canonical ID info",                    CPSG_Request_Resolve::fCanonicalId  },
    { "other-ids",       "Return other IDs info",                       CPSG_Request_Resolve::fOtherIds     },
    { "molecule-type",   "Return molecule type info",                   CPSG_Request_Resolve::fMoleculeType },
    { "length",          "Return length info",                          CPSG_Request_Resolve::fLength       },
    { "state",           "Return state info",                           CPSG_Request_Resolve::fState        },
    { "blob-id",         "Return blob ID info",                         CPSG_Request_Resolve::fBlobId       },
    { "tax-id",          "Return tax ID info",                          CPSG_Request_Resolve::fTaxId        },
    { "hash",            "Return hash info",                            CPSG_Request_Resolve::fHash         },
    { "date-changed",    "Return date changed info",                    CPSG_Request_Resolve::fDateChanged  },
};

const initializer_list<SInfoFlag>& SRequestBuilder::GetInfoFlags()
{
    return kInfoFlags;
}

CJson_Document CProcessing::RequestSchema()
{
    return CJson_Document(R"REQUEST_SCHEMA(
{
    "$schema": "http://json-schema.org/schema#",
    "type": "object",
    "definitions": {
        "jsonrpc": {
            "$id": "#jsonrpc",
            "enum": [ "2.0" ]
        },
        "id": {
            "$id": "#id",
            "type": "string"
        },
        "bio_id": {
            "$id": "#bio_id",
            "type": "array",
            "items": {
                "type": "string"
            },
            "minItems": 1,
            "maxItems": 2
        },
        "include_data": {
            "$id": "#include_data",
            "enum": [
                "no-tse",
                "slim-tse",
                "smart-tse",
                "whole-tse",
                "orig-tse"
            ]
        },
        "include_info": {
            "$id": "#include_info",
            "type": "array",
            "items": {
                "type": "string",
                "enum": [
                    "all-info-except",
                    "canonical-id",
                    "other-ids",
                    "molecule-type",
                    "length",
                    "state",
                    "blob-id",
                    "tax-id",
                    "hash",
                    "date-changed"
                ]
            },
            "uniqueItems": true
        },
        "named_annots": {
            "$id": "#named_annots",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "exclude_blobs": {
            "$id": "#exclude_blobs",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "context": {
            "$id": "#context",
            "type": "object",
            "items": {
                "sid": { "type": "string" },
                "phid": { "type": "string" },
                "client_ip": { "type": "string" }
            }
        }
    },
    "oneOf": [
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "biodata" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#bio_id" },
                        "include_data": { "$ref": "#include_data" },
                        "exclude_blobs": { "$ref": "#exclude_blobs" },
                        "context": { "$ref": "#context" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "blob" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "blob_id": { "type": "string" },
                        "last_modified": { "type": "string" },
                        "include_data": { "$ref": "#include_data" },
                        "context": { "$ref": "#context" }
                    },
                    "required": [ "blob_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "resolve" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#bio_id" },
                        "include_info": { "$ref": "#include_info" },
                        "context": { "$ref": "#context" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "named_annot" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#bio_id" },
                        "named_annots": { "$ref": "#named_annots" },
                        "context": { "$ref": "#context" }
                    },
                    "required": [ "bio_id","named_annots" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$rev": "#jsonrpc" },
                "method": { "enum": [ "tse_chunk" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "blob_id": { "type": "string" },
                        "chunk_no": { "type": "number" },
                        "split_ver": { "type": "number" },
                        "context": { "$ref": "#context" }
                    },
                    "required": [ "blob_id", "chunk_no", "split_ver" ]
                },
                "id": { "$ref": "#id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        }
    ]
}
        )REQUEST_SCHEMA");
}

END_NCBI_SCOPE
