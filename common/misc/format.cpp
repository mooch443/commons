#include "format.h"
#include <file/Path.h>
#include <misc/GlobalSettings.h>

#ifdef COMMONS_FORMAT_LOG_TO_FILE
namespace cmn {

extern IMPLEMENT(PrefixLiterals::names) = {
    "WARNING",
    "ERROR",
    "EXCEPT",
    "INFO"
};

static std::atomic_bool runtime_is_quiet{ false };
static std::mutex log_file_mutex;
static file::Path runtime_log_file{ "" };
static FILE* f{ nullptr };

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

bool has_log_file() {
    std::lock_guard guard(log_file_mutex);
    return !runtime_log_file.empty();
}
void set_log_file(const std::string& path) {
    std::lock_guard guard(log_file_mutex);
    if (f)
        fclose(f);
    f = nullptr;
    runtime_log_file = file::Path(path);
}

void write_log_message(const std::string& str) {
#if !defined(__EMSCRIPTEN__)
    if (!has_log_file())
        return;

    static std::once_flag flag;
    std::call_once(flag, []() {
        std::lock_guard guard(log_file_mutex);
        f = runtime_log_file.fopen("w");
        if (f) {
            static const char head[] =
                "<style>\n"
                "    body {\n"
                "        background: rgb(19, 19, 19);\n"
                "        font-family: Cascadia Mono,Source Code Pro,monospace;\n"
                "        font-size: 13px;\n"
                "    }\n"
                "    row { display: block; color:white; }\n"
                "    black { color: white; }\n"
                "    purple { color: rgb(255, 125, 255); }\n"
                "    pink { color: rgb(255, 107, 255); }\n"
                "    yellow { color: yellow; }\n"
                "    green { color:  lightgreen; }\n"
                "    red { color: red; }\n"
                "    darkgreen { color: green; }\n"
                "    lightblue { color: lightblue; }\n"
                "    lightcyan { color: lightcyan; }\n"
                "    cyan { color: cyan; }\n"
                "    darkred { color: darkred; }\n"
                "    darkcyan { color: rgb(0, 176, 176); }\n"
                "    gray {color: gray;}\n"
                "    lightgray {color: lightgray;}\n"
                "    darkgray {color:  darkgray;}\n"
                "</style>\n";

            fwrite((const void*)head, sizeof(char), sizeof(head) - 1, f);
        }
    });

    std::lock_guard guard(log_file_mutex);
    if (f) {
        auto r = utils::find_replace(str, { {"\n", "<br/>"}, {"\t","&nbsp;&nbsp;&nbsp;&nbsp;"} }) + "\n";
        fwrite((void*)r.c_str(), sizeof(char), r.length(), f);
        fflush(f);
    }
#endif
}

}
#endif
