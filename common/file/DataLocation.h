#pragma once

#include <commons.pc.h>
#include <file/Path.h>

namespace file {

class DataLocation {
public:
    static void register_path(std::string purpose, std::function<file::Path(file::Path)> fn);
    static void replace_path(std::string purpose, std::function<file::Path(file::Path)> fn);
    static file::Path parse(const std::string& purpose, file::Path path = file::Path());
    static bool is_registered(std::string purpose);
private:
    DataLocation() {}
};

}
