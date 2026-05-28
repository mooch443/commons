module;

#include <commons.pc.h>
#include "generated/commons.includes.http.inc"

export module commons.http;

#if WITH_MHD
#include "generated/commons.exports.http.inc"
#else
export namespace cmn::http_module {
inline constexpr bool enabled = false;
}
#endif
