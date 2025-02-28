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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*           Structures used by CScope
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/scope.hpp>

#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/bioseq_set_handle.hpp>

#include <corelib/ncbi_param.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if 0
# define _TRACE_TSE_LOCK(x) _TRACE(x)
#else
# define _TRACE_TSE_LOCK(x) ((void)0)
#endif


NCBI_PARAM_DECL(bool, OBJMGR, SCOPE_AUTORELEASE);
NCBI_PARAM_DEF_EX(bool, OBJMGR, SCOPE_AUTORELEASE, true,
                  eParam_NoThread, OBJMGR_SCOPE_AUTORELEASE);

static bool s_GetScopeAutoReleaseEnabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJMGR, SCOPE_AUTORELEASE)> sx_Value;
    return sx_Value->Get();
}


NCBI_PARAM_DECL(unsigned, OBJMGR, SCOPE_AUTORELEASE_SIZE);
NCBI_PARAM_DEF_EX(unsigned, OBJMGR, SCOPE_AUTORELEASE_SIZE, 10,
                  eParam_NoThread, OBJMGR_SCOPE_AUTORELEASE_SIZE);

static unsigned s_GetScopeAutoReleaseSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJMGR, SCOPE_AUTORELEASE_SIZE)> sx_Value;
    return sx_Value->Get();
}


NCBI_PARAM_DECL(int, OBJMGR, SCOPE_POSTPONE_DELETE);
NCBI_PARAM_DEF_EX(int, OBJMGR, SCOPE_POSTPONE_DELETE, 1,
                  eParam_NoThread, OBJMGR_SCOPE_POSTPONE_DELETE);

static int s_GetScopePostponeDelete(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJMGR, SCOPE_POSTPONE_DELETE)> sx_Value;
    return sx_Value->Get();
}


DEFINE_STATIC_FAST_MUTEX(sx_UsedTSEMutex);


/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

CDataSource_ScopeInfo::CDataSource_ScopeInfo(CScope_Impl& scope,
                                             CDataSource& ds)
    : m_Scope(&scope),
      m_DataSource(&ds),
      m_CanBeUnloaded(s_GetScopeAutoReleaseEnabled() &&
                      ds.GetDataLoader() &&
                      ds.GetDataLoader()->CanGetBlobById()),
      m_CanBeEdited(ds.CanBeEdited()),
      m_CanRemoveOnResetHistory(false),
      m_NextTSEIndex(0),
      m_TSE_UnlockQueue(s_GetScopeAutoReleaseSize())
{
}


CDataSource_ScopeInfo::~CDataSource_ScopeInfo(void)
{
    _ASSERT(!m_Scope);
    _ASSERT(!m_DataSource);
}


CScope_Impl& CDataSource_ScopeInfo::GetScopeImpl(void) const
{
    if ( !m_Scope ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CDataSource_ScopeInfo is not attached to CScope");
    }
    return *m_Scope;
}


CDataLoader* CDataSource_ScopeInfo::GetDataLoader(void)
{
    return GetDataSource().GetDataLoader();
}


bool CDataSource_ScopeInfo::IsConst(void) const
{
    return !CanBeEdited() && GetDataSource().CanBeEdited();
}


void CDataSource_ScopeInfo::SetConst(void)
{
    _ASSERT(CanBeEdited());
    _ASSERT(GetDataSource().CanBeEdited());
    m_CanBeEdited = false;
    _ASSERT(IsConst());
}


void CDataSource_ScopeInfo::SetCanRemoveOnResetHistory(void)
{
    _ASSERT(CanBeEdited());
    _ASSERT(GetDataSource().CanBeEdited());
    m_CanRemoveOnResetHistory = true;
    _ASSERT(CanRemoveOnResetHistory());
}


void CDataSource_ScopeInfo::DetachScope(void)
{
    if ( m_Scope ) {
        _ASSERT(m_DataSource);
        ResetDS();
        GetScopeImpl().m_ObjMgr->ReleaseDataSource(m_DataSource);
        _ASSERT(!m_DataSource);
        m_Scope = 0;
    }
}


const CDataSource_ScopeInfo::TTSE_InfoMap&
CDataSource_ScopeInfo::GetTSE_InfoMap(void) const
{
    return m_TSE_InfoMap;
}


const CDataSource_ScopeInfo::TTSE_LockSet&
CDataSource_ScopeInfo::GetTSE_LockSet(void) const
{
    return m_TSE_LockSet;
}


static DECLARE_TLS_VAR(CUnlockedTSEsGuard*, st_Guard);


CUnlockedTSEsGuard::CUnlockedTSEsGuard(void)
{
    if ( !st_Guard ) {
        st_Guard = this;
    }
}


CUnlockedTSEsGuard::~CUnlockedTSEsGuard(void)
{
    if ( st_Guard == this ) {
        while ( !m_UnlockedTSEsInternal.empty() ) {
            TUnlockedTSEsInternal locks;
            swap(m_UnlockedTSEsInternal, locks);
        }
        while ( !m_UnlockedTSEsLock.empty() ) {
            TUnlockedTSEsLock locks;
            swap(m_UnlockedTSEsLock, locks);
        }
        st_Guard = 0;
    }
}


void CUnlockedTSEsGuard::SaveLock(const CTSE_Lock& lock)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsLock.push_back(ConstRef(&*lock));
    }
}


void CUnlockedTSEsGuard::SaveInternal(const CTSE_ScopeInternalLock& lock)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsInternal.push_back(lock);
    }
}


void CUnlockedTSEsGuard::SaveInternal(const TUnlockedTSEsInternal& locks)
{
    if ( !s_GetScopePostponeDelete() ) {
        return;
    }
    _ASSERT(st_Guard);
    if ( CUnlockedTSEsGuard* guard = st_Guard ) {
        guard->m_UnlockedTSEsInternal.insert
            (guard->m_UnlockedTSEsInternal.end(), locks.begin(), locks.end());
    }
}


void CDataSource_ScopeInfo::RemoveTSE_Lock(const CTSE_Lock& lock)
{
    CUnlockedTSEsGuard::SaveLock(lock);
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    _VERIFY(m_TSE_LockSet.RemoveLock(lock));
}


void CDataSource_ScopeInfo::AddTSE_Lock(const CTSE_Lock& lock)
{
    TTSE_LockSetMutex::TWriteLockGuard guard(m_TSE_LockSetMutex);
    _VERIFY(m_TSE_LockSet.AddLock(lock));
}

//
// CDataSource_ScopeInfo fields:
//   m_TSE_InfoMap - history TSEs by their Blob-id, they may be unlocked with possibility to reload
//   m_TSE_LockSet - all CTSE_Lock objects for lookup by CDataSource
//   m_TSE_UnlockQueue - queue of CTSE_ScopeInternalLock ready to release their CTSE_Lock
//   m_EditDS, m_ReplacedTSEs - edited entries support
//
// CTSE_ScopeInfo fields:
//   m_ScopeInfoMap keeps track of sub-object ScopeInfo that's used in CSeq_entry_Handle and such.
//     When new handle is created the ScopeInfo object is reused or created
//     ScopeInfo objects are kept until m_TSE_Lock is being held
//     Then m_TSE_Lock is released all ScopeInfo object are detached and released
//   m_UsedTSE_Set rooted tree of dependent TSEs
//   m_UsedByTSE parent pointer in the dependent TSEs tree
//     The tree is cleared when internal lock is released,
//     possibly causing release of internal locks of dependent TSEs.
//
// CScopeInfo<> fields:
//   m_TSE_ScopeInfo - pointer to its TSE
//   m_LockCounter - track count of corresponding handles
//   m_TSE_Handle - holds TSE handle when m_LockCounter > 0
//   m_ObjectInfo - pointer to CDataSource object
//   m_DetachedInfo - mapping of sub-objects to CScopeInfo when object is removed from TSE (edit)
//   
/////////////////////////////////////////////////////////////////////////////
// CScope TSE locking scheme.
//
// CTSE_ScopeUserLock is a link from user handles, it's also an internal lock.
//
// User lock maintains m_UserLockCounter, and when the last handle is deleted
// the TSE is put into m_TSE_UnlockQueue.
// The entries pushed out of m_TSE_UnlockQueue lose their internal lock.
//
// The TSE with user locks can be forcily released by explicit request:
// RemoveFromHistory(), RemoveDataLoader(), ResetHistory() etc.
// In this case all handles become invalidated - disconnected from scope.
// When a TSE is being forcily released the CTSE_ScopeInfo will remain
// as it may be referenced from other user-level handles, but the content
// of CTSE_ScopeInfo is cleared - info->IsAttached() == false.

// CTSE_ScopeInternalLock lock holds CTSE_Lock m_TSE_lock (in CDataSource).
//
// Internal locks are either links from user handles,
// links from another TSE (master to segments, sequence to external annots),
// or entries in m_TSE_UnlockQueue.
// Internal lock maintains m_TSE_LockCounter, and when the last internal lock
// is deleted the m_TSE_lock will be released if it's possible to reload
// the TSE later.
// When an internal lock is obtained it will be assigned with proper CTSE_Lock
// if necessary, which could involve reloading of entry from CDataLoader.

