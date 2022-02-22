#pragma once
#include <cnpy/cnpy.h>

namespace cmn {
    template <typename T>
    void npz_save(std::string zipname, std::string fname, const T *data,
                  const std::vector<size_t> &shape, std::string mode = "w") {
        try {
            cnpy::npz_save(zipname, fname, data, shape, mode);
        } catch(const std::runtime_error& e) {
            throw U_EXCEPTION("Exception while saving '",zipname,"': ",e.what(),"");
        } catch(...) {
            throw U_EXCEPTION("Unknown exception while saving ",zipname,".");
        }
    }
    
    template<typename T> void npz_save(std::string zipname, std::string fname, const std::vector<T> data, std::string mode = "w") {
        try {
            cnpy::npz_save(zipname, fname, data, mode);
        } catch(const std::runtime_error& e) {
            throw U_EXCEPTION("Exception while saving '",zipname,"': ",e.what(),"");
        } catch(...) {
            throw U_EXCEPTION("Unknown exception while saving ",zipname,".");
        }
    }
}
