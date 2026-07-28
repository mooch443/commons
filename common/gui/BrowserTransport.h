#pragma once

#include <commons.pc.h>
#include <gui/BrowserProtocol.h>

namespace cmn::gui::browser {
struct Asset {
    std::string path;
    std::string mime;
    std::string contents;
};

struct TransportConfig {
    std::string bind_address{"0.0.0.0"};
    uint16_t port{8080};
    size_t max_messages_per_client{64};
    size_t max_bytes_per_client{8 * 1024 * 1024};
    std::vector<Asset> assets;
};

class BrowserTransport {
public:
    using ClientId = uint64_t;
    using MessageCallback = std::function<void(ClientId, const ClientMessage&)>;
    using DisconnectCallback = std::function<void(ClientId)>;
    using ClientCountCallback = std::function<void(size_t)>;

private:
    class Impl;
    std::unique_ptr<Impl> _impl;

public:
    BrowserTransport();
    ~BrowserTransport();
    BrowserTransport(const BrowserTransport&) = delete;
    BrowserTransport& operator=(const BrowserTransport&) = delete;

    bool start(TransportConfig,
               MessageCallback,
               DisconnectCallback,
               ClientCountCallback,
               std::string* error = nullptr);
    void stop();
    bool running() const;
    size_t client_count() const;

    void set_scene_snapshot(std::shared_ptr<const std::vector<uint8_t>>);
    void set_resource(uint32_t resource_id, std::shared_ptr<const std::vector<uint8_t>>);
    void remove_resource(uint32_t resource_id);
    void broadcast(std::shared_ptr<const std::vector<uint8_t>>);
};
}