// So, possible states of CTSE_ScopeInfo are:
//   0 = Detached: detached from DS & Scope, can be only deleted
//     m_DS_Info == null
//     m_TSE_LockCounter >= 0
//     m_UserLockCounter >= 0
//     m_TSE_Lock == null
//     m_UnloadedInfo == null
//     m_UsedByTSE, m_UsedTSE_Set: empty
//     m_ReplacedTSE == null
//     m_BioseqById, m_ScopeInfoMap: empty
//     not in m_TSE_UnlockQueue
//   1 = Unlocked: attached, unlocked, can be locked again
//     m_DS_Info != null
//     m_TSE_LockCounter == 0
//     m_UserLockCounter == 0
//     m_TSE_Lock, m_UnloadedInfo: exactly one of them is null
//     m_UsedByTSE, m_UsedTSE_Set: empty
//     m_ReplacedTSE
//     m_BioseqById, m_ScopeInfoMap
//     not in m_TSE_UnlockQueue
//   2 = InternalLocked: attached, locked, no user handle locks
//     m_DS_Info != null
//     m_TSE_LockCounter > 0
//     m_UserLockCounter == 0
//     m_TSE_Lock != null
//     m_UnloadedInfo: any
//     m_UsedByTSE, m_UsedTSE_Set: any
//     m_ReplacedTSE: any
//     m_BioseqById: any
//     m_ScopeInfoMap: any
//     may be m_TSE_UnlockQueue
//   3 = UserLocked: attached, locked, with user handle locks
//     m_DS_Info != null
//     m_TSE_LockCounter > 0
//     m_UserLockCounter > 0
//     m_TSE_Lock != null
//     m_UnloadedInfo: any
//     m_UsedByTSE, m_UsedTSE_Set: any
//     m_ReplacedTSE: any
//     m_BioseqById: any
//     m_ScopeInfoMap: any
//     not in m_TSE_UnlockQueue
//
// Notable conditions:
//     (m_TSE_LockCounter == 0) => (not in m_TSE_UnlockQueue && not in UsedByTSE tree)
//     (m_TSE_LockCounter > 0) => (m_TSE_Lock != null)
//     (m_UserLockCounter > 0) => (not in m_TSE_UnlockQueue)
//
// TSE state changes:
//   3 -> 2 when last user handle is destroyed (m_UserLockCounter becomes zero).
//     method - CTSE_ScopeUserLock -> CTSE_ScopeInternalLock
//     : put it in unlock queue
//     : possibly release TSE pushed out from unlock queue (via InternalLock mechanics)
//   2 -> 1 when last internal lock is released (e.g. pushed out of unlock queue).
//     method - CTSE_ScopeInternalLock -> CRef<CTSE_ScopeInfo>(?)
//     : release internal locks of used TSEs
//   1 -> 2 when internal lock is obtained (user handle or TSE dependency).
//     method - CRef<CTSE_ScopeInfo> -> CTSE_ScopeInternalLock
//     : aquire TSE lock if necessary
//   2 -> 3 when user handle is created
//     method - CTSE_ScopeInternalLock -> CTSE_ScopeUserLock
//     : remove from unlock queue
//   1,2,3 -> 0 when TSE is forcely released (RemoveFromHistory() or similar).
//     method - CTSE_ScopeUserLock -> null
//   0 -> nothing
//
// CScopeObject states:
//   0 = Detached completely: detached from Scope, DS, and TSE
//     m_TSE_ScopeInfo == null
//     m_LockCounter: any (number of existing handles)
//     m_TSE_Handle == null
//     m_ObjectInfo == null
//     m_DetachedInfo == null
//   1 = Detached: detached from Scope, DS, and TSE, can be reattached by user API
//     m_TSE_ScopeInfo == null
//     m_LockCounter: any (number of existing handles)
//     m_TSE_Handle == null
//     m_ObjectInfo != null
//     m_DetachedInfo != null
//   2 = Unlocked: CScopeObject is allocated but not used in any handle
//     m_TSE_ScopeInfo != null
//     m_LockCounter == 0
//     m_TSE_Handle == null
//     m_ObjectInfo != null
//     m_DetachedInfo == null
//   3 = UserLocked: there are handles to this object
//     m_TSE_ScopeInfo != null
//     m_LockCounter > 0
//     m_TSE_Handle != null
//     m_ObjectInfo != null
//     m_DetachedInfo == null
//
// CScopeObject state changes:
//   3 -> 2 when last user handle is destroyed (m_LockCounter becomes zero).
//     method - x_LastReferenceReleased()
//     : clear m_TSE_Handle
//     : release old TSE_Handle out of mutex lock
//   2 -> 3 when user handle is obtained
//     methods - GetAnnotLock() etc
//     : set m_TSE_Handle
//   3,2 -> 0 when TSE is removed from history
//     method - RemoveFromHistory(tse)
//     : clear m_TSE_Handle, m_TSE_ScopeInfo, m_ObjectInfo
//   3 -> 1 when the object is deleted from TSE (edit API)
//     methods - RemoveAnnot() etc
//     : information about all sub-objects is stored into m_DetachedInfo
//     : clear m_TSE_Handle
//   1 -> 3 when the object is attached to a TSE (edit API)
//     methods - AddAnnot() etc
//     : set m_TSE_Handle
//     : m_DetachedInfo is used to restore CScopeObject of sub-objects
//   0 -> nothing

// get user lock for a CTSE_Lock (in CDataSource)
// create CTSE_ScopeInfo if necessary
// preconditions:
//   lock != null
//   m_UserLockCounter >= 0 (any)


CTSE_ScopeUserLock
CDataSource_ScopeInfo::GetTSE_Lock(const CTSE_Lock& lock)
{
    CTSE_ScopeUserLock ret;
    _ASSERT(lock);
    if ( m_EditDS && TSEIsReplaced(lock->GetBlobId()) ) {
        return ret;
    }
    TTSE_ScopeInfo info;
    {{
        {{
            TTSE_InfoMapMutex::TWriteLockGuard guard(m_TSE_InfoMapMutex);
            TTSE_ScopeInfo& slot = m_TSE_InfoMap[lock->GetBlobId()];
            if ( !slot ) {
                slot = info = new CTSE_ScopeInfo(*this, lock,
                                                 m_NextTSEIndex++,
                                                 m_CanBeUnloaded);
                if ( m_CanBeUnloaded ) {
                    // add this TSE into index by SeqId
                    x_IndexTSE(*info);
                }
            }
            else {
                info = slot;
            }
        }}
        _ASSERT(info->IsAttached() && &info->GetDSInfo() == this);
        ++info->m_TSE_LockCounter;
        ++info->m_UserLockCounter;
        {{
            // first remove the TSE from unlock queue
            TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
            // TSE must be locked already by caller
            _ASSERT(info->m_TSE_LockCounter > 0);
            m_TSE_UnlockQueue.Erase(info);
            // TSE must be still locked by caller even after removing it
            // from unlock queue
            _ASSERT(info->m_TSE_LockCounter > 0);
        }}
        info->SetTSE_Lock(lock);
        ret.Reset(info);
        _VERIFY(--info->m_UserLockCounter > 0);
        _VERIFY(--info->m_TSE_LockCounter > 0);
        _ASSERT(info->GetTSE_Lock() == lock);
    }}
    return ret;
}


inline bool CTSE_ScopeInfo::x_TSE_LockIsAssigned() const
{
    return m_TSE_LockAssignState == 2;
}


inline bool CTSE_ScopeInfo::x_TSE_LockIsNotAssigned() const
{
    return m_TSE_LockAssignState == 0;
}


inline bool CTSE_ScopeInfo::x_VerifyTSE_LockIsAssigned() const
{
    return x_TSE_LockIsAssigned() && GetTSE_Lock();
}


inline bool CTSE_ScopeInfo::x_VerifyTSE_LockIsAssigned(const CTSE_Lock& tse) const
{
    return tse && x_VerifyTSE_LockIsAssigned() && GetTSE_Lock() == tse;
}


inline bool CTSE_ScopeInfo::x_VerifyTSE_LockIsAssigned(const CTSE_Info& tse) const
{
    return x_VerifyTSE_LockIsAssigned() && &*GetTSE_Lock() == &tse;
}


inline bool CTSE_ScopeInfo::x_VerifyTSE_LockIsNotAssigned() const
{
    return x_TSE_LockIsNotAssigned() && !GetTSE_Lock();
}


void CDataSource_ScopeInfo::AttachTSE(CTSE_ScopeInfo& info,
                                      const CTSE_Lock& lock)
{
    _ASSERT(m_CanBeUnloaded == info.CanBeUnloaded());
    _ASSERT(!info.m_DS_Info);
    _ASSERT(info.x_VerifyTSE_LockIsNotAssigned());
    _ASSERT(lock && &lock->GetDataSource() == &GetDataSource());
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard(m_TSE_InfoMapMutex);
        _VERIFY(m_TSE_InfoMap.insert(TTSE_InfoMap::value_type
                                     (lock->GetBlobId(),
                                      //STSE_Key(*lock, m_CanBeUnloaded),
                                      Ref(&info))).second);
        if ( m_CanBeUnloaded ) {
            // add this TSE into index by SeqId
            x_IndexTSE(info);
        }
        info.m_DS_Info = this;
    }}
    info.SetTSE_Lock(lock);
}


void CDataSource_ScopeInfo::x_IndexTSE(CTSE_ScopeInfo& tse)
{
    ITERATE ( CTSE_ScopeInfo::TSeqIds, it, tse.GetBioseqsIds() ) {
        m_TSE_BySeqId.insert(TTSE_BySeqId::value_type(*it, Ref(&tse)));
    }
}


void CDataSource_ScopeInfo::x_UnindexTSE(const CTSE_ScopeInfo& tse)
{
    ITERATE ( CTSE_ScopeInfo::TSeqIds, it, tse.GetBioseqsIds() ) {
        TTSE_BySeqId::iterator tse_it = m_TSE_BySeqId.lower_bound(*it);
        while ( tse_it != m_TSE_BySeqId.end() && tse_it->first == *it ) {
            if ( tse_it->second == &tse ) {
                m_TSE_BySeqId.erase(tse_it++);
            }
            else {
                ++tse_it;
            }
        }
    }
}


CDataSource_ScopeInfo::TTSE_ScopeInfo
CDataSource_ScopeInfo::x_FindBestTSEInIndex(const CSeq_id_Handle& idh) const
{
    TTSE_ScopeInfo tse;
    for ( TTSE_BySeqId::const_iterator it = m_TSE_BySeqId.lower_bound(idh);
          it != m_TSE_BySeqId.end() && it->first == idh; ++it ) {
        if ( !tse || x_IsBetter(idh, *it->second, *tse) ) {
            tse = it->second;
        }
    }
    return tse;
}


// Called by destructor of CTSE_ScopeUserLock when lock counter goes to 0
void CDataSource_ScopeInfo::ReleaseTSEUserLock(CTSE_ScopeInfo& tse)
{
    CUnlockedTSEsGuard guard;
    {{
        CTSE_ScopeInternalLock unlocked;
        TTSE_LockSetMutex::TWriteLockGuard tse_guard(m_TSE_UnlockQueueMutex);
        if ( tse.m_UserLockCounter > 0 ) {
            // relocked already
            return;
        }
        if ( !tse.GetTSE_Lock() ) {
            // already unlocked
            return;
        }
        m_TSE_UnlockQueue.Erase(&tse);
        m_TSE_UnlockQueue.Put(&tse, CTSE_ScopeInternalLock(&tse), &unlocked);
        if ( unlocked ) {
            CUnlockedTSEsGuard::SaveInternal(unlocked);
        }
    }}
}


