#ifndef UFC6_STARTUP_PROMPTS_H
#define UFC6_STARTUP_PROMPTS_H
#include <stdint.h>
/* Returns 1=PSN/EA requirement, 2=roster-download failure, 3=Press any button, 0=unrecognized.
   Recognizes only English 16:9 (720p or higher) warning layouts with their exact text signatures. */
typedef struct {int enabled,focused,visible,held,pulsing,controller;unsigned seen;uint64_t now,deadline;} StartupPromptContext;
int startup_prompt_can_confirm(int id,const StartupPromptContext *context);
int startup_prompt_identify(const unsigned char *pixels,uint32_t width,uint32_t height);
#endif
