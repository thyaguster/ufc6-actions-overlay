#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "performance_settings.h"
#include "launch_config.h"
static int path_for(const wchar_t *bin,wchar_t *ini){
 if(!bin||wcslen(bin)>LAUNCH_PATH-32)return 0;
 LaunchConfig launch;if(!launch_config_load(bin,&launch))return 0;
 wchar_t root[LAUNCH_PATH],executable[LAUNCH_PATH];lstrcpynW(root,bin,LAUNCH_PATH);wchar_t *slash=wcsrchr(root,L'\\');if(!slash)return 0;*slash=0;
 swprintf(executable,LAUNCH_PATH,L"%ls\\UFC6.exe",root);
 if(launch.custom&&lstrcmpiW(launch.executable,executable))return 0;
 DWORD attributes=GetFileAttributesW(executable);if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return 0;
 swprintf(ini,LAUNCH_PATH,L"%ls\\UFC6.ini",root);attributes=GetFileAttributesW(ini);return attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
static int flag(const wchar_t *ini,const wchar_t *key,int fallback,int *out){
 wchar_t value[32],*end;DWORD n=GetPrivateProfileStringW(L"UFC6",key,L"",value,32,ini);if(!n){*out=fallback;return 1;}long v=wcstol(value,&end,10);if(*end||v<0||v>1)return 0;*out=(int)v;return 1;
}
static int read_settings(const wchar_t *ini,PerformanceSettings *p){
 if(!flag(ini,L"crowd_off",0,&p->crowd_off)||!flag(ini,L"particles_off",0,&p->particles_off)||!flag(ini,L"dof_off",1,&p->dof_off)||!flag(ini,L"drs_off",1,&p->drs_off))return 0;
 wchar_t value[32],*end;GetPrivateProfileStringW(L"UFC6",L"render_scale",L"0",value,32,ini);long scale=wcstol(value,&end,10);if(*end||scale<0||scale>300)return 0;p->render_scale=(int)scale;return 1;
}
int performance_load(const wchar_t *bin,PerformanceSettings *out){wchar_t ini[LAUNCH_PATH];PerformanceSettings p;if(!out||!path_for(bin,ini)||!read_settings(ini,&p))return PERF_UNAVAILABLE;*out=p;return PERF_OK;}
int performance_save(const wchar_t *bin,const PerformanceSettings *expected,const PerformanceSettings *desired){
 if(!expected||!desired||desired->crowd_off<0||desired->crowd_off>1||desired->particles_off<0||desired->particles_off>1||desired->dof_off<0||desired->dof_off>1||desired->drs_off<0||desired->drs_off>1||desired->render_scale<0||desired->render_scale>300)return PERF_WRITE_FAILED;
 wchar_t ini[LAUNCH_PATH];PerformanceSettings current;if(!path_for(bin,ini))return PERF_UNAVAILABLE;
 WritePrivateProfileStringW(NULL,NULL,NULL,ini);
 HANDLE source=CreateFileW(ini,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);if(source==INVALID_HANDLE_VALUE)return PERF_WRITE_FAILED;
 if(!read_settings(ini,&current)){CloseHandle(source);return PERF_UNAVAILABLE;}if(!performance_equal(&current,expected)){CloseHandle(source);return PERF_CHANGED_EXTERNALLY;}
 const wchar_t *keys[]={L"crowd_off",L"particles_off",L"dof_off",L"drs_off",L"render_scale",L"render_scale_75"};wchar_t values[6][24];const wchar_t *data[6];int numbers[]={desired->crowd_off,desired->particles_off,desired->dof_off,desired->drs_off,desired->render_scale,0};
 for(int i=0;i<6;++i){swprintf(values[i],24,L"%d",numbers[i]);data[i]=values[i];}
 BY_HANDLE_FILE_INFORMATION expected_file={0};if(!GetFileInformationByHandle(source,&expected_file)){CloseHandle(source);return PERF_WRITE_FAILED;}CloseHandle(source);
 int scale_changed=desired->render_scale!=expected->render_scale||desired->drs_off!=expected->drs_off;
 size_t count=scale_changed&&(desired->render_scale||!desired->drs_off)?6:5;
 int ok=launch_config_update_values(ini,L"UFC6",keys,data,count,&expected_file);DWORD error=GetLastError();
 return ok?PERF_OK:error==ERROR_RETRY?PERF_CHANGED_EXTERNALLY:PERF_WRITE_FAILED;
}

int apply_parsec_preset(int preset_id){
 if(preset_id<0||preset_id>=PARSEC_PRESET_COUNT)return 0;
 wchar_t appdata[MAX_PATH];
 DWORD len=GetEnvironmentVariableW(L"APPDATA",appdata,MAX_PATH);
 if(!len||len>=MAX_PATH-64)return 0;

 wchar_t parsec_dir[MAX_PATH];
 swprintf(parsec_dir,MAX_PATH,L"%ls\\Parsec",appdata);
 CreateDirectoryW(parsec_dir,NULL);

 wchar_t cfg_path[MAX_PATH],bak_path[MAX_PATH];
 swprintf(cfg_path,MAX_PATH,L"%ls\\config.txt",parsec_dir);
 swprintf(bak_path,MAX_PATH,L"%ls\\config.txt.bak",parsec_dir);

 if(preset_id==PARSEC_PRESET_RESTORE){
  if(GetFileAttributesW(bak_path)==INVALID_FILE_ATTRIBUTES)return 0;
  return CopyFileW(bak_path,cfg_path,FALSE)?1:0;
 }

 if(GetFileAttributesW(bak_path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(cfg_path)!=INVALID_FILE_ATTRIBUTES){
  CopyFileW(cfg_path,bak_path,TRUE);
 }

 int bitrate=20,res_x=1920,res_y=1080;
 if(preset_id==PARSEC_PRESET_BALANCED){bitrate=28;res_x=2560;res_y=1440;}
 else if(preset_id==PARSEC_PRESET_LAN){bitrate=48;res_x=3840;res_y=2160;}

 char new_content[16384]="";
 FILE *f=_wfopen(cfg_path,L"r");
 if(f){
  char line[512];
  while(fgets(line,sizeof(line),f)){
   if(strstr(line,"encoder_bitrate")||strstr(line,"encoder_codec")||strstr(line,"encoder_min_qp")||
      strstr(line,"client_vsync")||strstr(line,"host_resolution_x")||strstr(line,"host_resolution_y"))continue;
   if(strlen(new_content)+strlen(line)<sizeof(new_content)-1024)strcat(new_content,line);
  }
  fclose(f);
 }

 char preset_lines[512];
 snprintf(preset_lines,sizeof(preset_lines),
  "\n# Parsectagon Preset (%s)\n"
  "encoder_bitrate = %d\n"
  "encoder_codec = 2\n"
  "encoder_min_qp = 5\n"
  "client_vsync = 0\n"
  "host_resolution_x = %d\n"
  "host_resolution_y = %d\n",
  preset_id==PARSEC_PRESET_ESPORTS?"Esports 1080p60":preset_id==PARSEC_PRESET_BALANCED?"Balanced 1440p60":"LAN 4K60",
  bitrate,res_x,res_y);

 if(strlen(new_content)+strlen(preset_lines)<sizeof(new_content)-1)strcat(new_content,preset_lines);

 f=_wfopen(cfg_path,L"w");
 if(!f)return 0;
 fputs(new_content,f);
 fclose(f);
 return 1;
}
