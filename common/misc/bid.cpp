#include "bid.h"
#include <misc/metastring.h>
#include <misc/PVBlob.h>

namespace pv {
using namespace cmn;

std::string bid::toStr() const {
    if(!valid())
        return "null";
    return Meta::toStr<uint32_t>(_id);
}

bid bid::fromStr(const std::string& str) {
    if(str == "null")
        return pv::bid();
    return bid(Meta::fromStr<uint32_t>(str));
}

bid bid::from_blob(const pv::Blob &blob) {
    if(!blob.lines() || blob.lines()->empty())
        return bid::invalid;

    return from_data(blob.lines()->front().x0,
                     blob.lines()->front().x1,
                     blob.lines()->front().y,
                     blob.lines()->size());
}

bid bid::from_blob(const pv::CompressedBlob &blob) {
    if(blob.lines().empty())
        return bid::invalid;

    return from_data(blob.lines().front().x0(),
                     blob.lines().front().x1(),
                     blob.start_y,
                     blob.lines().size());
}

}
