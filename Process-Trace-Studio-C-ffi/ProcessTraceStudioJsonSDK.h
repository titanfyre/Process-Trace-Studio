/* SPDX-License-Identifier: MIT */
#ifndef PROCESS_TRACE_STUDIO_CSDK_H
#define PROCESS_TRACE_STUDIO_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT
#endif
#if defined(PROCESS_TRACE_STUDIO_CSDK_BUILD)
#define PROCESS_TRACE_STUDIO_CSDK_API __declspec(dllexport)
#else
#define PROCESS_TRACE_STUDIO_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
PROCESS_TRACE_STUDIO_CSDK_API uint32_t process_trace_studio_json_open(uint64_t *session) PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT;
PROCESS_TRACE_STUDIO_CSDK_API uint32_t process_trace_studio_json_close(uint64_t session) PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT;
PROCESS_TRACE_STUDIO_CSDK_API char *process_trace_studio_json_call(uint64_t session,const char *request,uint32_t length) PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT;
PROCESS_TRACE_STUDIO_CSDK_API void process_trace_studio_json_free(char *response) PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef PROCESS_TRACE_STUDIO_CSDK_NOEXCEPT
#endif
