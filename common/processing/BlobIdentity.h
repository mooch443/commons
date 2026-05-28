#pragma once

#include <misc/bid.h>

namespace pv {

class Blob;
struct CompressedBlob;

bid blob_bid(const Blob& blob);
bid blob_bid(const CompressedBlob& blob);

}
