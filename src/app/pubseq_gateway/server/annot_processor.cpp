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
 * Authors: Sergey Satskiy
 *
 * File Description: named annotation processor
 *
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/regexp.hpp>

#include "annot_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "named_annot_callback.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;


CPSGS_AnnotProcessor::CPSGS_AnnotProcessor() :
    m_AnnotRequest(nullptr),
    m_Cancelled(false)
{}


CPSGS_AnnotProcessor::CPSGS_AnnotProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                vector<string> &  valid_names) :
    CPSGS_CassProcessorBase(request, reply),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_AnnotProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_AnnotProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4)),
    m_ValidNames(move(valid_names)),
    m_Cancelled(false)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_AnnotRequest>() everywhere
    m_AnnotRequest = & request->GetRequest<SPSGS_AnnotRequest>();
}


CPSGS_AnnotProcessor::~CPSGS_AnnotProcessor()
{}


IPSGS_Processor*
CPSGS_AnnotProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                      shared_ptr<CPSGS_Reply> reply) const
{
    if (request->GetRequestType() != CPSGS_Request::ePSGS_AnnotationRequest)
        return nullptr;

    auto    valid_annots = x_FilterNames(request);
    if (valid_annots.empty())
        return nullptr;

    return new CPSGS_AnnotProcessor(request, reply, valid_annots);
}


vector<string>
CPSGS_AnnotProcessor::x_FilterNames(shared_ptr<CPSGS_Request> request)
{
    vector<string>  valid_annots;
    for (const auto &  name : request->GetRequest<SPSGS_AnnotRequest>().m_Names) {
        if (x_IsNameValid(name))
            valid_annots.push_back(name);
    }
    return valid_annots;
}


bool CPSGS_AnnotProcessor::x_IsNameValid(const string &  name)
{
    static CRegexp  regexp("^NA\\d+\\.\\d+$", CRegexp::fCompile_ignore_case);
    return regexp.IsMatch(name);
}


void CPSGS_AnnotProcessor::Process(void)
{
    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_AnnotProcessor::x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (status != CRequestStatus::e404_NotFound)
        UpdateOverallStatus(status);
    PSG_WARNING(message);

    if (status == CRequestStatus::e404_NotFound) {
        size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
        IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, message,
                                                       status,
                                                       ePSGS_NoBioseqInfo,
                                                       eDiag_Error);
        IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, 2);
    } else {
        IPSGS_Processor::m_Reply->PrepareReplyMessage(message, status,
                                                      code, severity);
    }

    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
}


// This callback is called only in case of a valid resolution
void
CPSGS_AnnotProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    // Initiate annotation request
    auto *                          app = CPubseqGatewayApp::GetInstance();
    vector<pair<string, int32_t>>   bioseq_na_keyspaces =
                CPubseqGatewayApp::GetInstance()->GetBioseqNAKeyspaces();

    for (const auto &  bioseq_na_keyspace : bioseq_na_keyspaces) {
        unique_ptr<CCassNamedAnnotFetch>   details;
        details.reset(new CCassNamedAnnotFetch(*m_AnnotRequest));

        CCassNAnnotTaskFetch *  fetch_task =
                new CCassNAnnotTaskFetch(app->GetCassandraTimeout(),
                                         app->GetCassandraMaxRetries(),
                                         app->GetCassandraConnection(),
                                         bioseq_na_keyspace.first,
                                         bioseq_resolution.m_BioseqInfo.GetAccession(),
                                         bioseq_resolution.m_BioseqInfo.GetVersion(),
                                         bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                         m_ValidNames,
                                         nullptr, nullptr);
        details->SetLoader(fetch_task);

        fetch_task->SetConsumeCallback(
            CNamedAnnotationCallback(
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotData,
                     this, _1, _2, _3, _4),
                details.get(), bioseq_na_keyspace.second));
        fetch_task->SetErrorCB(
            CNamedAnnotationErrorCallback(
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotError,
                     this, _1, _2, _3, _4, _5),
                details.get()));
        fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
                ToJson(*fetch_task).Repr(CJsonNode::fStandardJson),
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        m_FetchDetails.push_back(move(details));
    }

    // Initiate the retrieval loop
    for (auto &  fetch_details: m_FetchDetails) {
        if (fetch_details)
            if (!fetch_details->ReadFinished())
                fetch_details->GetLoader()->Wait();
    }
}


void
CPSGS_AnnotProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);

    size_t  item_id = IPSGS_Processor::m_Reply->GetItemId();
    auto    data_to_send = ToJson(bioseq_resolution.m_BioseqInfo,
                                  SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                                        Repr(CJsonNode::fStandardJson);

    IPSGS_Processor::m_Reply->PrepareBioseqData(
            item_id, data_to_send, SPSGS_ResolveRequest::ePSGS_JsonFormat);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, 2);
}


bool
CPSGS_AnnotProcessor::x_OnNamedAnnotData(CNAnnotRecord &&  annot_record,
                                         bool  last,
                                         CCassNamedAnnotFetch *  fetch_details,
                                         int32_t  sat)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->SetReadFinished();
        return false;
    }
    if (IPSGS_Processor::m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");

        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
        return false;
    }

    if (last) {
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Named annotation no-more-data callback",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
        fetch_details->SetReadFinished();

        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
        return false;
    }

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Named annotation data received",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }
    IPSGS_Processor::m_Reply->PrepareNamedAnnotationData(
        annot_record.GetAnnotName(),
        ToJson(annot_record, sat).Repr(CJsonNode::fStandardJson));

    x_Peek(false);
    return true;
}


void
CPSGS_AnnotProcessor::x_OnNamedAnnotError(CCassNamedAnnotFetch *  fetch_details,
                                          CRequestStatus::ECode  status,
                                          int  code,
                                          EDiagSev  severity,
                                          const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    auto *  app = CPubseqGatewayApp::GetInstance();
    PSG_ERROR(message);

    if (is_error) {
        if (code == CCassandraException::eQueryTimeout)
            app->GetErrorCounters().IncCassQueryTimeoutError();
        else
            app->GetErrorCounters().IncUnknownError();
    }

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Named annotation error callback",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    IPSGS_Processor::m_Reply->PrepareReplyMessage(
        message, CRequestStatus::e500_InternalServerError, code, severity);
    if (is_error) {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        // There will be no more activity
        fetch_details->SetReadFinished();
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        x_Peek(false);
    }
}


void CPSGS_AnnotProcessor::Cancel(void)
{
    m_Cancelled = true;
}


IPSGS_Processor::EPSGS_Status CPSGS_AnnotProcessor::GetStatus(void)
{
    return CPSGS_CassProcessorBase::GetStatus();
}


string CPSGS_AnnotProcessor::GetName(void) const
{
    return "LMDB cache/Cassandra named annotations processor";
}


void CPSGS_AnnotProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_AnnotProcessor::x_Peek(bool  need_wait)
{
    if (m_Cancelled)
        return;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    for (auto &  details: m_FetchDetails) {
        if (details)
            x_Peek(details, need_wait);
    }

    // Ready packets needs to be send only once when everything is finished
    if (IPSGS_Processor::m_Reply->IsOutputReady())
        if (AreAllFinishedRead())
            IPSGS_Processor::m_Reply->Flush(false);
}


void CPSGS_AnnotProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                  bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return;

    if (need_wait)
        if (!fetch_details->ReadFinished())
            fetch_details->GetLoader()->Wait();

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetErrorCounters().IncUnknownError();
        PSG_ERROR(error);

        IPSGS_Processor::m_Reply->PrepareReplyMessage(
                error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    }
}

