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
 *        attrib_xattr.c
 *
 * Abstract:
 *
 *        Likewise Posix File System Driver (PVFS)
 *
 *        Extended Attribute Dos Attributes implementation
 *
 * Authors: Gerald Carter <gcarter@likewise.com>
 */

#include "pvfs.h"

#ifdef HAVE_EA_SUPPORT


#define PVFS_EA_ATTRIB_NAME       "user.lwio_attrib_v1"

/* Forward declarations */


/* File Globals */



/* Code */


/****************************************************************
 ***************************************************************/

NTSTATUS
PvfsGetFilenameAttributesXattr(
    IN PSTR pszPath,
    OUT PFILE_ATTRIBUTES pAttributes
    )
{
    NTSTATUS ntError = STATUS_NO_SECURITY_ON_OBJECT;
    int iEaSize = 0;
    int unixerr = 0;

    iEaSize = getxattr(pszPath,
                       PVFS_EA_ATTRIB_NAME,
                       (PVOID)pAttributes,
                       sizeof(*pAttributes));
    if (iEaSize == -1) {
        PVFS_BAIL_ON_UNIX_ERROR(unixerr, ntError);
    }

    ntError = STATUS_SUCCESS;

cleanup:
    return ntError;

error:
    goto cleanup;
}

/****************************************************************
 ***************************************************************/

NTSTATUS
PvfsGetFileAttributesXattr(
    IN PPVFS_CCB pCcb,
    OUT PFILE_ATTRIBUTES pAttributes
    )
{
    NTSTATUS ntError = STATUS_NO_SECURITY_ON_OBJECT;
    int iEaSize = 0;
    int unixerr = 0;

    iEaSize = fgetxattr(pCcb->fd,
                        PVFS_EA_ATTRIB_NAME,
                        (PVOID)pAttributes,
                        sizeof(*pAttributes));
    if (iEaSize == -1) {
        PVFS_BAIL_ON_UNIX_ERROR(unixerr, ntError);
    }

    ntError = STATUS_SUCCESS;

cleanup:
    return ntError;

error:
    goto cleanup;
}

/****************************************************************
 ***************************************************************/

NTSTATUS
PvfsSetFileAttributesXattr(
    IN PPVFS_CCB pCcb,
    IN FILE_ATTRIBUTES Attributes
    )
{
    NTSTATUS ntError = STATUS_ACCESS_DENIED;
    int unixerr = 0;
    int iRet = 0;

    iRet = fsetxattr(pCcb->fd,
                     PVFS_EA_ATTRIB_NAME,
                     (PVOID)&Attributes,
                     sizeof(Attributes),
                     0);
    if (iRet == -1) {
        PVFS_BAIL_ON_UNIX_ERROR(unixerr, ntError);
    }

    ntError = STATUS_SUCCESS;

cleanup:
    return ntError;

error:
    goto cleanup;
}


#endif   /* HAVE_EA_SUPPORT  */


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/

