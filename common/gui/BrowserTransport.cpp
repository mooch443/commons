#include "BrowserTransport.h"

#if COMMONS_HAS_HTTPD
#include <httplib.h>
#endif

namespace cmn::gui::browser {
#if COMMONS_HAS_HTTPD
class BrowserTransport::Impl {
    struct Client {
        ClientId id{0};
        httplib::ws::WebSocket* socket{nullptr};
        size_t max_messages{0};
        size_t max_bytes{0};
        std::mutex mutex;
        std::condition_variable condition;
        std::deque<std::shared_ptr<const std::vector<uint8_t>>> queue;
        size_t queued_bytes{0};
        bool stopping{false};
        std::thread sender;

        static std::optional<MessageType> message_type(const std::vector<uint8_t>& message) {
            if(message.size() < 16)
                return std::nullopt;
            const auto raw = uint16_t(message[6]) | (uint16_t(message[7]) << 8);
            return static_cast<MessageType>(raw);
        }

        static std::optional<uint32_t> image_resource(const std::vector<uint8_t>& message) {
            if(message_type(message) != MessageType::Image || message.size() < 20)
                return std::nullopt;
            uint32_t resource = 0;
            for(size_t index = 0; index < 4; ++index)
                resource |= uint32_t(message[16 + index]) << (index * 8);
            return resource;
        }

        template<typename Predicate>
        void erase_queued(Predicate&& predicate) {
            for(auto found = queue.begin(); found != queue.end();) {
                if(*found && predicate(**found)) {
                    queued_bytes -= (*found)->size();
                    found = queue.erase(found);
                } else {
                    ++found;
                }
            }
        }

        void append_recovery(const std::vector<std::shared_ptr<const std::vector<uint8_t>>>& recovery) {
            for(const auto& item : recovery) {
                if(item && queue.size() < max_messages && queued_bytes + item->size() <= max_bytes) {
                    queued_bytes += item->size();
                    queue.push_back(item);
                }
            }
        }

        void enqueue(std::shared_ptr<const std::vector<uint8_t>> message,
                     const std::vector<std::shared_ptr<const std::vector<uint8_t>>>& recovery)
        {
            if(not message || message->empty())
                return;
            std::lock_guard guard(mutex);
            if(stopping)
                return;

            const auto type = message_type(*message);
            if(type == MessageType::Snapshot || type == MessageType::Delta) {
                const bool scene_pending = std::any_of(queue.begin(), queue.end(), [](const auto& queued) {
                    if(not queued)
                        return false;
                    const auto queued_type = message_type(*queued);
                    return queued_type == MessageType::Snapshot || queued_type == MessageType::Delta;
                });
                if(scene_pending) {
                    erase_queued([](const auto& queued) {
                        const auto queued_type = message_type(queued);
                        return queued_type == MessageType::Snapshot
                            || queued_type == MessageType::Delta
                            || queued_type == MessageType::Image;
                    });
                    append_recovery(recovery);
                    condition.notify_one();
                    return;
                }
            } else if(type == MessageType::Image) {
                const auto resource = image_resource(*message);
                erase_queued([&](const auto& queued) {
                    return resource && image_resource(queued) == resource;
                });
            } else if(type == MessageType::Clients) {
                erase_queued([](const auto& queued) {
                    return message_type(queued) == MessageType::Clients;
                });
            }

            if(queue.size() + 1 > max_messages || queued_bytes + message->size() > max_bytes) {
                queue.clear();
                queued_bytes = 0;
                append_recovery(recovery);
            } else {
                queued_bytes += message->size();
                queue.push_back(std::move(message));
            }
            condition.notify_one();
        }

        void begin_sender() {
            sender = std::thread([this] {
                cmn::set_thread_name("browser-ws-send");
                while(true) {
                    std::shared_ptr<const std::vector<uint8_t>> message;
                    {
                        std::unique_lock guard(mutex);
                        condition.wait(guard, [&] { return stopping || not queue.empty(); });
                        if(stopping && queue.empty())
                            return;
                        message = std::move(queue.front());
                        queue.pop_front();
                        queued_bytes -= message->size();
                    }
                    if(not socket->send(reinterpret_cast<const char*>(message->data()), message->size())) {
                        std::lock_guard guard(mutex);
                        stopping = true;
                        condition.notify_all();
                        return;
                    }
                }
            });
        }

        void shutdown() {
            {
                std::lock_guard guard(mutex);
                stopping = true;
                queue.clear();
                queued_bytes = 0;
            }
            condition.notify_all();
            if(socket && socket->is_open())
                socket->close();
            if(sender.joinable())
                sender.join();
        }
    };

