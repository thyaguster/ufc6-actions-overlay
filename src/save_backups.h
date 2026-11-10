#ifndef UFC6_SAVE_BACKUPS_H
#define UFC6_SAVE_BACKUPS_H
#include <windows.h>
#include <stddef.h>
enum { BACKUP_READY, BACKUP_BUSY, BACKUP_SAVED, BACKUP_ERROR };
void save_backups_init(HMODULE module);
void save_backups_create(void);
int save_backups_state(wchar_t *message,size_t count);
BOOL save_backups_open(void);
#endif
