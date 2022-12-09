#pragma once

namespace cmn {
namespace CPULabeling {

class DLList;

struct ListCache_t {
    DLList* obj{ nullptr };
    ListCache_t();
    ~ListCache_t();
};

}
}
