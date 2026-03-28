#pragma once

#include <commons.pc.h>
#include <misc/Path.h>

namespace cmn::file {

class DataLocation {
    std::mutex location_mutex;
    std::map<std::string, std::function<file::Path(const cmn::sprite::Map&, file::Path)>> location_funcs;
public:
    static void register_path(std::string purpose, std::function<file::Path(const sprite::Map&, file::Path)> fn);
    static void replace_path(std::string purpose, std::function<file::Path(const sprite::Map&, file::Path)> fn);
    static file::Path parse(const std::string& purpose, file::Path path = file::Path(), const sprite::Map* settings = nullptr);
    static bool is_registered(std::string purpose);

    //! Explicitly create and register the owned singleton instance.
    //! Must be called before any register_path/parse/instance() access.
    static DataLocation& create();

    //! managing instances in order to support Windows DLLs
    static DataLocation* instance();
    static DataLocation* instance_if_set() noexcept;
    static void set_instance(DataLocation* instance);
private:
    DataLocation() {}
};

}
