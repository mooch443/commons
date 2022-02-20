#include <misc/format.h>

#ifdef COMMONS_FORMAT_LOG_TO_FILE
namespace cmn {

void log_to_file(std::string str) {
    static FILE* f{ nullptr };
    str = "[" + console_color<FormatColor::CYAN, FormatterType::UNIX>( current_time_string() ) + "]"
        + console_color<FormatColor::BLACK, FormatterType::HTML>(str);
    
    if (!f) {
        f = fopen("site.html", "w");
        if (f) {
            static const char head[] =
                "<style>\n"
                "    body {\n"
                "        background:  black;\n"
                "        font-family: Cascadia Mono,Source Code Pro,monospace;\n"
                "    }\n"
                "    black { color: white; }\n"
                "    purple { color: purple; }\n"
                "    yellow { color: yellow; }\n"
                "    green { color:  green; }\n"
                "    red { color: red; }\n"
                "    darkgreen { color: darkgreen; }\n"
                "    lightblue { color: lightblue; }\n"
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
}

}
#endif