// Called when lock counter becomes non-zero
void CDataSource_ScopeInfo::AcquireTSEUserLock(CTSE_ScopeInfo& tse)
{
    {{
        // TODO: possible deadlock (1), m_TSE_UnlockQueueMutex is taken before m_TSE_LockMutex
        TTSE_LockSetMutex::TWriteLockGuard tse_guard(m_TSE_UnlockQueueMutex);
        m_TSE_UnlockQueue.Erase(&tse);
    }}
    if ( !tse.x_TSE_LockIsAssigned() ) {
        CDataSource_ScopeInfo* ds = tse.m_DS_Info;
        if ( !ds ) {
            --tse.m_UserLockCounter;
            NCBI_THROW(CCoreException, eNullPtr,
                       "CTSE_ScopeInfo is not attached to CScope");
        }
        // obtain lock from CDataSource
        CTSE_Lock lock = tse.m_UnloadedInfo->LockTSE();
        _ASSERT(lock);
        tse.SetTSE_Lock(lock);
        _ASSERT(tse.x_TSE_LockIsAssigned());
        _ASSERT(tse.GetTSE_Lock() == lock);
        _ASSERT(tse.m_UserLockCounter > 0);
    }
    _ASSERT(tse.x_TSE_LockIsAssigned());
}


// Called by destructor of CTSE_ScopeInternalLock when lock counter goes to 0
// CTSE_ScopeInternalLocks are stored in m_TSE_UnlockQueue 
void CDataSource_ScopeInfo::ForgetTSELock(CTSE_ScopeInfo& tse)
{
    if ( tse.m_TSE_LockCounter > 0 ) {
        // relocked already
        return;
    }
    if ( tse.x_TSE_LockIsNotAssigned() ) {
        // already unlocked
        return;
    }
    CUnlockedTSEsGuard guard;
    tse.ForgetTSE_Lock();
}


void CDataSource_ScopeInfo::ResetDS(void)
{
    CUnlockedTSEsGuard guard;
    TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
    {{
        TTSE_UnlockQueue::TUnlockSet unlocked;
        {{
            TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
            m_TSE_UnlockQueue.Clear(&unlocked);
        }}
        if ( !unlocked.empty() ) {
            CUnlockedTSEsGuard::SaveInternal(unlocked);
        }
    }}
    NON_CONST_ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
        it->second->DropTSE_Lock();
        it->second->x_DetachDS();
    }
    m_TSE_InfoMap.clear();
    m_TSE_BySeqId.clear();
    m_ReplacedTSEs.clear();
    {{
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_LockSetMutex);
        m_TSE_LockSet.clear();
    }}
    m_NextTSEIndex = 0;
}


void CDataSource_ScopeInfo::ResetHistory(int action_if_locked)
{
    if ( action_if_locked == CScope::eRemoveIfLocked ) {
        // no checks -> fast reset
        ResetDS();
        return;
    }
    typedef vector< CRef<CTSE_ScopeInfo> > TTSEs;
    TTSEs tses;
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
        tses.reserve(m_TSE_InfoMap.size());
        ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
            if ( it->second->IsUserLocked() ) {
                if ( action_if_locked == CScope::eKeepIfLocked ) {
                    // skip locked TSEs
                    continue;
                }
                if ( action_if_locked == CScope::eThrowIfLocked ) {
                    // there are locked TSEs
                    NCBI_THROW(CObjMgrException, eLockedData,
                               "Cannot reset scope's history "
                               "because TSE is locked");
                }
            }
            tses.push_back(it->second);
        }
    }}
    CUnlockedTSEsGuard guard;
    ITERATE ( TTSEs, it, tses ) {
        RemoveFromHistory(it->GetNCObject());
    }
}


void CDataSource_ScopeInfo::RemoveFromHistory(CTSE_ScopeInfo& tse,
                                              bool drop_from_ds)
{
    tse.ReleaseUsedTSEs();
    {{
        TTSE_InfoMapMutex::TWriteLockGuard guard1(m_TSE_InfoMapMutex);
        if ( tse.CanBeUnloaded() ) {
            x_UnindexTSE(tse);
        }
        tse.RestoreReplacedTSE();
        _VERIFY(m_TSE_InfoMap.erase(tse.GetBlobId()));
    }}
    // prevent storing into m_TSE_UnlockQueue
    _VERIFY(++tse.m_UserLockCounter > 0);
    // remove TSE lock completely
    {{
        // release the TSE recursively outside of mutex
        CTSE_ScopeInternalLock unlocked;
        TTSE_LockSetMutex::TWriteLockGuard guard2(m_TSE_UnlockQueueMutex);
        m_TSE_UnlockQueue.Erase(&tse, &unlocked);
    }}
    if ( CanRemoveOnResetHistory() ||
         (drop_from_ds && GetDataSource().CanBeEdited()) ) {
        // remove TSE from static blob set in DataSource
        CConstRef<CTSE_Info> tse_info(&*tse.GetTSE_Lock());
        tse.ResetTSE_Lock();
        GetDataSource().DropStaticTSE(const_cast<CTSE_Info&>(*tse_info));
    }
    else {
        tse.ResetTSE_Lock();
    }
    tse.x_DetachDS();
    _VERIFY(--tse.m_UserLockCounter >= 0); // restore lock counter
    _ASSERT(!tse.GetTSE_Lock());
    _ASSERT(!tse.m_DS_Info);
}


CDataSource_ScopeInfo::TTSE_Lock
CDataSource_ScopeInfo::FindTSE_Lock(const CSeq_entry& tse)
{
    CDataSource::TTSE_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindTSE_Lock(tse, m_TSE_LockSet);
    }}
    if ( lock ) {
        return GetTSE_Lock(lock);
    }
    return TTSE_Lock();
}


CDataSource_ScopeInfo::TSeq_entry_Lock
CDataSource_ScopeInfo::GetSeq_entry_Lock(const CBlobIdKey& blob_id)
{
    CDataSource::TSeq_entry_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().GetSeq_entry_Lock(blob_id);
    }}
    if ( lock.first ) {
        return TSeq_entry_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_entry_Lock();
}


CDataSource_ScopeInfo::TSeq_entry_Lock
CDataSource_ScopeInfo::FindSeq_entry_Lock(const CSeq_entry& entry)
{
    CDataSource::TSeq_entry_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_entry_Lock(entry, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TSeq_entry_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_entry_Lock();
}


CDataSource_ScopeInfo::TSeq_annot_Lock
CDataSource_ScopeInfo::FindSeq_annot_Lock(const CSeq_annot& annot)
{
    CDataSource::TSeq_annot_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_annot_Lock(annot, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TSeq_annot_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TSeq_annot_Lock();
}


CDataSource_ScopeInfo::TBioseq_set_Lock
CDataSource_ScopeInfo::FindBioseq_set_Lock(const CBioseq_set& seqset)
{
    CDataSource::TBioseq_set_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindBioseq_set_Lock(seqset, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return TBioseq_set_Lock(lock.first, GetTSE_Lock(lock.second));
    }
    return TBioseq_set_Lock();
}


CDataSource_ScopeInfo::TBioseq_Lock
CDataSource_ScopeInfo::FindBioseq_Lock(const CBioseq& bioseq)
{
    CDataSource::TBioseq_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindBioseq_Lock(bioseq, m_TSE_LockSet);
    }}
    if ( lock.first ) {
        return GetTSE_Lock(lock.second)->GetBioseqLock(null, lock.first);
    }
    return TBioseq_Lock();
}