    TransportConfig _config;
    httplib::Server _server;
    std::thread _server_thread;
    std::atomic_bool _running{false};
    std::atomic<ClientId> _next_client{1};
    mutable std::mutex _mutex;
    std::unordered_map<ClientId, std::shared_ptr<Client>> _clients;
    std::shared_ptr<const std::vector<uint8_t>> _scene_snapshot;
    std::map<uint32_t, std::shared_ptr<const std::vector<uint8_t>>> _resources;
    MessageCallback _message_callback;
    DisconnectCallback _disconnect_callback;
    ClientCountCallback _count_callback;

    std::vector<std::shared_ptr<const std::vector<uint8_t>>> bootstrap_locked() const {
        std::vector<std::shared_ptr<const std::vector<uint8_t>>> result;
        if(_scene_snapshot)
            result.push_back(_scene_snapshot);
        for(const auto& [id, resource] : _resources) {
            UNUSED(id);
            result.push_back(resource);
        }
        return result;
    }

    void websocket(httplib::ws::WebSocket& socket) {
        auto client = std::make_shared<Client>();
        client->id = _next_client.fetch_add(1);
        client->socket = &socket;
        client->max_messages = _config.max_messages_per_client;
        client->max_bytes = _config.max_bytes_per_client;

        std::vector<std::shared_ptr<const std::vector<uint8_t>>> bootstrap;
        size_t count;
        {
            std::lock_guard guard(_mutex);
            _clients.emplace(client->id, client);
            bootstrap = bootstrap_locked();
            count = _clients.size();
        }
        client->begin_sender();
        for(const auto& message : bootstrap)
            client->enqueue(message, bootstrap);
        if(_count_callback)
            _count_callback(count);

        auto enqueue_error = [&](std::string_view message) {
            std::vector<std::shared_ptr<const std::vector<uint8_t>>> recovery;
            {
                std::lock_guard guard(_mutex);
                recovery = bootstrap_locked();
            }
            client->enqueue(
                std::make_shared<const std::vector<uint8_t>>(error_message(0, message)),
                recovery);
        };

        std::string payload;
        while(_running && socket.is_open()) {
            const auto result = socket.read(payload);
            if(result == httplib::ws::ReadResult::Fail)
                break;
            if(result != httplib::ws::ReadResult::Binary) {
                enqueue_error("Binary protocol messages are required.");
                continue;
            }

            std::string parse_error;
            auto message = decode_client_message(
                {reinterpret_cast<const uint8_t*>(payload.data()), payload.size()}, &parse_error);
            if(not message) {
                enqueue_error(parse_error);
                continue;
            }
            if(message->type == MessageType::Resync) {
                std::vector<std::shared_ptr<const std::vector<uint8_t>>> current;
                {
                    std::lock_guard guard(_mutex);
                    current = bootstrap_locked();
                }
                for(const auto& item : current)
                    client->enqueue(item, current);
            } else if(_message_callback) {
                _message_callback(client->id, *message);
            }
        }

        client->shutdown();
        {
            std::lock_guard guard(_mutex);
            _clients.erase(client->id);
            count = _clients.size();
        }
        if(_disconnect_callback)
            _disconnect_callback(client->id);
        if(_count_callback)
            _count_callback(count);
    }

public:
    bool start(TransportConfig config,
               MessageCallback message_callback,
               DisconnectCallback disconnect_callback,
               ClientCountCallback count_callback,
               std::string* error)
    {
        if(_running)
            return true;
        _config = std::move(config);
        _message_callback = std::move(message_callback);
        _disconnect_callback = std::move(disconnect_callback);
        _count_callback = std::move(count_callback);

        for(const auto& asset : _config.assets) {
            const bool immutable = asset.path.starts_with("/fonts/");
            _server.Get(asset.path, [contents = asset.contents, mime = asset.mime, immutable](const auto&, auto& response) {
                response.set_header("Cache-Control", immutable
                    ? "public, max-age=31536000, immutable"
                    : "no-cache");
                response.set_header("X-Content-Type-Options", "nosniff");
                response.set_header("Content-Security-Policy", "default-src 'self'; connect-src 'self' ws: wss:; img-src 'self' blob:; style-src 'self'; script-src 'self'; font-src 'self'");
                response.set_content(contents, mime);
            });
        }
        _server.Get("/", [this](const auto& request, auto& response) {
            UNUSED(request);
            const auto found = std::find_if(_config.assets.begin(), _config.assets.end(), [](const auto& asset) {
                return asset.path == "/index.html";
            });
            if(found == _config.assets.end()) {
                response.status = 404;
                return;
            }
            response.set_header("Cache-Control", "no-cache");
            response.set_header("X-Content-Type-Options", "nosniff");
            response.set_header("Content-Security-Policy", "default-src 'self'; connect-src 'self' ws: wss:; img-src 'self' blob:; style-src 'self'; script-src 'self'; font-src 'self'");
            response.set_content(found->contents, found->mime);
        });
        _server.WebSocket("/ws", [this](const auto&, auto& socket) { websocket(socket); });
        _server.set_websocket_ping_interval(15);
        _server.set_websocket_max_missed_pongs(3);

        if(not _server.bind_to_port(_config.bind_address, _config.port)) {
            if(error)
                *error = "Could not bind browser GUI server to " + _config.bind_address + ":" + std::to_string(_config.port) + ".";
            return false;
        }
        _running = true;
        _server_thread = std::thread([this] {
            cmn::set_thread_name("browser-httpd");
            _server.listen_after_bind();
            _running = false;
        });
        return true;
    }

