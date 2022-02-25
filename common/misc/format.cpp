#include "format.h"
#include <file/Path.h>

#ifdef COMMONS_FORMAT_LOG_TO_FILE
namespace cmn {

extern IMPLEMENT(PrefixLiterals::names) = {
    "WARNING",
    "ERROR",
    "EXCEPT",
    "INFO"
};

static std::atomic_bool runtime_is_quiet{ false };

void set_runtime_quiet(bool quiet) {
    runtime_is_quiet = quiet;
}

std::function<void(PrefixLiterals::Prefix, const std::string&, bool)> log_callback_function;
//extern IMPLEMENT(PrefixLiterals::EXCEPT);
//extern IMPLEMENT(PrefixLiterals::ERROR_PREFIX);
void* set_debug_callback(std::function<void(PrefixLiterals::Prefix, const std::string&, bool)> fn) {
    log_callback_function = fn;
    return (void*) & fn;
}
void log_to_callback(const std::string& str, PrefixLiterals::Prefix prefix, bool force) {
    if (!has_log_callback())
        return;
    log_callback_function(prefix, str, force);
}
void log_to_terminal(const std::string& str, bool force_display) {
    if(!runtime_is_quiet || force_display)
        printf("%s\n", str.c_str());
}

bool has_log_callback() {
    return log_callback_function != nullptr;
}

void log_to_file(const std::string& str) {
#if !defined(__EMSCRIPTEN__)
    static FILE* f{ nullptr };
    
    if (!f) {
        f = file::Path("site.html").fopen("w");
        if (f) {
            static const char head[] =
                "<style>\n"
                "    body {\n"
                "        background:  black;\n"
                "        font-family: Cascadia Mono,Source Code Pro,monospace;\n"
                "    }\n"
                "    row { display: block; color:white; }\n"
                "    black { color: white; }\n"
                "    purple, pink { color: magenta; }\n"
                "    yellow { color: yellow; }\n"
                "    green { color:  lightgreen; }\n"
                "    red { color: red; }\n"
                "    darkgreen { color: green; }\n"
                "    lightblue { color: lightblue; }\n"
                "    lightcyan { color: lightcyan; }\n"
                "    cyan { color: cyan; }\n"
                "    darkred { color: darkred; }\n"
                "    darkcyan { color: darkcyan; }\n"
                "    gray {color: gray;}\n"
                "    darkgray {color:  darkgray;}\n"
                "</style>\n";

            fwrite((const void*)head, sizeof(char), sizeof(head) - 1, f);
        }
    }

    if (f) {
        
        fwrite((void*)str.c_str(), sizeof(char), str.length()-1, f);
        fflush(f);
    }
#endif
}

}
#endif