CDataSource_ScopeInfo::TSeq_feat_Lock
CDataSource_ScopeInfo::FindSeq_feat_Lock(const CSeq_id_Handle& loc_id,
                                         TSeqPos loc_pos,
                                         const CSeq_feat& feat)
{
    TSeq_feat_Lock ret;
    CDataSource::TSeq_feat_Lock lock;
    {{
        TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
        lock = GetDataSource().FindSeq_feat_Lock(loc_id, loc_pos, feat);
    }}
    if ( lock.first.first ) {
        ret.first.first = lock.first.first;
        ret.first.second = GetTSE_Lock(lock.first.second);
        ret.second = lock.second;
    }
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::BestResolve(const CSeq_id_Handle& idh,
                                                   int get_flag)
{
    SSeqMatch_Scope ret = x_GetSeqMatch(idh);
    if ( !ret && get_flag == CScope::eGetBioseq_All ) {
        // Try to load the sequence from the data source
        SSeqMatch_DS ds_match = GetDataSource().BestResolve(idh);
        if ( ds_match ) {
            x_SetMatch(ret, ds_match);
        }
    }
#ifdef _DEBUG
    if ( ret ) {
        _ASSERT(ret.m_Seq_id);
        _ASSERT(ret.m_Bioseq);
        _ASSERT(ret.m_TSE_Lock);
        _ASSERT(ret.m_Bioseq == ret.m_TSE_Lock->m_TSE_Lock->FindBioseq(ret.m_Seq_id));
    }
#endif
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::Resolve(const CSeq_id_Handle& idh,
                                               CTSE_ScopeInfo& tse)
{
    SSeqMatch_Scope ret;
    x_SetMatch(ret, tse, idh);
    return ret;
}


map<size_t, SSeqMatch_Scope>
CDataSource_ScopeInfo::ResolveBulk(const map<size_t, CSeq_id_Handle>& ids,
                                   CTSE_ScopeInfo& tse)
{
    map<size_t, SSeqMatch_Scope> ret;
    map<size_t, CConstRef<CBioseq_Info>> bioseqs = tse.GetTSE_Lock()->FindBioseqBulk(ids);
    for ( auto& [ i, idh ] : ids ) {
        SSeqMatch_Scope match;
        match.m_Seq_id = idh;
        match.m_TSE_Lock = CTSE_ScopeUserLock(&tse);
        _ASSERT(match.m_Seq_id);
        _ASSERT(match.m_TSE_Lock);
        match.m_Bioseq = bioseqs[i];
        _ASSERT(match.m_Bioseq);
        _ASSERT(match.m_Bioseq == match.m_TSE_Lock->m_TSE_Lock->FindBioseq(match.m_Seq_id));
        ret[i] = match;
    }
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::x_GetSeqMatch(const CSeq_id_Handle& idh)
{
    SSeqMatch_Scope ret = x_FindBestTSE(idh);
    if ( !ret && idh.HaveMatchingHandles() ) {
        CSeq_id_Handle::TMatches ids;
        idh.GetMatchingHandles(ids, eAllowWeakMatch);
        ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
            if ( *it == idh ) // already checked
                continue;
            if ( ret && ret.m_Seq_id.IsBetter(*it) ) // worse hit
                continue;
            if ( SSeqMatch_Scope match = x_FindBestTSE(*it) ) {
                ret = match;
            }
        }
    }
    return ret;
}


SSeqMatch_Scope CDataSource_ScopeInfo::x_FindBestTSE(const CSeq_id_Handle& idh)
{
    SSeqMatch_Scope ret;
    if ( m_CanBeUnloaded ) {
        // We have full index of static TSEs.
        TTSE_InfoMapMutex::TReadLockGuard guard(GetTSE_InfoMapMutex());
        TTSE_ScopeInfo tse = x_FindBestTSEInIndex(idh);
        if ( tse ) {
            x_SetMatch(ret, *tse, idh);
        }
    }
    else {
        // We have to ask data source about it.
        CDataSource::TSeqMatches matches;
        {{
            TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_LockSetMutex);
            CDataSource::TSeqMatches matches2 =
                GetDataSource().GetMatches(idh, m_TSE_LockSet);
            matches.swap(matches2);
        }}
        ITERATE ( CDataSource::TSeqMatches, it, matches ) {
            SSeqMatch_Scope nxt;
            x_SetMatch(nxt, *it);
            if ( !nxt ) {
                continue;
            }
            if ( !ret || x_IsBetter(idh, *nxt.m_TSE_Lock, *ret.m_TSE_Lock) ) {
                ret = nxt;
            }
        }
    }
    return ret;
}


bool CDataSource_ScopeInfo::x_IsBetter(const CSeq_id_Handle& idh,
                                       const CTSE_ScopeInfo& tse1,
                                       const CTSE_ScopeInfo& tse2)
{
    // First of all we check if we already resolve bioseq with this id.
    bool resolved1 = tse1.HasResolvedBioseq(idh);
    bool resolved2 = tse2.HasResolvedBioseq(idh);
    if ( resolved1 != resolved2 ) {
        return resolved1;
    }
    // Now check TSEs' orders.
    CTSE_ScopeInfo::TBlobOrder order1 = tse1.GetBlobOrder();
    CTSE_ScopeInfo::TBlobOrder order2 = tse2.GetBlobOrder();
    if ( order1 != order2 ) {
        return order1 < order2;
    }

    // Now we have very similar TSE's so we'll prefer the first one added.
    return tse1.GetLoadIndex() < tse2.GetLoadIndex();
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       CTSE_ScopeInfo& tse,
                                       const CSeq_id_Handle& idh) const
{
    match.m_Seq_id = idh;
    match.m_TSE_Lock = CTSE_ScopeUserLock(&tse);
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_TSE_Lock);
    match.m_Bioseq = match.m_TSE_Lock->GetTSE_Lock()->FindBioseq(idh);
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock->m_TSE_Lock->FindBioseq(match.m_Seq_id));
}


void CDataSource_ScopeInfo::x_SetMatch(SSeqMatch_Scope& match,
                                       const SSeqMatch_DS& ds_match)
{
    match.m_TSE_Lock = GetTSE_Lock(ds_match.m_TSE_Lock);
    if ( !match.m_TSE_Lock ) {
        match.m_Seq_id.Reset();
        match.m_Bioseq.Reset();
        return;
    }
    match.m_Seq_id = ds_match.m_Seq_id;
    match.m_Bioseq = ds_match.m_Bioseq;
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_Bioseq);
    _ASSERT(match.m_TSE_Lock);
    _ASSERT(match.m_Bioseq == match.m_TSE_Lock->GetTSE_Lock()->FindBioseq(match.m_Seq_id));
}


void CDataSource_ScopeInfo::GetBlobs(TSeqMatchMap& match_map)
{
    CDataSource::TSeqMatchMap ds_match_map;
    ITERATE(TSeqMatchMap, it, match_map) {
        if ( it->second ) {
            continue;
        }
        ds_match_map.insert(CDataSource::TSeqMatchMap::value_type(
            it->first, SSeqMatch_DS()));
    }
    if ( match_map.empty() ) {
        return;
    }
    GetDataSource().GetBlobs(ds_match_map);
    ITERATE(CDataSource::TSeqMatchMap, ds_match, ds_match_map) {
        if ( !ds_match->second ) {
            continue;
        }
        SSeqMatch_Scope& scope_match = match_map[ds_match->first];
        scope_match = x_GetSeqMatch(ds_match->first);
        x_SetMatch(scope_match, ds_match->second);
        if ( !scope_match ) {
            match_map.erase(ds_match->first);
        }
    }
}


bool CDataSource_ScopeInfo::TSEIsInQueue(const CTSE_ScopeInfo& tse) const
{
    TTSE_LockSetMutex::TReadLockGuard guard(m_TSE_UnlockQueueMutex);
    return m_TSE_UnlockQueue.Contains(&tse);
}


bool CDataSource_ScopeInfo::TSEIsReplaced(const TBlobId& blob_id) const
{
    if ( m_EditDS ) {
        return m_EditDS->TSEIsReplaced(blob_id);
    }
    return m_ReplacedTSEs.find(blob_id) != m_ReplacedTSEs.end();
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


CTSE_ScopeInfo::SUnloadedInfo::SUnloadedInfo(const CTSE_Lock& tse_lock)
    : m_Source(&tse_lock->GetDataSource()),
      m_BlobId(tse_lock->GetBlobId()),
      m_BlobOrder(tse_lock->GetBlobOrder())
{
    _ASSERT(m_Source);
    _ASSERT(m_BlobId);
    // copy all bioseq ids
    tse_lock->GetBioseqsIds(m_BioseqsIds);
}


CTSE_Lock CTSE_ScopeInfo::SUnloadedInfo::LockTSE(void)
{
    _ASSERT(m_Source);
    _ASSERT(m_BlobId);
    CTSE_Lock lock = m_Source->GetDataLoader()->GetBlobById(m_BlobId);
    if ( !lock ) {
        NCBI_THROW_FMT(CLoaderException, eConnectionFailed,
                       "Data loader GetBlobById("<<m_BlobId.ToString()<<") returned null");
    }
    return lock;
}


CTSE_ScopeInfo::CTSE_ScopeInfo(CDataSource_ScopeInfo& ds_info,
                               const CTSE_Lock& lock,
                               int load_index,
                               bool can_be_unloaded)
    : m_DS_Info(&ds_info),
      m_LoadIndex(load_index),
      m_TSE_LockCounter(0),
      m_UserLockCounter(0),
      m_TSE_LockAssignState(0),
      m_UsedByTSE(0)
{
    _ASSERT(lock);
    if ( can_be_unloaded ) {
        _ASSERT(lock->GetBlobId());
        m_UnloadedInfo.reset(new SUnloadedInfo(lock));
    }
    else {
        // permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm lock");
        ++m_TSE_LockCounter;
        SetTSE_Lock(lock);
        _ASSERT(x_VerifyTSE_LockIsAssigned(lock));
    }
}


CTSE_ScopeInfo::~CTSE_ScopeInfo(void)
{
    if ( !CanBeUnloaded() ) {
        // remove permanent lock
        _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") perm unlock: "<<m_TSE_LockCounter);
        _VERIFY(--m_TSE_LockCounter == 0);
    }
    x_DetachDS();
    _TRACE_TSE_LOCK("CTSE_ScopeInfo("<<this<<") final: "<<m_TSE_LockCounter);
    _ASSERT(m_TSE_LockCounter == 0);
    _ASSERT(x_VerifyTSE_LockIsNotAssigned());
    _ASSERT(!m_UsedByTSE);
    _ASSERT(m_UsedTSE_Set.empty());
}


CTSE_ScopeInfo::TBlobOrder CTSE_ScopeInfo::GetBlobOrder(void) const
{
    if ( CanBeUnloaded() ) {
        _ASSERT(m_UnloadedInfo.get());
        return m_UnloadedInfo->m_BlobOrder;
    }
    else {
        _ASSERT(m_TSE_Lock);
        return m_TSE_Lock->GetBlobOrder();
    }
}


CTSE_ScopeInfo::TBlobId CTSE_ScopeInfo::GetBlobId(void) const
{
    if ( CanBeUnloaded() ) {
        _ASSERT(m_UnloadedInfo.get());
        return m_UnloadedInfo->m_BlobId;
    }
    else {
        _ASSERT(m_TSE_Lock);
        return m_TSE_Lock->GetBlobId();
    }
}


const CTSE_ScopeInfo::TSeqIds& CTSE_ScopeInfo::GetBioseqsIds(void) const
{
    _ASSERT(CanBeUnloaded());
    return m_UnloadedInfo->m_BioseqsIds;
}


/////////////////////////////////////////////////////////////////////////////
// TSE locking support classes

inline
void CTSE_ScopeInfo::x_InternalLockTSE(void)
{
    _VERIFY(++m_TSE_LockCounter > 0);
}


inline
void CTSE_ScopeInfo::x_InternalRelockTSE(void)
{
    _VERIFY(++m_TSE_LockCounter > 1);
}


inline
void CTSE_ScopeInfo::x_InternalUnlockTSE(void)
{
    if ( --m_TSE_LockCounter == 0 ) {
        _ASSERT(CanBeUnloaded());
        if ( IsAttached() ) {
            GetDSInfo().ForgetTSELock(*this);
        }
    }
}


