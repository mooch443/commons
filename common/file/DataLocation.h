#pragma once

#include <commons.pc.h>
#include <file/PathArray.h>

namespace cmn::file {

class DataLocation {
    using Resolver = std::function<Path(const sprite::Map&, Path)>;
    std::mutex location_mutex;
    std::map<std::string, Resolver> location_funcs;
    static Resolver lookup(const std::string& purpose);
    static PathArray parse_files(const std::string& purpose, const PathArray& source, const sprite::Map* settings);
public:
    static void register_path(std::string purpose, std::function<file::Path(const sprite::Map&, file::Path)> fn);
    static void replace_path(std::string purpose, std::function<file::Path(const sprite::Map&, file::Path)> fn);
    /// Resolve an already chosen path for its purpose. For input and output files,
    /// callers supply the filename and extension. An absolute path keeps its location.
    static file::Path parse(const std::string& purpose, file::Path path = file::Path(), const sprite::Map* settings = nullptr);
    /// Expand source patterns and apply the registered scalar resolver to each absolute input.
    template<std::same_as<PathArray> Array>
    static PathArray parse(const std::string& purpose, const Array& source, const sprite::Map* settings = nullptr) {
        return parse_files(purpose, source, settings);
    }
    static bool is_registered(std::string purpose);
    /// Clears all registered path resolvers while keeping the singleton available.
    static void unregister_locations();

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
