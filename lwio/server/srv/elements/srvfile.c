/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.  You should have received a copy of the GNU General
 * Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */



/*
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        srvfile.c
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        Elements
 *
 *        File Object
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 */

#include "includes.h"

static
VOID
SrvFileFree(
    PLWIO_SRV_FILE pFile
    );

NTSTATUS
SrvFileCreate(
    USHORT                  fid,
    PWSTR                   pwszFilename,
    PIO_FILE_HANDLE         phFile,
    PIO_FILE_NAME*          ppFilename,
    ACCESS_MASK             desiredAccess,
    LONG64                  allocationSize,
    FILE_ATTRIBUTES         fileAttributes,
    FILE_SHARE_FLAGS        shareAccess,
    FILE_CREATE_DISPOSITION createDisposition,
    FILE_CREATE_OPTIONS     createOptions,
    PLWIO_SRV_FILE*         ppFile
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_FILE pFile = NULL;

    LWIO_LOG_DEBUG("Creating file [fid:%u]", fid);

    ntStatus = SrvAllocateMemory(
                    sizeof(LWIO_SRV_FILE),
                    (PVOID*)&pFile);
    BAIL_ON_NT_STATUS(ntStatus);

    pFile->refcount = 1;

    pthread_rwlock_init(&pFile->mutex, NULL);
    pFile->pMutex = &pFile->mutex;

    ntStatus = SrvAllocateStringW(pwszFilename, &pFile->pwszFilename);
    BAIL_ON_NT_STATUS(ntStatus);

    pFile->fid = fid;
    pFile->hFile = *phFile;
    *phFile = NULL;
    pFile->pFilename = *ppFilename;
    *ppFilename = NULL;
    pFile->desiredAccess = desiredAccess;
    pFile->allocationSize = allocationSize;
    pFile->fileAttributes = fileAttributes;
    pFile->shareAccess = shareAccess;
    pFile->createDisposition = createDisposition;
    pFile->createOptions = createOptions;
    pFile->ullLastFailedLockOffset = -1;

    pFile->resource.resourceType                 = SRV_RESOURCE_TYPE_FILE;
    pFile->resource.pAttributes                  = &pFile->resourceAttrs;
    pFile->resource.pAttributes->protocolVersion = SMB_PROTOCOL_VERSION_1;
    pFile->resource.pAttributes->fileId.pFid1    = &pFile->fid;

    LWIO_LOG_DEBUG("Associating file [object:0x%x][fid:%u]",
                    pFile,
                    fid);

    SRV_ELEMENTS_INCREMENT_OPEN_FILES;

    *ppFile = pFile;

cleanup:

    return ntStatus;

error:

    *ppFile = NULL;

    if (pFile)
    {
        SrvFileRelease(pFile);
    }

    goto cleanup;
}

NTSTATUS
SrvFileSetOplockState(
    PLWIO_SRV_FILE                   pFile,
    HANDLE                           hOplockState,
    PFN_LWIO_SRV_CANCEL_OPLOCK_STATE pfnCancelOplockState,
    PFN_LWIO_SRV_FREE_OPLOCK_STATE   pfnFreeOplockState
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bInLock  = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    if (pFile->hOplockState)
    {
        if (pFile->pfnCancelOplockState)
        {
            pFile->pfnCancelOplockState(pFile->hOplockState);
        }

        if (pFile->pfnFreeOplockState)
        {
            pFile->pfnFreeOplockState(pFile->hOplockState);
        }

        pFile->hOplockState         = NULL;
        pFile->pfnFreeOplockState   = NULL;
        pFile->pfnCancelOplockState = NULL;
    }

    pFile->hOplockState         = hOplockState;
    pFile->pfnFreeOplockState   = pfnFreeOplockState;
    pFile->pfnCancelOplockState = pfnCancelOplockState;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);

    return ntStatus;
}

HANDLE
SrvFileRemoveOplockState(
    PLWIO_SRV_FILE pFile
    )
{
    HANDLE  hOplockState = NULL;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    hOplockState = pFile->hOplockState;

    pFile->hOplockState         = NULL;
    pFile->pfnFreeOplockState   = NULL;
    pFile->pfnCancelOplockState = NULL;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);

    return hOplockState;
}