inline
void CTSE_ScopeInfo::x_UserLockTSE(void)
{
    if ( ++m_UserLockCounter || !GetTSE_Lock() ) {
        //_ASSERT(CanBeUnloaded());
        if ( IsAttached() ) {
            // TODO: possible race - if the TSE becomes detached after the above check
            // scenario - in several threads:
            //   1. get new TSE handle
            //   2. call RemoveFromHistory()
            try {
                GetDSInfo().AcquireTSEUserLock(*this);
            }
            catch ( ... ) {
                x_UserUnlockTSE();
                throw;
            }
        }
    }
}


inline
void CTSE_ScopeInfo::x_UserRelockTSE(void)
{
    _VERIFY(++m_UserLockCounter > 1);
}


inline
void CTSE_ScopeInfo::x_UserUnlockTSE(void)
{
    if ( --m_UserLockCounter == 0 ) {
        //_ASSERT(CanBeUnloaded());
        if ( IsAttached() ) {
            GetDSInfo().ReleaseTSEUserLock(*this);
        }
    }
}


void CTSE_ScopeInternalLocker::Lock(CTSE_ScopeInfo* tse) const
{
    CObjectCounterLocker::Lock(tse);
    tse->x_InternalLockTSE();
}


void CTSE_ScopeInternalLocker::Relock(CTSE_ScopeInfo* tse) const
{
    CObjectCounterLocker::Relock(tse);
    tse->x_InternalRelockTSE();
}


void CTSE_ScopeInternalLocker::Unlock(CTSE_ScopeInfo* tse) const
{
    tse->x_InternalUnlockTSE();
    CObjectCounterLocker::Unlock(tse);
}


void CTSE_ScopeUserLocker::Lock(CTSE_ScopeInfo* tse) const
{
    CObjectCounterLocker::Lock(tse);
    tse->x_InternalLockTSE();
    try {
        tse->x_UserLockTSE();
    }
    catch ( ... ) {
        tse->x_InternalUnlockTSE();
        CObjectCounterLocker::Unlock(tse);
        throw;
    }
}


void CTSE_ScopeUserLocker::Relock(CTSE_ScopeInfo* tse) const
{
    CObjectCounterLocker::Relock(tse);
    tse->x_InternalRelockTSE();
    tse->x_UserRelockTSE();
}


void CTSE_ScopeUserLocker::Unlock(CTSE_ScopeInfo* tse) const
{
    tse->x_UserUnlockTSE();
    tse->x_InternalUnlockTSE();
    CObjectCounterLocker::Unlock(tse);
}

// end of TSE locking support classes
/////////////////////////////////////////////////////////////////////////////


inline
bool CTSE_ScopeInfo::x_SameTSE(const CTSE_Info& tse) const
{
    return m_TSE_LockCounter > 0 && x_VerifyTSE_LockIsAssigned(tse);
}


void CTSE_ScopeInfo::ReleaseUsedTSEs(void)
{
    // release all used TSEs
    TUsedTSE_LockSet used;
    CTSE_ScopeInternalLock self_lock;
    CFastMutexGuard guard(sx_UsedTSEMutex);
    NON_CONST_ITERATE ( TUsedTSE_LockSet, it, m_UsedTSE_Set ) {
        _ASSERT(it->second->m_UsedByTSE == this);
        it->second->m_UsedByTSE = 0;
    }
    m_UsedTSE_Set.swap(used);
    if ( m_UsedByTSE ) {
        self_lock.Reset(this); // prevent recursive deletion
        m_UsedByTSE->m_UsedTSE_Set.erase(ConstRef(this));
        m_UsedByTSE = 0;
    }
}


bool CTSE_ScopeInfo::AddUsedTSE(const CTSE_ScopeUserLock& used_tse) const
{
    CTSE_ScopeInternalLock add_lock(used_tse.GetNCPointer());
    CTSE_ScopeInfo& add_info = const_cast<CTSE_ScopeInfo&>(*used_tse);
    if ( &add_info == this || // the same TSE
         !add_info.CanBeUnloaded() || // added is permanentrly locked
         m_TSE_LockCounter == 0 ) { // this one is unlocked
        return false;
    }
    CFastMutexGuard guard(sx_UsedTSEMutex);
    if ( add_info.m_UsedByTSE ) { // already used
        return false;
    }
    for ( const CTSE_ScopeInfo* p = m_UsedByTSE; p; p = p->m_UsedByTSE ) {
        if ( p == &add_info ) {
            return false;
        }
    }
    CTSE_ScopeInternalLock& add_slot = m_UsedTSE_Set[ConstRef(&*used_tse)];
    _ASSERT(!add_slot);
    add_info.m_UsedByTSE = this;
    swap(add_slot, add_lock);
    return true;
}


void CTSE_ScopeInfo::SetTSE_Lock(const CTSE_Lock& lock)
{
    _ASSERT(lock);
    if ( !x_TSE_LockIsAssigned() ) {
        CMutexGuard guard(m_TSE_LockMutex);
        if ( !x_TSE_LockIsAssigned() ) {
            _ASSERT(m_TSE_LockAssignState == 0);
            _ASSERT(!m_TSE_Lock);
            //LOG_POST("x_SetTSE_Lock: "<<CStackTrace());
            m_TSE_LockAssignState = 1;
            m_TSE_Lock = lock;
            if ( IsAttached() ) {
                GetDSInfo().AddTSE_Lock(lock);
            }
            m_TSE_LockAssignState = 2;
        }
        _ASSERT(m_TSE_LockAssignState == 2);
        _ASSERT(m_TSE_Lock == lock);
    }
    _ASSERT(x_VerifyTSE_LockIsAssigned(lock));
}


void CTSE_ScopeInfo::ResetTSE_Lock(void)
{
    if ( !x_TSE_LockIsNotAssigned() ) {
        CTSE_Lock lock; // delete the OM TSE lock outside of mutex
        CMutexGuard guard(m_TSE_LockMutex);
        if ( !x_TSE_LockIsNotAssigned() ) {
            _ASSERT(m_TSE_LockAssignState == 2);
            m_TSE_LockAssignState = 1;
            lock.Swap(m_TSE_Lock);
            if ( IsAttached() ) {
                GetDSInfo().RemoveTSE_Lock(lock);
            }
            m_TSE_LockAssignState = 0;
        }
        _ASSERT(m_TSE_LockAssignState == 0);
        _ASSERT(!m_TSE_Lock);
    }
    _ASSERT(x_TSE_LockIsNotAssigned());
}


void CTSE_ScopeInfo::DropTSE_Lock(void)
{
    if ( !x_TSE_LockIsNotAssigned() ) {
        CMutexGuard guard(m_TSE_LockMutex);
        if ( !x_TSE_LockIsNotAssigned() ) {
            _ASSERT(m_TSE_LockAssignState == 2);
            m_TSE_LockAssignState = 1;
            m_TSE_Lock.Reset();
            m_TSE_LockAssignState = 0;
        }
        _ASSERT(m_TSE_LockAssignState == 0);
        _ASSERT(!m_TSE_Lock);
    }
    _ASSERT(x_TSE_LockIsNotAssigned());
}


void CTSE_ScopeInfo::SetEditTSE(const CTSE_Lock& new_tse_lock,
                                CDataSource_ScopeInfo& new_ds)
{
    _ASSERT(!CanBeEdited());
    _ASSERT(new_ds.CanBeEdited());
    _ASSERT(&new_tse_lock->GetDataSource() == &new_ds.GetDataSource());

    CUnlockedTSEsGuard unlocked_guard;
    CTSE_Lock old_tse_lock;
    {{
        CMutexGuard guard(m_TSE_LockMutex);
        _ASSERT(x_VerifyTSE_LockIsAssigned());
        _ASSERT(&m_TSE_Lock->GetDataSource() == &GetDSInfo().GetDataSource());
        old_tse_lock = m_TSE_Lock;
    }}
    
    TScopeInfoMap old_map; // save old scope info map
    TBioseqById old_bioseq_map; // save old bioseq info map
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        old_map.swap(m_ScopeInfoMap);
        old_bioseq_map.swap(m_BioseqById);
    }}

    GetDSInfo().RemoveFromHistory(*this); // remove tse from old ds
    _ASSERT(x_VerifyTSE_LockIsNotAssigned());
    _ASSERT(!m_DS_Info);
    if ( CanBeUnloaded() ) {
        m_UnloadedInfo.reset(); // edit tse cannot be unloaded
        ++m_TSE_LockCounter;
    }

    // convert scope info map
    const TEditInfoMap& edit_map = new_tse_lock->m_BaseTSE->m_ObjectCopyMap;
    NON_CONST_ITERATE ( TScopeInfoMap, it, old_map ) {
        CConstRef<CTSE_Info_Object> old_obj(it->first);
        _ASSERT(old_obj);
        TEditInfoMap::const_iterator iter = edit_map.find(old_obj);
        TScopeInfoMapKey new_obj;
        if ( iter == edit_map.end() ) {
            _ASSERT(&*old_obj == &*old_tse_lock);
            new_obj.Reset(&*new_tse_lock);
        }
        else {
            new_obj.Reset(&dynamic_cast<const CTSE_Info_Object&>(*iter->second));
        }
        _ASSERT(new_obj);
        _ASSERT(&*new_obj != &*old_obj);
        TScopeInfoMapValue info = it->second;
        _ASSERT(info->HasObject(*old_obj));
        info->m_ObjectInfo = new_obj;
        _ASSERT(info->HasObject(*new_obj));
        _VERIFY(m_ScopeInfoMap.insert
                (TScopeInfoMap::value_type(new_obj, info)).second);
    }
    // restore bioseq info map
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        m_BioseqById.swap(old_bioseq_map);
    }}

    new_ds.AttachTSE(*this, new_tse_lock);

    _ASSERT(&GetDSInfo() == &new_ds);
    _ASSERT(x_VerifyTSE_LockIsAssigned(new_tse_lock));
    
    const_cast<CTSE_Info&>(*new_tse_lock).m_BaseTSE->m_ObjectCopyMap.clear();
}


void CTSE_ScopeInfo::RestoreReplacedTSE(void)
{
    if ( m_ReplacedTSE ) {
        _ASSERT(m_DS_Info);
        m_DS_Info->m_ReplacedTSEs.erase(m_ReplacedTSE);
        m_ReplacedTSE = TBlobId();
    }
}


