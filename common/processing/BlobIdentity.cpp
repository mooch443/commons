#include "BlobIdentity.h"

#include <processing/PVBlob.h>

namespace pv {

bid blob_bid(const Blob& blob) {
    if (!blob.lines() || blob.lines()->empty()) {
        return bid::invalid;
    }

    return bid::from_data(blob.lines()->front().x0,
                          blob.lines()->front().x1,
                          blob.lines()->front().y,
                          blob.lines()->size());
}

bid blob_bid(const CompressedBlob& blob) {
    if (blob.lines().empty()) {
        return bid::invalid;
    }

    return bid::from_data(blob.lines().front().x0(),
                          blob.lines().front().x1(),
                          blob.start_y,
                          blob.lines().size());
}

}
