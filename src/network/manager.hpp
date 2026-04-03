#pragma once
#include <functional>
#undef _WINSOCKAPI_ 
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>

#include <network/packets/packet.hpp>
#include <serialization.hpp>

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <defs.hpp>

using namespace geode::prelude;

using DisconnectCallback = geode::Function<void(std::string)>;
using MiddlewareCb = geode::Function<void(std::function<void()> callback)>;

class CR_DLL NetworkManager : public CCObject {
public:
    static NetworkManager& get() {
        static NetworkManager instance;
        return instance;
    }

    void setDisconnectCallback(DisconnectCallback callback) {
        disconnectBtnCallback = std::move(callback);
    }

    void onDisconnect(DisconnectCallback callback) {
        disconnectEventCb = std::move(callback);
    }

    template<typename T>
    inline void addToQueue(T* packet) {
        this->packetQueue.push_back([this, packet]() { this->send(packet); });
    }

    bool showDisconnectPopup = true;
    bool isConnected = false;

    float responseTime = 0;

    MiddlewareCb middleware;

    void connect(bool shouldReconnect = true, std::function<void()> callback = []() {});
    void disconnect();

    template<typename T>
    requires std::is_base_of_v<Packet, T>
    inline void on(std::function<void(T)> callback, bool shouldUnbind = false) {
        listeners[T::PACKET_ID] = [callback = std::move(callback)](std::string msg) mutable {
            
#ifdef CR_DEBUG
            // Log the size of the incoming data to detect truncated or empty server responses
            log::debug("[Network] Received packet {}, Raw string size: {} bytes", T::PACKET_NAME, msg.size());
#endif

            std::stringstream ss(std::move(msg));
            T packet = T::create();

            try {
                cereal::JSONInputArchive iarchive(ss);
                iarchive(cereal::make_nvp("packet", packet));
            } catch (const std::exception& e) {
                // If the Java server sends missing fields, empty strings, or corrupted JSON, 
                // cereal will throw an exception here instead of silently passing a "void/empty" level.
                log::error("[Network] CRITICAL ERROR: Failed to parse packet {} ({}).", T::PACKET_NAME, T::PACKET_ID);
                log::error("[Network] Reason: The server sent corrupted, incomplete, or empty JSON data.");
                log::error("[Network] Cereal exception detail: {}", e.what());
                return; // Prevent sending an empty/broken level to the main game thread
            }

            Loader::get()->queueInMainThread([callback, packet = std::move(packet)]() mutable {
                callback(std::move(packet));
            });
        };
    }

    template<typename T>
    requires std::is_base_of_v<Packet, T>
    inline void unbind() {
        if (!this->isConnected) return;

        if (!listeners.contains(T::PACKET_ID)) {
            log::error("[Network] unable to remove listener for {} ({}), listener does not exist", T::PACKET_NAME, T::PACKET_ID);
            return;
        }

        listeners.erase(T::PACKET_ID);

        log::debug("[Network] removed listener for {} ({})", T::PACKET_NAME, T::PACKET_ID);
    }

    template<typename T>
    inline void send(T const& packet, std::function<void()> callback = nullptr) {
        if (!this->isConnected) {
            return;
        }

        std::stringstream ss;
        // cereal uses RAII, meaning the contents of `ss` is guaranteed
        // to be filled at the end of the braces
        {
            cereal::JSONOutputArchive oarchive(ss);
            oarchive(cereal::make_nvp("packet", packet));
        }
        
        auto jsonRes = matjson::parse(ss.str()).mapErr([](std::string err) { return err; });
        if (jsonRes.isErr()) {
            log::error("[Network] Failed to parse local JSON for packet {}: {}", packet.getPacketID(), jsonRes.unwrapErr());
            return;
        }
        auto json = jsonRes.unwrap();

        json["packet_id"] = packet.getPacketID();
        auto uncompressedStr = json.dump(0);

#ifdef CR_DEBUG
        // Log outgoing uncompressed size to see if the client generated an empty level string before sending
        log::debug("[Network] Sending packet {} ({}), Uncompressed size: {} bytes", packet.getPacketName(), packet.getPacketID(), uncompressedStr.size());
#endif

        unsigned char* compressedData = nullptr;
        size_t compressedSize = ZipUtils::ccDeflateMemory(
            reinterpret_cast<unsigned char*>(uncompressedStr.data()),
            uncompressedStr.size(),
            &compressedData
        );

        // SAFETY CHECK: Prevent sending "garbage" or empty data if compression fails due to memory/sync issues
        if (compressedSize == 0 || compressedData == nullptr) {
            log::error("[Network] CRITICAL ERROR: ccDeflateMemory failed to compress packet {} ({})!", packet.getPacketName(), packet.getPacketID());
            log::error("[Network] Reason: The level string generated by the game might be completely empty, or the device ran out of memory.");
            log::error("[Network] Packet was dropped to prevent sending a void level to the server.");
            if (compressedData) free(compressedData); // Prevent memory leak just in case
            return; 
        }

        ix::IXWebSocketSendData data(
            reinterpret_cast<const char*>(compressedData),
            compressedSize
        );
        socket.sendBinary(
            data, [callback = std::move(callback)](int current, int total) mutable {
                if (current + 1 == total && callback) {
                    Loader::get()->queueInMainThread([callback = std::move(callback)]() mutable {
                        callback();
                    });
                }
                return true;
            }
        );

        // VERY IMPORTANT: ZipUtils::ccDeflateMemory allocates memory using malloc internally.
        // We MUST free it here, otherwise the game will slowly leak memory and start lagging.
        free(compressedData);
    }
protected:
    NetworkManager();
    ix::WebSocket socket;

    void onMessage(const ix::WebSocketMessagePtr& msg);
    void update(float dt);

    DisconnectCallback disconnectBtnCallback;
    DisconnectCallback disconnectEventCb;

    std::unordered_map<
        int,
        geode::Function<void(std::string)>
    > listeners;

    std::vector<
        geode::Function<void()>
    > packetQueue;
};