void CTSE_ScopeInfo::ReplaceTSE(const CTSE_Info& old_tse)
{
    RestoreReplacedTSE();
    _ASSERT(m_DS_Info);
    m_ReplacedTSE = old_tse.GetBlobId();
    if ( !m_DS_Info->m_ReplacedTSEs.insert(m_ReplacedTSE).second ) {
        m_ReplacedTSE = TBlobId();
        ERR_POST("CTSE_ScopeInfo::ReplaceTSE("<<
                 old_tse.GetDescription()<<"): already replaced");
    }
}


// Action A4.
void CTSE_ScopeInfo::ForgetTSE_Lock(void)
{
    if ( m_TSE_LockCounter > 0 ) {
        // relocked already
        return;
    }
    ReleaseUsedTSEs();
    if ( !x_TSE_LockIsNotAssigned() ) {
        CTSE_Lock lock; // delete the OM TSE lock outside of mutex
        CMutexGuard guard(m_TSE_LockMutex);
        if ( !x_TSE_LockIsNotAssigned() ) {
            _ASSERT(m_TSE_LockAssignState == 2);
            if ( m_TSE_LockCounter > 0 ) {
                // relocked already
                return;
            }
            m_TSE_LockAssignState = 1;
            {{
                CMutexGuard guard2(m_ScopeInfoMapMutex);
                NON_CONST_ITERATE ( TScopeInfoMap, it, m_ScopeInfoMap ) {
                    _ASSERT(!it->second->m_TSE_HandleAssigned);
                    it->second->m_ObjectInfoAssigned = false;
                    it->second->m_ObjectInfo.Reset();
                    _ASSERT(!it->second->HasObject());
                    if ( it->second->IsTemporary() ) {
                        it->second->x_DetachTSE(this);
                    }
                }
                m_ScopeInfoMap.clear();
            }}
            lock.Swap(m_TSE_Lock);
            if ( IsAttached() ) {
                GetDSInfo().RemoveTSE_Lock(lock);
            }
            m_TSE_LockAssignState = 0;
        }
        _ASSERT(x_VerifyTSE_LockIsNotAssigned());
    }
}


void CTSE_ScopeInfo::x_DetachDS(void)
{
    if ( !m_DS_Info ) {
        return;
    }
    ReleaseUsedTSEs();
    CMutexGuard guard(m_TSE_LockMutex);
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        NON_CONST_ITERATE ( TScopeInfoMap, it, m_ScopeInfoMap ) {
            it->second->m_TSE_HandleAssigned = false;
            it->second->m_TSE_Handle.Reset();
            it->second->m_ObjectInfoAssigned = false;
            it->second->m_ObjectInfo.Reset();
            _ASSERT(!it->second->HasObject());
            it->second->x_DetachTSE(this);
        }
        m_ScopeInfoMap.clear();
    }}
    m_TSE_LockAssignState = 0;
    m_TSE_Lock.Reset();
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        while ( !m_BioseqById.empty() ) {
            CRef<CBioseq_ScopeInfo> bioseq = m_BioseqById.begin()->second;
            bioseq->x_DetachTSE(this);
            _ASSERT(m_BioseqById.empty()||m_BioseqById.begin()->second != bioseq);
        }
    }}
    m_DS_Info = 0;
}


int CTSE_ScopeInfo::x_GetDSLocksCount(void) const
{
    int max_locks = CanBeUnloaded() ? 0 : 1;
    if ( GetDSInfo().TSEIsInQueue(*this) ) {
        // Extra-lock from delete queue allowed
        ++max_locks;
    }
    return max_locks;
}


pair<bool, CScopeInfo_Base*> CTSE_ScopeInfo::GetUserLockState(const CTSE_Handle* tseh) const
{
    pair<bool, CScopeInfo_Base*> ret;
    if ( !tseh ) {
        // no request handle, use simple handle lock count
        ret.first = IsUserLocked();
        return ret;
    }
    // now we have one handle already
    _ASSERT(&tseh->x_GetScopeInfo() == this);
    _ASSERT(m_UserLockCounter >= 1);
    if ( m_UserLockCounter > 1 ) {
        // there are more sub-object handles
        ret.first = true;
        return ret;
    }
    // now we may have several sub-object handles pointing to the same object
    // scan m_ScopeInfoMap for a possible handle having tseh inside
    CMutexGuard guard(m_ScopeInfoMapMutex);
    for ( auto& s : m_ScopeInfoMap ) {
        if ( &s.second->m_TSE_Handle == tseh ) {
            _ASSERT(s.second->m_LockCounter >= 1);
            ret.second = s.second.GetNCPointer();
            ret.first = s.second->m_LockCounter > 1;
            return ret;
        }
    }
    return ret;
}


void CTSE_ScopeInfo::RemoveFromHistory(const CTSE_Handle* tseh,
                                       int action_if_locked,
                                       bool drop_from_ds)
{
    auto locked = GetUserLockState(tseh);
    if ( locked.first ) {
        switch ( action_if_locked ) {
        case CScope::eKeepIfLocked:
            return;
        case CScope::eThrowIfLocked:
            NCBI_THROW(CObjMgrException, eLockedData,
                       "Cannot remove TSE from scope's history "
                       "because it's locked");
        default: // forced removal
            break;
        }
    }
    CTSE_Handle tse;
    if ( locked.second ) {
        locked.second->m_TSE_HandleAssigned = false;
        tse.Swap(locked.second->m_TSE_Handle);
        _ASSERT(&tse.x_GetScopeInfo() == this);
    }
    CUnlockedTSEsGuard guard;
    GetDSInfo().RemoveFromHistory(*this, drop_from_ds);
}


void CTSE_ScopeInfo::RemoveFromHistory(const CTSE_Handle& tseh,
                                       int action_if_locked,
                                       bool drop_from_ds)
{
    tseh.x_GetScopeInfo().RemoveFromHistory(&tseh, action_if_locked, drop_from_ds);
}


bool CTSE_ScopeInfo::HasResolvedBioseq(const CSeq_id_Handle& id) const
{
    CMutexGuard guard2(m_ScopeInfoMapMutex);
    return m_BioseqById.find(id) != m_BioseqById.end();
}


bool CTSE_ScopeInfo::ContainsBioseq(const CSeq_id_Handle& id) const
{
    if ( CanBeUnloaded() ) {
        return binary_search(m_UnloadedInfo->m_BioseqsIds.begin(),
                             m_UnloadedInfo->m_BioseqsIds.end(),
                             id);
    }
    else {
        return m_TSE_Lock->ContainsBioseq(id);
    }
}


CSeq_id_Handle
CTSE_ScopeInfo::ContainsMatchingBioseq(const CSeq_id_Handle& id) const
{
    if ( CanBeUnloaded() ) {
        if ( ContainsBioseq(id) ) {
            return id;
        }
        if ( id.HaveMatchingHandles() ) {
            CSeq_id_Handle::TMatches ids;
            id.GetMatchingHandles(ids, eAllowWeakMatch);
            ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
                if ( *it != id ) {
                    if ( ContainsBioseq(*it) ) {
                        return *it;
                    }
                }
            }
        }
        return null;
    }
    else {
        return m_TSE_Lock->ContainsMatchingBioseq(id);
    }
}


// Action A5.
template<class TScopeInfo>
CScopeInfo_Ref<TScopeInfo>
CTSE_ScopeInfo::x_GetScopeLock(const CTSE_Handle& tse,
                               const typename TScopeInfo::TObjectInfo& info)
{
    CRef<TScopeInfo> scope_info;
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        _ASSERT(x_SameTSE(tse.x_GetTSE_Info()));
        TScopeInfoMapKey key(&info);
        TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
        if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
            scope_info = new TScopeInfo(tse, info);
            _ASSERT(scope_info->HasObject(info));
            TScopeInfoMapValue value(scope_info);
            m_ScopeInfoMap.insert(iter, TScopeInfoMap::value_type(key, value));
            _ASSERT(value->HasObject(info));
        }
        else {
            _ASSERT(iter->second->HasObject(info));
            scope_info = &dynamic_cast<TScopeInfo&>(*iter->second);
        }
    }}
    CScopeInfo_Ref<TScopeInfo> ret(*scope_info);
    ret->x_SetTSE_Handle(tse);
    return ret;
}


// Action A5.
CTSE_ScopeInfo::TSeq_entry_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CSeq_entry_Info& info)
{
    return x_GetScopeLock<CSeq_entry_ScopeInfo>(tse, info);
}


// Action A5.
CTSE_ScopeInfo::TSeq_annot_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CSeq_annot_Info& info)
{
    return x_GetScopeLock<CSeq_annot_ScopeInfo>(tse, info);
}

// Action A5.
CTSE_ScopeInfo::TBioseq_set_Lock
CTSE_ScopeInfo::GetScopeLock(const CTSE_Handle& tse,
                             const CBioseq_set_Info& info)
{
    return x_GetScopeLock<CBioseq_set_ScopeInfo>(tse, info);
}

