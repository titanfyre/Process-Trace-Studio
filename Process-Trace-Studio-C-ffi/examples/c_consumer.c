#include "ProcessTraceStudioJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(process_trace_studio_json_open(&session))return 1;char *reply=process_trace_studio_json_call(session,"{",1);if(!reply){process_trace_studio_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;process_trace_studio_json_free(reply);if(process_trace_studio_json_close(session))return 3;return failed;}
