#include <Geode/Geode.hpp>
#include "SwapManager.hpp"

#include <network/manager.hpp>

#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <matjson/reflect.hpp>

#include <layers/ChatPanel.hpp>
#include <utils.hpp>

using namespace geode::prelude;

#define CR_REQUIRE_CONNECTION() if(!NetworkManager::get().isConnected) return;

SwapManager::SwapManager() {
    auto& nm = NetworkManager::get();
    nm.onDisconnect([this](std::string) {
        this->currentLobbyCode = "";
    });
}

LobbySettings SwapManager::createDefaultSettings() {
    auto acc = cr::utils::createAccountType();

    return {
        .name = fmt::format("{}'s Lobby", acc.name),
        .turns = 4,
        .minutesPerTurn = 5,
        .owner = acc,
        .isPublic = false
    };
}

void SwapManager::createLobby(LobbySettings lobby, std::function<void(std::string)> callback) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    nm.send(CreateLobbyPacket::create(lobby));
    nm.on<LobbyCreatedPacket>([this, callback](LobbyCreatedPacket createdLobby) {
        auto code = createdLobby.info.code;
        this->joinLobby(
            std::move(code),
            [createdLobby = std::move(createdLobby), callback]() {
                callback(std::move(createdLobby.info.code));
            }
        );
    });
}

void SwapManager::joinLobby(std::string code, std::function<void()> callback) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();

    nm.send(JoinLobbyPacket::create(code, cr::utils::createAccountType()));

    nm.on<JoinedLobbyPacket>([this, callback, code](auto) {
        this->currentLobbyCode = code;
        callback();
    }, true);
}

void SwapManager::getLobbyAccounts(std::function<void(std::vector<Account>)> callback, std::function<void(std::string)> onError) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    nm.send(GetAccountsPacket::create(this->currentLobbyCode));
    nm.on<ReceiveAccountsPacket>([callback = std::move(callback), onError = std::move(onError)](ReceiveAccountsPacket accounts) {
        if (onError && accounts.accounts.empty()) {
            onError("No accounts in lobby");
        }
        callback(std::move(accounts.accounts));
    }, true);
}

void SwapManager::getLobbyInfo(std::function<void(LobbyInfo)> callback, std::function<void(std::string)> onError) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    nm.send(GetLobbyInfoPacket::create(this->currentLobbyCode));
    nm.on<ReceiveLobbyInfoPacket>([callback = std::move(callback), onError = std::move(onError)](ReceiveLobbyInfoPacket packet) {
        if (onError && packet.info.accounts.empty()) {
            log::warn("Lobby info received but no accounts (possible error)");
        }
        callback(std::move(packet.info));
    }, true);
}

void SwapManager::updateLobby(LobbySettings updatedLobby) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();

    nm.send(UpdateLobbyPacket::create(this->currentLobbyCode, std::move(updatedLobby)));
}

void SwapManager::disconnectLobby() {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    this->currentLobbyCode = "";
    ChatPanel::clearMessages();

    // no longer needed
    // nm.send(DisconnectFromLobbyPacket::create());
    nm.disconnect();
}

// LEVEL SWAP //

void SwapManager::startSwap(SwapStartedPacket packet) {
    getLobbyInfo([this](LobbyInfo info) {
        secondsPerRound = info.settings.minutesPerTurn * 60;
        log::info("Swap started. {} minutes per turn", info.settings.minutesPerTurn);
    });

    registerListeners();

    auto glm = GameLevelManager::sharedState();
    auto newLvl = glm->createNewLevel();

    levelId = EditorIDs::getID(newLvl);
    
    log::info("Created new level with ID: {}", levelId);

    roundStartedTime = time(nullptr);

    auto scene = EditLevelLayer::scene(newLvl);
    cr::utils::replaceScene(scene);
}

void SwapManager::registerListeners() {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();

    nm.setDisconnectCallback([](std::string reason) {});

    nm.on<TimeToSwapPacket>([this](TimeToSwapPacket p) {
        auto& nm = NetworkManager::get();

        Notification::create("Sending your level...", NotificationIcon::Info, 2.5f)->show();

        if (auto editorLayer = LevelEditorLayer::get()) {
            auto fakePauseLayer = EditorPauseLayer::create(editorLayer);
            fakePauseLayer->saveLevel();
        }
        
        auto lvl = EditorIDs::getLevelByID(levelId);
        if (!lvl) {
            log::error("Level not found with ID: {}", levelId);
            return;
        }

        auto b = new DS_Dictionary();
        lvl->encodeWithCoder(b);

        LevelData lvlData = {
            .levelName = lvl->m_levelName,
            .songID = lvl->m_songID,
            .songIDs = lvl->m_songIDs,
            .levelString = b->saveRootSubDictToString()
        };

        #ifdef CR_DEBUG
        log::info("Sending level data. Size: {} bytes, name: {}", lvlData.levelString.length(), lvlData.levelName);
        #endif

        nm.send(SendLevelPacket::create(std::move(lvlData)));

        if (lvl && Mod::get()->getSettingValue<bool>("delete-lvls")) {
            GameLevelManager::sharedState()->deleteLevel(lvl);
        }
    });
    nm.on<ReceiveSwappedLevelPacket>([this](ReceiveSwappedLevelPacket packet) {
        LevelData lvlData;
        auto myAccountID = cr::utils::createAccountType().accountID;
        bool foundLevel = false;

        for (auto& swappedLevel : packet.levels) {
            if (swappedLevel.accountID == myAccountID) {
                lvlData = std::move(swappedLevel.level);
                foundLevel = true;
                break;
            }
        }

        if (!foundLevel) {
            log::error("No level found for current user in swap packet!");
            Notification::create("Error: Could not receive your level!", NotificationIcon::Error, 3.f)->show();
            return;
        }

        auto lvl = GJGameLevel::create();
        auto b = new DS_Dictionary();
        b->loadRootSubDictFromString(lvlData.levelString);
        lvl->dataLoaded(b);
        lvl->m_levelType = GJLevelType::Editor;

        levelId = EditorIDs::getID(lvl);
        LocalLevelManager::get()->m_localLevels->insertObject(lvl, 0);

        Notification::create("Level received! Opening editor...", NotificationIcon::Success, 2.5f)->show();

        #ifdef GEODE_IS_MACOS
        try {
        #endif
        auto scene = EditLevelLayer::scene(lvl);
        cr::utils::replaceScene(scene);
        #ifdef GEODE_IS_MACOS
        } catch (std::exception e) {}
        #endif

        roundStartedTime = time(nullptr);
    });
    nm.on<SwapEndedPacket>([this](SwapEndedPacket p) {
        log::debug("swap ended; disconnecting from server");
        auto& nm = NetworkManager::get();
        nm.showDisconnectPopup = false;
        this->disconnectLobby();
        FLAlertLayer::create(
            "Creation Rotation",
            "The Creation Rotation level swap has <cy>ended!</c>\nHope you had fun! :D\n\n<cl>You have been disconnected from the Creation Rotation server.</c>",
            "OK"
        )->show();
    });
}

int SwapManager::getTimeRemaining() {
    if (secondsPerRound <= 0) {
        return 0;
    }

    return (roundStartedTime + secondsPerRound) - time(nullptr);
}
