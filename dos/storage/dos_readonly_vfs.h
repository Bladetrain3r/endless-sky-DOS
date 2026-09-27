/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_READONLY_VFS_H
#define DOS_READONLY_VFS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Counts completed xRead calls and bytes actually returned by stdio. */
unsigned long long dos_vfs_reads(void);
unsigned long long dos_vfs_bytes(void);
void dos_vfs_reset(void);

#ifdef __cplusplus
}
#endif

#endif