VOID
SrvFileResetOplockState(
    PLWIO_SRV_FILE pFile
    )
{
    SrvFileSetOplockState(pFile, NULL, NULL, NULL);
}

VOID
SrvFileSetOplockLevel(
    PLWIO_SRV_FILE pFile,
    UCHAR          ucOplockLevel
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    pFile->ucCurrentOplockLevel = ucOplockLevel;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);
}

UCHAR
SrvFileGetOplockLevel(
    PLWIO_SRV_FILE pFile
    )
{
    UCHAR ucOplockLevel = SMB_OPLOCK_LEVEL_NONE;

    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pFile->mutex);

    ucOplockLevel = pFile->ucCurrentOplockLevel;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);

    return ucOplockLevel;
}

VOID
SrvFileSetLastFailedLockOffset(
    PLWIO_SRV_FILE pFile,
    ULONG64        ullLastFailedLockOffset
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    pFile->ullLastFailedLockOffset = ullLastFailedLockOffset;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);
}

ULONG64
SrvFileGetLastFailedLockOffset(
    PLWIO_SRV_FILE pFile
    )
{
    ULONG64 ullLastFailedLockOffset = -1;

    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pFile->mutex);

    ullLastFailedLockOffset = pFile->ullLastFailedLockOffset;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);

    return ullLastFailedLockOffset;
}

VOID
SrvFileRegisterLock(
    PLWIO_SRV_FILE pFile
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    pFile->ulNumLocks++;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);
}

VOID
SrvFileRegisterUnlock(
    PLWIO_SRV_FILE pFile
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pFile->mutex);

    pFile->ulNumLocks--;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pFile->mutex);
}

PLWIO_SRV_FILE
SrvFileAcquire(
    PLWIO_SRV_FILE pFile
    )
{
    LWIO_LOG_DEBUG("Acquiring file [fid:%u]", pFile->fid);

    InterlockedIncrement(&pFile->refcount);

    return pFile;
}

VOID
SrvFileRelease(
    PLWIO_SRV_FILE pFile
    )
{
    LWIO_LOG_DEBUG("Releasing file [fid:%u]", pFile->fid);

    if (InterlockedDecrement(&pFile->refcount) == 0)
    {
        SRV_ELEMENTS_DECREMENT_OPEN_FILES;

        SrvFileFree(pFile);
    }
}

VOID
SrvFileRundown(
    PLWIO_SRV_FILE pFile
    )
{
    if (pFile->resource.ulResourceId)
    {
        PSRV_RESOURCE pResource = NULL;

        SrvElementsUnregisterResource(pFile->resource.ulResourceId, &pResource);
        pFile->resource.ulResourceId = 0;
    }
    if (pFile->hFile)
    {
        IoCancelFile(pFile->hFile);
    }

    SrvOplockStateRundown(pFile);
}

static
VOID
SrvFileFree(
    PLWIO_SRV_FILE pFile
    )
{
    LWIO_LOG_DEBUG("Freeing file [object:0x%x][fid:%u]",
                    pFile,
                    pFile->fid);

    if (pFile->pMutex)
    {
        pthread_rwlock_destroy(&pFile->mutex);
        pFile->pMutex = NULL;
    }

    if (pFile->pFilename)
    {
        if (pFile->pFilename->FileName)
        {
            SrvFreeMemory (pFile->pFilename->FileName);
        }

        SrvFreeMemory(pFile->pFilename);
    }

    if (pFile->pwszFilename)
    {
        SrvFreeMemory(pFile->pwszFilename);
    }

    if (pFile->hOplockState && pFile->pfnFreeOplockState)
    {
        pFile->pfnFreeOplockState(pFile->hOplockState);
    }

    if (pFile->hCancellableBRLStateList && pFile->pfnFreeBRLStateList)
    {
        pFile->pfnFreeBRLStateList(pFile->hCancellableBRLStateList);
    }

    if (pFile->hFile)
    {
        IoCloseFile(pFile->hFile);
    }

    if (pFile->resource.ulResourceId)
    {
        PSRV_RESOURCE pResource = NULL;

        SrvElementsUnregisterResource(pFile->resource.ulResourceId, &pResource);
        pFile->resource.ulResourceId = 0;
    }

    SrvFreeMemory(pFile);
}



/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
