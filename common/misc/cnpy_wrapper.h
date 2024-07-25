#pragma once
#include <cnpy/cnpy.h>

namespace cmn {
    template <typename T>
    void npz_save(std::string zipname, std::string fname, const T *data,
                  const std::vector<size_t> &shape, std::string mode = "w") {
        try {
            cnpy::npz_save(zipname, fname, data, shape, mode);
        } catch(const std::runtime_error& e) {
            throw U_EXCEPTION("Exception while saving ",zipname,": ",e.what(),"");
        } catch(...) {
            throw U_EXCEPTION("Unknown exception while saving ",zipname,".");
        }
    }
    
    template<typename T> void npz_save(std::string zipname, std::string fname, const std::vector<T>& data, std::string mode = "w") {
        try {
            cnpy::npz_save(zipname, fname, data, mode);
        } catch(const std::runtime_error& e) {
            throw U_EXCEPTION("Exception while saving ",zipname,": ",e.what(),"");
        } catch(...) {
            throw U_EXCEPTION("Unknown exception while saving ",zipname,".");
        }
    }

    inline void npz_save(std::string zipname, std::string fname, const std::string& data, std::string mode = "w") {
        try {
            // Convert std::string to a std::vector<char>
            std::vector<char> charArray(data.begin(), data.end());

            // Save the char array to an NPZ file using cnpy
            cnpy::npz_save(zipname, fname, charArray.data(), {charArray.size()}, mode);
        } catch(const std::runtime_error& e) {
            throw U_EXCEPTION("Exception while saving ",zipname,": ",e.what(),"");
        } catch(...) {
            throw U_EXCEPTION("Unknown exception while saving ",zipname,".");
        }
    }
}
