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

void SwapManager::getLobbyAccounts(std::function<void(std::vector<Account>)> callback) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    nm.send(GetAccountsPacket::create(this->currentLobbyCode));
    nm.on<ReceiveAccountsPacket>([callback = std::move(callback)](ReceiveAccountsPacket accounts) {
        callback(std::move(accounts.accounts));
    });
}

void SwapManager::getLobbyInfo(std::function<void(LobbyInfo)> callback) {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();
    nm.send(GetLobbyInfoPacket::create(this->currentLobbyCode));
    nm.on<ReceiveLobbyInfoPacket>([callback = std::move(callback)](ReceiveLobbyInfoPacket packet) {
        callback(std::move(packet.info));
    });
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
    });

    auto glm = GameLevelManager::sharedState();
    auto newLvl = glm->createNewLevel();

    levelId = EditorIDs::getID(newLvl);

    registerListeners();

    roundStartedTime = time(nullptr);

    auto scene = EditLevelLayer::scene(newLvl);
    cr::utils::replaceScene(scene);
}

void SwapManager::registerListeners() {
    CR_REQUIRE_CONNECTION()

    auto& nm = NetworkManager::get();

    nm.setDisconnectCallback([](std::string reason) {});

    // --- SENDING THE LEVEL TO THE SERVER ---
    nm.on<TimeToSwapPacket>([this](TimeToSwapPacket p) {
        auto& nm = NetworkManager::get();

        Notification::create("Swapping levels!", NotificationIcon::Info, 2.5f)->show();

        // 1. Force save the level
        if (auto editorLayer = LevelEditorLayer::get()) {
            // Update internal level string before saving to prevent race condition
            editorLayer->updateLevelString(); 
            auto fakePauseLayer = EditorPauseLayer::create(editorLayer);
            fakePauseLayer->saveLevel();
        }
        
        auto lvl = EditorIDs::getLevelByID(levelId);
        if (!lvl) {
            log::error("[SwapManager] CRITICAL ERROR: Could not find level by ID {} before swapping!", levelId);
            return;
        }

        // 2. Generate level string
        auto b = new DS_Dictionary();
        lvl->encodeWithCoder(b);
        std::string finalLevelString = b->saveRootSubDictToString();
        
        // 3. FIX MEMORY LEAK: We must delete 'b' after using it!
        delete b; 

        // 4. PREVENT EMPTY LEVEL BUG
        if (finalLevelString.empty() || finalLevelString.size() < 10) {
            log::error("[SwapManager] CRITICAL ERROR: b->saveRootSubDictToString() generated an EMPTY string!");
            log::error("[SwapManager] The game engine did not save the level fast enough (Race Condition).");
            
            // Fallback: Try to use the cached m_levelString directly
            if (!lvl->m_levelString.empty()) {
                finalLevelString = lvl->m_levelString;
                log::warn("[SwapManager] Used fallback m_levelString (Size: {} bytes).", finalLevelString.size());
            } else {
                log::error("[SwapManager] Fallback failed! m_levelString is also empty. Sending void level...");
            }
        } else {
#ifdef CR_DEBUG
            log::debug("[SwapManager] Successfully generated level string. Size: {} bytes.", finalLevelString.size());
#endif
        }

        LevelData lvlData = {
            .levelName = lvl->m_levelName,
            .songID = lvl->m_songID,
            .songIDs = lvl->m_songIDs,
            .levelString = finalLevelString
        };

        nm.send(
            SendLevelPacket::create(std::move(lvlData))
        );

        if (lvl && Mod::get()->getSettingValue<bool>("delete-lvls")) {
            // let's not spam everyone's created levels list
            GameLevelManager::sharedState()->deleteLevel(lvl);
        }
    });

    // --- RECEIVING THE LEVEL FROM THE SERVER ---
    nm.on<ReceiveSwappedLevelPacket>([this](ReceiveSwappedLevelPacket packet) {
        LevelData lvlData;
        bool foundMyLevel = false;

        for (auto& swappedLevel : packet.levels) {
            if (swappedLevel.accountID != cr::utils::createAccountType().accountID) continue;

            lvlData = std::move(swappedLevel.level);
            foundMyLevel = true;
            break;
        }

        // 1. PREVENT EMPTY LEVEL BUG ON RECEIVE
        if (!foundMyLevel) {
            log::error("[SwapManager] CRITICAL ERROR: Server did not send a level for my Account ID!");
        } else if (lvlData.levelString.empty()) {
            log::error("[SwapManager] CRITICAL ERROR: The server sent a level, but the levelString is EMPTY!");
            log::error("[SwapManager] This means the previous player failed to upload it, or the server wiped it.");
        } else {
#ifdef CR_DEBUG
            log::debug("[SwapManager] Successfully received swapped level! String size: {} bytes.", lvlData.levelString.size());
#endif
        }

        auto lvl = GJGameLevel::create();

        // 2. Load the level data
        auto b = new DS_Dictionary();
        b->loadRootSubDictFromString(lvlData.levelString);
        lvl->dataLoaded(b);
        
        // 3. FIX MEMORY LEAK: We must delete 'b' after loading data!
        delete b;

        lvl->m_levelType = GJLevelType::Editor;

        levelId = EditorIDs::getID(lvl);
        LocalLevelManager::get()->m_localLevels->insertObject(lvl, 0);

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
        log::debug("[SwapManager] swap ended; disconnecting from server");
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