// Action A5.
CTSE_ScopeInfo::TBioseq_Lock
CTSE_ScopeInfo::GetBioseqLock(CRef<CBioseq_ScopeInfo> info,
                              CConstRef<CBioseq_Info> bioseq)
{
    // TODO: possible deadlock (1), m_TSE_LockMutex is taken before m_TSE_UnlockQueueMutex
    // this thread calls GetBioseqHandle()
    //CMutexGuard guard(m_TSE_LockMutex);
    CTSE_ScopeUserLock tse(this);
    _ASSERT(x_TSE_LockIsAssigned());
    if ( !info ) {
        // find CBioseq_ScopeInfo
        _ASSERT(bioseq);
        _ASSERT(bioseq->BelongsToTSE_Info(*m_TSE_Lock));
        const CBioseq_Info::TId& ids = bioseq->GetId();
        if ( !ids.empty() ) {
            // named bioseq, look in Seq-id index only
            info = x_FindBioseqInfo(ids);
            if ( !info ) {
                CMutexGuard guard2(m_ScopeInfoMapMutex);
                info = x_CreateBioseqInfo(ids);
            }
        }
        else {
            // unnamed bioseq, look in object map, create if necessary
            {{
                CMutexGuard guard2(m_ScopeInfoMapMutex);
                TScopeInfoMapKey key(bioseq);
                TScopeInfoMap::iterator iter = m_ScopeInfoMap.lower_bound(key);
                if ( iter == m_ScopeInfoMap.end() || iter->first != key ) {
                    info = new CBioseq_ScopeInfo(*this);
                    TScopeInfoMapValue value(info);
                    m_ScopeInfoMap.insert(iter, TScopeInfoMap::value_type(key, value));
                    _ASSERT(!value->HasObject());
                    value->m_ObjectInfo = &*bioseq;
                    value->m_ObjectInfoAssigned = true;
                    _ASSERT(value->HasObject(*bioseq));
                }
                else {
                    _ASSERT(iter->second->HasObject(*bioseq));
                    info.Reset(&dynamic_cast<CBioseq_ScopeInfo&>(*iter->second));
                }
            }}
            TBioseq_Lock ret(*info);
            ret->x_SetTSE_Lock(tse, *bioseq);
            return ret;
        }
    }
    _ASSERT(info);
    _ASSERT(!info->IsDetached());
    // update CBioseq_ScopeInfo object
    if ( !info->HasObject() ) {
        if ( !bioseq ) {
            const CBioseq_ScopeInfo::TIds& ids = info->GetIds();
            if ( !ids.empty() ) {
                const CSeq_id_Handle& id = *ids.begin();
                bioseq = m_TSE_Lock->FindBioseq(id);
                _ASSERT(bioseq);
            }
            else {
                // unnamed bioseq without object - error,
                // this situation must be prevented by code.
                _ASSERT(0 && "CBioseq_ScopeInfo without ids and bioseq");
            }
        }
        _ASSERT(bioseq);
        _ASSERT(bioseq->GetId() == info->GetIds());
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        TScopeInfoMapKey key(bioseq);
        TScopeInfoMapValue value(info);
        m_ScopeInfoMap.insert(TScopeInfoMap::value_type(key, value));
    }
    TBioseq_Lock ret(*info);
    if ( bioseq ) {
        ret->x_SetTSE_Lock(tse, *bioseq);
    }
    else {
        ret->x_SetTSE_Handle(tse);
    }
    return ret;
}


// Find scope bioseq info by match: CConstRef<CBioseq_Info> & CSeq_id_Handle
// The problem is that CTSE_Info and CBioseq_Info may be unloaded and we
// cannot store pointers to them.
// However, we have to find the same CBioseq_ScopeInfo object.
// It is stored in m_BioseqById map under one of Bioseq's ids.
CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::GetBioseqInfo(const SSeqMatch_Scope& match)
{
    _ASSERT(&*match.m_TSE_Lock == this);
    _ASSERT(match.m_Seq_id);
    _ASSERT(match.m_Bioseq);
    CRef<CBioseq_ScopeInfo> info;
    const CBioseq_Info::TId& ids = match.m_Bioseq->GetId();
    _ASSERT(find(ids.begin(), ids.end(), match.m_Seq_id) != ids.end());

    //CMutexGuard guard(m_TSE_LockMutex);
    
    info = x_FindBioseqInfo(ids);
    if ( !info ) {
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        info = x_CreateBioseqInfo(ids);
    }
    return info;
}


CRef<CBioseq_ScopeInfo>
CTSE_ScopeInfo::x_FindBioseqInfo(const TSeqIds& ids) const
{
    if ( !ids.empty() ) {
        const CSeq_id_Handle& id = *ids.begin();
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        for ( TBioseqById::const_iterator it(m_BioseqById.lower_bound(id));
              it != m_BioseqById.end() && it->first == id; ++it ) {
            if ( it->second->GetIds() == ids ) {
                return it->second;
            }
        }
    }
    return null;
}


CRef<CBioseq_ScopeInfo> CTSE_ScopeInfo::x_CreateBioseqInfo(const TSeqIds& ids)
{
    return Ref(new CBioseq_ScopeInfo(*this, ids));
}


void CTSE_ScopeInfo::x_IndexBioseq(const CSeq_id_Handle& id,
                                   CBioseq_ScopeInfo* info)
{
    CMutexGuard guard2(m_ScopeInfoMapMutex);
    m_BioseqById.insert(TBioseqById::value_type(id, Ref(info)));
}


void CTSE_ScopeInfo::x_UnindexBioseq(const CSeq_id_Handle& id,
                                     const CBioseq_ScopeInfo* info)
{
    CMutexGuard guard2(m_ScopeInfoMapMutex);
    for ( TBioseqById::iterator it = m_BioseqById.lower_bound(id);
          it != m_BioseqById.end() && it->first == id; ++it ) {
        if ( it->second == info ) {
            m_BioseqById.erase(it);
            return;
        }
    }
    _ASSERT(0 && "UnindexBioseq: CBioseq_ScopeInfo is not in index");
}

// Action A2.
void CTSE_ScopeInfo::ResetEntry(CSeq_entry_ScopeInfo& info)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    CScopeInfo_Ref<CScopeInfo_Base> child;
    if ( info.GetObjectInfo().Which() == CSeq_entry::e_Set ) {
        child.Reset(&*GetScopeLock(info.m_TSE_Handle,
                                   info.GetObjectInfo().GetSet()));
    }
    else if ( info.GetObjectInfo().Which() == CSeq_entry::e_Seq ) {
        CConstRef<CBioseq_Info> bioseq(&info.GetObjectInfo().GetSeq());
        child.Reset(&GetBioseqLock(null, bioseq).GetNCObject());
    }
    else {
        // nothing to do
        return;
    }
    info.GetNCObjectInfo().Reset();
    x_SaveRemoved(*child);
    _ASSERT(child->IsDetached());
}

// Action A2.
void CTSE_ScopeInfo::RemoveEntry(CSeq_entry_ScopeInfo& info)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    CSeq_entry_Info& entry = info.GetNCObjectInfo();
    entry.GetParentBioseq_set_Info().RemoveEntry(Ref(&entry));
    x_SaveRemoved(info);
    _ASSERT(info.IsDetached());
}

// Action A2.
void CTSE_ScopeInfo::RemoveAnnot(CSeq_annot_ScopeInfo& info)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(info.IsAttached());
    _ASSERT(info.GetObjectInfo().BelongsToTSE_Info(*m_TSE_Lock));
    CSeq_annot_Info& annot = info.GetNCObjectInfo();
    annot.GetParentBioseq_Base_Info().RemoveAnnot(Ref(&annot));
    x_SaveRemoved(info);
    _ASSERT(info.IsDetached());
    _ASSERT(!info.GetObjectInfo().HasTSE_Info());
}


// Action A7.
#ifdef _DEBUG
void CTSE_ScopeInfo::x_CheckAdded(CScopeInfo_Base& parent,
                                  CScopeInfo_Base& child)
{
    _ASSERT(parent.IsAttached());
    _ASSERT(parent.HasObject());
    _ASSERT(parent.m_LockCounter > 0);
    _ASSERT(child.IsDetached());
    _ASSERT(child.m_DetachedInfo);
    _ASSERT(child.HasObject());
    _ASSERT(!child.GetObjectInfo_Base().HasParent_Info());
    _ASSERT(child.m_LockCounter > 0);
    _ASSERT(x_SameTSE(parent.GetTSE_Handle().x_GetTSE_Info()));
}
#else  /* _DEBUG */
void CTSE_ScopeInfo::x_CheckAdded(CScopeInfo_Base& /*parent*/,
                                  CScopeInfo_Base& /*child*/)
{}
#endif


// Action A7.
void CTSE_ScopeInfo::AddEntry(CBioseq_set_ScopeInfo& parent,
                              CSeq_entry_ScopeInfo& child,
                              int index)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    parent.GetNCObjectInfo().AddEntry(Ref(&child.GetNCObjectInfo()), index, true);
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Action A7.
void CTSE_ScopeInfo::AddAnnot(CSeq_entry_ScopeInfo& parent,
                              CSeq_annot_ScopeInfo& child)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    _ASSERT(!child.GetObjectInfo().HasTSE_Info());
    x_CheckAdded(parent, child);
    parent.GetNCObjectInfo().AddAnnot(Ref(&child.GetNCObjectInfo()));
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
    _ASSERT(child.GetObjectInfo().BelongsToTSE_Info(*m_TSE_Lock));
}


// Action A7.
void CTSE_ScopeInfo::SelectSet(CSeq_entry_ScopeInfo& parent,
                               CBioseq_set_ScopeInfo& child)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    _ASSERT(parent.GetObjectInfo().Which() == CSeq_entry::e_not_set);
    parent.GetNCObjectInfo().SelectSet(child.GetNCObjectInfo());
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Action A7.
void CTSE_ScopeInfo::SelectSeq(CSeq_entry_ScopeInfo& parent,
                               CBioseq_ScopeInfo& child)
{
    //CMutexGuard guard(m_TSE_LockMutex);
    x_CheckAdded(parent, child);
    _ASSERT(parent.GetObjectInfo().Which() == CSeq_entry::e_not_set);
    parent.GetNCObjectInfo().SelectSeq(child.GetNCObjectInfo());
    x_RestoreAdded(parent, child);
    _ASSERT(child.IsAttached());
}


// Save and restore scope info objects.

typedef pair<CConstRef<CTSE_Info_Object>,
             CRef<CScopeInfo_Base> > TDetachedInfoElement;
typedef vector<TDetachedInfoElement> TDetachedInfo;

// Action A3.
void CTSE_ScopeInfo::x_SaveRemoved(CScopeInfo_Base& info)
{
    _ASSERT(info.IsAttached()); // info is not yet detached
    _ASSERT(!info.m_DetachedInfo); // and doesn't contain m_DetachedInfo yet
    _ASSERT(info.HasObject()); // it contains pointer to removed object
    _ASSERT(!info.GetObjectInfo_Base().HasParent_Info()); //and is root of tree
    CRef<CObjectFor<TDetachedInfo> > save(new CObjectFor<TDetachedInfo>);
    _ASSERT(!m_UnloadedInfo); // this TSE cannot be unloaded
    _ASSERT(m_TSE_Lock); // and TSE is locked
    _TRACE("x_SaveRemoved("<<&info<<") TSE: "<<this);
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        for ( TScopeInfoMap::iterator it = m_ScopeInfoMap.begin(); it != m_ScopeInfoMap.end(); ) {
            if ( !it->first->BelongsToTSE_Info(*m_TSE_Lock) ) {
                _TRACE(" "<<it->second<<" " << it->first);
                it->second->m_TSE_HandleAssigned = false;
                it->second->m_TSE_Handle.Reset();
                it->second->x_DetachTSE(this);
                if ( &*it->second != &info ) {
                    _ASSERT(it->first->HasParent_Info());
                    save->GetData().push_back(TDetachedInfoElement(it->first,
                                                                   it->second));
                }
                m_ScopeInfoMap.erase(it++);
            }
            else {
                ++it;
            }
        }
    }}
    _ASSERT(info.IsDetached()); // info is already detached
    _ASSERT(m_TSE_Lock);
    info.m_DetachedInfo.Reset(save); // save m_DetachedInfo
