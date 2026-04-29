#include <Geode/Geode.hpp>

#include <network/manager.hpp>
#include <network/packets/client.hpp>
#include <network/packets/server.hpp>

#include <defs.hpp>

using namespace geode::prelude;

class CR_DLL AuthManager : public UploadMessageDelegate {
public:
    static AuthManager& get() {
        static AuthManager instance;
        return instance;
    }

    void beginAuthorization(std::function<void()> callback = nullptr);
    void login(std::function<void()> callback = nullptr);

    // delegate stuff
    void uploadMessageFinished(int);
    void uploadMessageFailed(int);
protected:
    AuthManager();

    std::function<void()> m_loginCallback;

    void beginAuthorizationImpl();

    std::string getToken() {
        return Mod::get()->getSavedValue<std::string>("token", "");
    }

    void setToken(std::string token) {
        Mod::get()->setSavedValue<std::string>("token", token);
    }
};