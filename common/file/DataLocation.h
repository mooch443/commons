#pragma once

#include <commons.pc.h>
#include <file/Path.h>

namespace file {

class DataLocation {
    std::mutex location_mutex;
    std::map<std::string, std::function<file::Path(file::Path)>> location_funcs;
public:
    static void register_path(std::string purpose, std::function<file::Path(file::Path)> fn);
    static void replace_path(std::string purpose, std::function<file::Path(file::Path)> fn);
    static file::Path parse(const std::string& purpose, file::Path path = file::Path());
    static bool is_registered(std::string purpose);

    //! managing instances in order to support Windows DLLs
    static DataLocation* instance();
    static void set_instance(DataLocation* instance);
private:
    DataLocation() {}
};

}