#ifdef _DEBUG
    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        ITERATE ( TBioseqById, it, m_BioseqById ) {
            _ASSERT(!it->second->IsDetached());
            _ASSERT(&it->second->x_GetTSE_ScopeInfo() == this);
            _ASSERT(!it->second->HasObject() || it->second->GetObjectInfo_Base().BelongsToTSE_Info(*m_TSE_Lock));
        }
    }}
#endif
    // post checks
    _ASSERT(info.IsDetached());
    _ASSERT(info.m_DetachedInfo);
    _ASSERT(info.HasObject()); // it contains pointer to removed object
    _ASSERT(!info.GetObjectInfo_Base().HasParent_Info());//and is root of tree
}

// Action A7
void CTSE_ScopeInfo::x_RestoreAdded(CScopeInfo_Base& parent,
                                    CScopeInfo_Base& child)
{
    _ASSERT(parent.IsAttached()); // parent is attached
    _ASSERT(parent.m_TSE_Handle); // and locked
    _ASSERT(parent.m_LockCounter > 0);
    _ASSERT(child.IsDetached()); // child is detached
    _ASSERT(child.m_DetachedInfo); // and contain m_DetachedInfo
    _ASSERT(child.HasObject()); // it contains pointer to removed object
    _ASSERT(child.GetObjectInfo_Base().HasParent_Info());//and is connected
    _ASSERT(child.m_LockCounter > 0);

    _TRACE("x_RestoreAdded("<<&child<<") TSE: "<<this);

    CRef<CObjectFor<TDetachedInfo> > infos
        (&dynamic_cast<CObjectFor<TDetachedInfo>&>(*child.m_DetachedInfo));
    child.m_DetachedInfo.Reset();
    infos->GetData().push_back
        (TDetachedInfoElement(ConstRef(&child.GetObjectInfo_Base()),
                              Ref(&child)));

    {{
        CMutexGuard guard2(m_ScopeInfoMapMutex);
        ITERATE ( TDetachedInfo, it, infos->GetData() ) {
            _TRACE(" "<<it->second<<" " << it->first);
            CScopeInfo_Base& info = it->second.GetNCObject();
            if ( info.m_LockCounter > 0 ) {
                info.x_AttachTSE(this);
                _VERIFY(m_ScopeInfoMap.insert
                        (TScopeInfoMap::value_type(it->first, it->second)).second);
                info.x_SetTSE_Handle(parent.m_TSE_Handle);
            }
        }
    }}
    _ASSERT(child.IsAttached());
    _ASSERT(child.m_TSE_Handle.m_TSE);
    _ASSERT(child.HasObject());
}


SSeqMatch_Scope CTSE_ScopeInfo::Resolve(const CSeq_id_Handle& id)
{
    return GetDSInfo().Resolve(id, *this);
}


map<size_t, SSeqMatch_Scope>
CTSE_ScopeInfo::ResolveBulk(const map<size_t, CSeq_id_Handle>& ids)
{
    return GetDSInfo().ResolveBulk(ids, *this);
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

// If this define will be uncomented then it must be changed to use ERR_POST_X
//#define BIOSEQ_TRACE(x) ERR_POST(x)
#ifndef BIOSEQ_TRACE
# define BIOSEQ_TRACE(x)
#endif

CBioseq_ScopeInfo::CBioseq_ScopeInfo(TBlobStateFlags flags, int timestamp)
    : m_BlobState(flags | CBioseq_Handle::fState_no_data),
      m_UnresolvedTimestamp(timestamp)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse)
    : m_BlobState(CBioseq_Handle::fState_none),
      m_UnresolvedTimestamp(0)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CTSE_ScopeInfo& tse,
                                     const TIds& ids)
    : m_Ids(ids),
      m_BlobState(CBioseq_Handle::fState_none),
      m_UnresolvedTimestamp(0)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this);
    x_AttachTSE(&tse);
}


CBioseq_ScopeInfo::~CBioseq_ScopeInfo(void)
{
    if ( IsAttached() ) {
        BIOSEQ_TRACE("~CBioseq_ScopeInfo: "<<this<<
                     " TSE "<<&x_GetTSE_ScopeInfo());
    }
    else {
        BIOSEQ_TRACE("~CBioseq_ScopeInfo: "<<this);
    }
    _ASSERT(!IsAttached());
}


void CBioseq_ScopeInfo::SetUnresolved(TBlobStateFlags flags, int timestamp)
{
    _ASSERT(!HasBioseq());
    m_BlobState = flags | CBioseq_Handle::fState_no_data;
    m_UnresolvedTimestamp = timestamp;
}


void CBioseq_ScopeInfo::SetResolved(CTSE_ScopeInfo& tse,
                                    const TIds& ids)
{
    _ASSERT(!HasBioseq());
    m_Ids = ids;
    m_BlobState = CBioseq_Handle::fState_none;
    m_UnresolvedTimestamp = 0;
    x_AttachTSE(&tse);
}


const CBioseq_ScopeInfo::TIndexIds* CBioseq_ScopeInfo::GetIndexIds(void) const
{
    const TIds& ids = GetIds();
    return ids.empty()? 0: &ids;
}


bool CBioseq_ScopeInfo::HasBioseq(void) const
{
    return (GetBlobState() & CBioseq_Handle::fState_no_data) == 0;
}


CBioseq_ScopeInfo::TBioseq_Lock
CBioseq_ScopeInfo::GetLock(CConstRef<CBioseq_Info> bioseq)
{
    return x_GetTSE_ScopeInfo().GetBioseqLock(Ref(this), bioseq);
}


void CBioseq_ScopeInfo::x_AttachTSE(CTSE_ScopeInfo* tse)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_AttachTSE "<<tse);
    m_BlobState = tse->GetTSE_Lock()->GetBlobState();
    CScopeInfo_Base::x_AttachTSE(tse);
    ITERATE ( TIds, it, GetIds() ) {
        tse->x_IndexBioseq(*it, this);
    }
}

void CBioseq_ScopeInfo::x_DetachTSE(CTSE_ScopeInfo* tse)
{
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_DetachTSE "<<tse);
    m_SynCache.Reset();
    x_ResetAnnotRef_Info();
    ITERATE ( TIds, it, GetIds() ) {
        tse->x_UnindexBioseq(*it, this);
    }
    CScopeInfo_Base::x_DetachTSE(tse);
    BIOSEQ_TRACE("CBioseq_ScopeInfo: "<<this<<" x_DetachTSE "<<tse<<" DONE");
}


string CBioseq_ScopeInfo::IdString(void) const
{
    CNcbiOstrstream os;
    const TIds& ids = GetIds();
    ITERATE ( TIds, it, ids ) {
        if ( it != ids.begin() )
            os << " | ";
        os << it->AsString();
    }
    return CNcbiOstrstreamToString(os);
}


void CBioseq_ScopeInfo::ResetId(void)
{
    _ASSERT(HasObject());
    const_cast<CBioseq_Info&>(GetObjectInfo()).ResetId();
    m_SynCache.Reset();
    x_GetScopeImpl().x_ClearCacheOnRemoveSeqId(CSeq_id_Handle(), *this);
    ITERATE ( TIds, it, GetIds() ) {
        x_GetTSE_ScopeInfo().x_UnindexBioseq(*it, this);
    }
    m_Ids.clear();
}


bool CBioseq_ScopeInfo::AddId(const CSeq_id_Handle& id)
{
    _ASSERT(HasObject());
    CBioseq_Info& info = const_cast<CBioseq_Info&>(GetObjectInfo());
    if ( !info.AddId(id) ) {
        return false;
    }
    m_Ids.push_back(id);
    m_SynCache.Reset();
    x_GetTSE_ScopeInfo().x_IndexBioseq(id, this);
    x_GetScopeImpl().x_ClearCacheOnNewData(info.GetTSE_Info(), id);
    return true;
}


bool CBioseq_ScopeInfo::RemoveId(const CSeq_id_Handle& id)
{
    _ASSERT(HasObject());
    if ( !const_cast<CBioseq_Info&>(GetObjectInfo()).RemoveId(id) ) {
        return false;
    }
    TIds::iterator it = find(m_Ids.begin(), m_Ids.end(), id);
    _ASSERT(it != m_Ids.end());
    x_GetScopeImpl().x_ClearCacheOnRemoveSeqId(id, *this);
    x_GetTSE_ScopeInfo().x_UnindexBioseq(id, this);
    m_Ids.erase(it);
    m_SynCache.Reset();
    return true;
}


/////////////////////////////////////////////////////////////////////////////
// SSeq_id_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

SSeq_id_ScopeInfo::SSeq_id_ScopeInfo(void)
{
}

SSeq_id_ScopeInfo::~SSeq_id_ScopeInfo(void)
{
}

/////////////////////////////////////////////////////////////////////////////
// CSynonymsSet
/////////////////////////////////////////////////////////////////////////////

CSynonymsSet::CSynonymsSet(void)
{
}


CSynonymsSet::~CSynonymsSet(void)
{
}


CSeq_id_Handle CSynonymsSet::GetSeq_id_Handle(const const_iterator& iter)
{
    return *iter;
}


bool CSynonymsSet::ContainsSynonym(const CSeq_id_Handle& id) const
{
   ITERATE ( TIdSet, iter, m_IdSet ) {
        if ( *iter == id ) {
            return true;
        }
    }
    return false;
}


void CSynonymsSet::AddSynonym(const CSeq_id_Handle& id)
{
    _ASSERT(!ContainsSynonym(id));
    m_IdSet.push_back(id);
}


END_SCOPE(objects)
END_NCBI_SCOPE
