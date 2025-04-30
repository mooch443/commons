#ifndef _MISC_UTILS
#define _MISC_UTILS

#include <commons.pc.h>

/**
 * Provides a wrapper for a vector of enum values.
 * Items can be pushed (and are saved only once) and then the list can
 * be asked whether it contains a specific value.
 * Values can also be removed.
 */
template<typename T>
class OptionsList {
    std::vector<T> list;
    
public:
    OptionsList() {}
    OptionsList(const std::vector<T> &values) : list(values) { }
	const std::vector<T>& values() const { return list;  }
    
    void push(const T& v) { if(is(v)) return; list.push_back(v); }
    void remove(const T& v) {
        for(auto it = list.begin(); it != list.end(); ++it) {
            if(*it == v) {
                list.erase(it);
                return;
            }
        }
        
        //throw U_EXCEPTION("Cannot find value '",v,"' of enum in this OptionsList.");
        //throw U_EXCEPTION("Cannot find value '",v.toString(),"' of enumeration '",T::name(),"' in this OptionsList.");
    }
    void clear() {
        list.clear();
    }
    
    size_t size() const { return list.size(); }
    
    bool is(const T& v) const {
        for(auto &l : list)
            if(l == v)
                return true;
        return false;
    }
    
    bool operator==(const OptionsList<T> &other) const {
		return list == other.list;
    }
};

#endif
