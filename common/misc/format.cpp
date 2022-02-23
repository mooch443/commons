#include "format.h"
#include <file/Path.h>

#ifdef COMMONS_FORMAT_LOG_TO_FILE
namespace cmn {

//extern IMPLEMENT(PrefixLiterals::WARNING);
//extern IMPLEMENT(PrefixLiterals::EXCEPT);
//extern IMPLEMENT(PrefixLiterals::ERROR_PREFIX);

void log_to_file(std::string str) {
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
}

}
#endif
