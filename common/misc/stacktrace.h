// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0
// and https://stackoverflow.com/questions/590160/how-to-log-stack-frames-with-windows-x64 for Win32

#ifndef _STACKTRACE_H_
#define _STACKTRACE_H_

#include <cstdio>
#include <cstdlib>
//#include <iostream>
#if !defined(WIN32) && !defined(__EMSCRIPTEN__)
#include <execinfo.h>
#include <cxxabi.h>
#elif WIN32

#include "DbgHelp.h"
#include <WinBase.h>
#pragma comment(lib, "Dbghelp.lib")

#endif
#include <string>
#include <sstream>
#include <memory>

/** Print a demangled stack backtrace of the caller function to FILE* out. */
static inline void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63)
{
#if !defined(WIN32) && !defined(__EMSCRIPTEN__)
    fprintf(out, "stack trace:\n");
    
    // storage array for stack trace address data
    std::shared_ptr<void*> addrlist((void**)malloc(sizeof(void*) * (max_frames + 1)), free);
    //void* addrlist[max_frames+1];
    
    // retrieve current stack addresses
    int addrlen = backtrace(addrlist.get(), (int)max_frames + 1);//sizeof(addrlist.get()) / sizeof(void*));
    
    if (addrlen == 0) {
        fprintf(out, "  <empty, possibly corrupt>\n");
        return;
    }
    
    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist.get(), addrlen);
    
    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);
    
    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;
        
        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        for (char *p = symbollist[i]; *p; ++p)
        {
            if (*p == '(')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
            else if (*p == ')' && begin_offset) {
                end_offset = p;
                break;
            }
        }
        
        if (begin_name && begin_offset && end_offset
            && begin_name < begin_offset)
        {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';
            
            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():
            
            int status;
            char* ret = abi::__cxa_demangle(begin_name,
                                            funcname, &funcnamesize, &status);
            if (status == 0) {
                funcname = ret; // use possibly realloc()-ed string
                fprintf(out, "  %s : %s+%s\n",
                        symbollist[i], funcname, begin_offset);
            }
            else {
                // demangling failed. Output function name as a C function with
                // no arguments.
                fprintf(out, "  %s : %s()+%s\n",
                        symbollist[i], begin_name, begin_offset);
            }
        }
        else
        {
            // couldn't parse the line? print the whole line.
            fprintf(out, "  %s\n", symbollist[i]);
        }
    }
    
    free(funcname);
    free(symbollist);

#elif !defined(__EMSCRIPTEN__)
    typedef USHORT(WINAPI* CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
    CaptureStackBackTraceType func = (CaptureStackBackTraceType)(GetProcAddress(LoadLibrary("kernel32.dll"), "RtlCaptureStackBackTrace"));

    if (func == NULL)
        return; // WOE 29.SEP.2010

    // Quote from Microsoft Documentation:
    // ## Windows Server 2003 and Windows XP:  
    // ## The sum of the FramesToSkip and FramesToCapture parameters must be less than 63.
    constexpr int kMaxCallers = 62;

    void* callers_stack[kMaxCallers];
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE         process;
    process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    frames = (func)(0, kMaxCallers, callers_stack, NULL);
    symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    fprintf(out, "(%X):\n", max_frames);
    const unsigned short  MAX_CALLERS_SHOWN = max_frames;
    frames = frames < MAX_CALLERS_SHOWN ? frames : MAX_CALLERS_SHOWN;
    for (unsigned int i = 0; i < frames; i++)
    {
        SymFromAddr(process, (DWORD64)(callers_stack[i]), 0, symbol);
        fprintf(out, "*** %u: %p %s - 0x%p\n", i, callers_stack[i], symbol->Name, (void*)symbol->Address);
    }

    free(symbol);
#endif
}


static inline std::tuple<int, std::shared_ptr<void*>> retrieve_stacktrace(unsigned int max_frames = 63)
{
#if defined(WIN32) || defined(__EMSCRIPTEN__)
    return { 0, nullptr };
#else
    // storage array for stack trace address data
    std::shared_ptr<void*> addrlist((void**)malloc(sizeof(void*) * (max_frames + 1)), free);
    //void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist.get(), (int)max_frames + 1);//sizeof(addrlist.get()) / sizeof(void*));
    return { addrlen, addrlist };
#endif
}

#if !defined(WIN32) && !defined(__EMSCRIPTEN__)
static inline std::string resolve_stacktrace(const std::tuple<int, std::shared_ptr<void*>>& tuple) {
    auto && [addrlen, addrlist] = tuple;
    std::stringstream ss;
    
    if (addrlen == 0) {
        ss << "  <empty, possibly corrupt>" << std::endl;
        return ss.str();
    }
    
    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist.get(), addrlen);
    
    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);
    
    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
        char *begin_hex = 0, *begin_name = 0, *begin_offset = 0, *end_offset = 0;
        
        //!  18  tracker                             0x000000010002145a main + 49018
#if __APPLE__
        char *p;
        for (p = symbollist[i]; *p; ++p)
        {
            if (!begin_hex && *p == 'x')
                begin_hex = p;
            else if(begin_hex && !begin_name && !begin_offset && *p == ' ')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
        }
        
        end_offset = p;
#else
        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        for (char *p = symbollist[i]; *p; ++p)
        {
            if (*p == '(')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
            else if (*p == ')' && begin_offset) {
                end_offset = p;
                break;
            }
        }
#endif
        
        if (begin_name && begin_offset && end_offset
            && begin_name < begin_offset)
        {
            *begin_name++ = '\0';
            *(begin_offset-1) = '\0';
            ++begin_offset;
            *end_offset = '\0';
            
            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():
            
            int status;
            char* ret = abi::__cxa_demangle(begin_name,
                                            funcname, &funcnamesize, &status);
            if (status == 0) {
                funcname = ret; // use possibly realloc()-ed string
                ss << " " << symbollist[i] << ": " << funcname << "+" << begin_offset << std::endl;
            }
            else {
                // demangling failed. Output function name as a C function with
                // no arguments.
                ss << " " << symbollist[i] << ": " << begin_name << "+" << begin_offset << std::endl;
            }
        }
        else
        {
            // couldn't parse the line? print the whole line.
            ss << " " << symbollist[i] << std::endl;
        }
    }
    
    free(funcname);
    free(symbollist);
    
    return ss.str();
}
#endif

#endif // _STACKTRACE_H_