    void stop() {
        if(not _running.exchange(false) && not _server_thread.joinable())
            return;
        std::vector<std::shared_ptr<Client>> clients;
        {
            std::lock_guard guard(_mutex);
            for(const auto& [id, client] : _clients) {
                UNUSED(id);
                clients.push_back(client);
            }
        }
        for(auto& client : clients) {
            if(client->socket && client->socket->is_open())
                client->socket->close();
        }
        _server.stop();
        if(_server_thread.joinable())
            _server_thread.join();
    }

    bool running() const { return _running; }

    size_t client_count() const {
        std::lock_guard guard(_mutex);
        return _clients.size();
    }

    void set_scene_snapshot(std::shared_ptr<const std::vector<uint8_t>> snapshot) {
        std::lock_guard guard(_mutex);
        _scene_snapshot = std::move(snapshot);
    }

    void set_resource(uint32_t id, std::shared_ptr<const std::vector<uint8_t>> resource) {
        std::lock_guard guard(_mutex);
        _resources[id] = std::move(resource);
    }

    void remove_resource(uint32_t id) {
        std::lock_guard guard(_mutex);
        _resources.erase(id);
    }

    void broadcast(std::shared_ptr<const std::vector<uint8_t>> message) {
        std::vector<std::shared_ptr<Client>> clients;
        std::vector<std::shared_ptr<const std::vector<uint8_t>>> recovery;
        {
            std::lock_guard guard(_mutex);
            recovery = bootstrap_locked();
            for(const auto& [id, client] : _clients) {
                UNUSED(id);
                clients.push_back(client);
            }
        }
        for(auto& client : clients)
            client->enqueue(message, recovery);
    }
};
#else
class BrowserTransport::Impl {
public:
    bool start(TransportConfig, MessageCallback, DisconnectCallback, ClientCountCallback, std::string* error) {
        if(error) *error = "This commons build does not include browser HTTPD support.";
        return false;
    }
    void stop() {}
    bool running() const { return false; }
    size_t client_count() const { return 0; }
    void set_scene_snapshot(std::shared_ptr<const std::vector<uint8_t>>) {}
    void set_resource(uint32_t, std::shared_ptr<const std::vector<uint8_t>>) {}
    void remove_resource(uint32_t) {}
    void broadcast(std::shared_ptr<const std::vector<uint8_t>>) {}
};
#endif

BrowserTransport::BrowserTransport() : _impl(std::make_unique<Impl>()) {}
BrowserTransport::~BrowserTransport() { stop(); }

bool BrowserTransport::start(TransportConfig config,
                             MessageCallback message_callback,
                             DisconnectCallback disconnect_callback,
                             ClientCountCallback count_callback,
                             std::string* error)
{
    return _impl->start(std::move(config), std::move(message_callback), std::move(disconnect_callback), std::move(count_callback), error);
}
void BrowserTransport::stop() { _impl->stop(); }
bool BrowserTransport::running() const { return _impl->running(); }
size_t BrowserTransport::client_count() const { return _impl->client_count(); }
void BrowserTransport::set_scene_snapshot(std::shared_ptr<const std::vector<uint8_t>> snapshot) { _impl->set_scene_snapshot(std::move(snapshot)); }
void BrowserTransport::set_resource(uint32_t id, std::shared_ptr<const std::vector<uint8_t>> resource) { _impl->set_resource(id, std::move(resource)); }
void BrowserTransport::remove_resource(uint32_t id) { _impl->remove_resource(id); }
void BrowserTransport::broadcast(std::shared_ptr<const std::vector<uint8_t>> message) { _impl->broadcast(std::move(message)); }
}
