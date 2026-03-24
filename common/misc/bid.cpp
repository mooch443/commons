#include "bid.h"

namespace pv {
using namespace cmn;

const char* filter_reason_to_str(FilterReason reason) {
    switch (reason) {
        case FilterReason::DontTrackTags: return "Tags are not tracked";
        case FilterReason::OutsideRange: return "Inacceptable size";
        case FilterReason::InsideIgnore: return "Inside ignored shape (track_ignore)";
        case FilterReason::OutsideInclude: return "Outside track_include shape";
        case FilterReason::SecondThreshold: return "Outside range after track_threshold_2";
        case FilterReason::Category: return "track_only_categories";
        case FilterReason::Label: return "track_only_classes";
        case FilterReason::TrackConfidenceThreshold: return "track_conf_threshold";
        case FilterReason::SplitFailed: return "Split failed";
        case FilterReason::OnlySegmentations: return "Only segmentations are tracked";
        case FilterReason::BdxIgnored: return "Inside track_ignore_bdx";
        case FilterReason::History: return "history_split";
        case FilterReason::Unknown:
        default:
            return "unknown";
    }
}

std::string bid::toStr() const {
    if (!valid()) {
        return "null";
    }
    return Meta::toStr<uint32_t>(_id);
}

glz::json_t bid::to_json() const {
    if (!valid()) {
        return glz::json_t::null_t{};
    }
    return _id;
}

}
