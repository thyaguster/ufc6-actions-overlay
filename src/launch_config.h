#ifndef UFC6_LAUNCH_CONFIG_H
#define UFC6_LAUNCH_CONFIG_H
#include <windows.h>
#define LAUNCH_PATH 1024
#define LAUNCH_ARGS 4096
typedef struct { wchar_t executable[LAUNCH_PATH],arguments[LAUNCH_ARGS],directory[LAUNCH_PATH]; int custom,wait_parent; } LaunchConfig;
void launch_config_default(const wchar_t *bin,LaunchConfig *config);
BOOL launch_config_load(const wchar_t *bin,LaunchConfig *config);
BOOL launch_config_read(const wchar_t *ini,const wchar_t *bin,LaunchConfig *config);
BOOL launch_config_valid(const LaunchConfig *config);
BOOL launch_config_save(const wchar_t *ini,const LaunchConfig *config);
BOOL launch_config_command(const LaunchConfig *config,wchar_t *command,size_t count);
BOOL launch_config_update_values(const wchar_t *ini,const wchar_t *section,const wchar_t *const *keys,const wchar_t *const *values,size_t count,const BY_HANDLE_FILE_INFORMATION *expected);
BOOL overlay_settings_reset(const wchar_t *ini);
#endif
