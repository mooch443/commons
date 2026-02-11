#include "ask_for_permission.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>   // for NSApplication (needed in some sandboxed contexts)
#include <future>
#include <memory>

namespace cmn::file {
// Helper that resolves a bookmark on a background queue
static std::expected<void, std::string> resolve_bookmark(const std::string& path)
{
    @autoreleasepool {
        // 1️⃣ Create a bookmark (non‑persistent, just for this run)
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
        if (!url) {
            return std::unexpected("Invalid URL");
        }
        
        // 2️⃣ Resolve the bookmark – this is where the system may pop up a dialog
        NSError *err = nil;
        // 1️⃣ Create the bookmark data (this is usually stored elsewhere).
        NSData *bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                         includingResourceValuesForKeys:nil
                                          relativeToURL:nil
                                                  error:&err];
        if (!bookmark) {
            // The bookmark creation failed – return nil.
            return std::unexpected("Cannot resolve url");
        }
        
        return {};
    }
}

std::expected<void, std::string> request_access(const std::string& path)
{
    // 1️⃣ Create a promise/future pair
    auto prom = std::make_shared<std::promise<std::expected<void, std::string>>>();
    auto fut  = prom->get_future();
    
    // 2️⃣ Dispatch the heavy work to a background queue
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   // Move the shared‑ptr into the block.  The block is *mutable*
                   // so we can call set_value().
                   [p = std::move(prom), path]() mutable {
        auto result = resolve_bookmark(path);   // <-- your heavy work
        p->set_value(std::move(result));       // signal completion
    });
    
    // 3️⃣ Wait for the result (blocks until set_value() is called)
    return fut.get();
}

}
