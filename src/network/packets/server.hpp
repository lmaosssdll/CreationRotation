#pragma once
#include "packet.hpp"

#include <types/lobby.hpp>
#include <utils.hpp>

using namespace cr::utils;

class LobbyCreatedPacket : public Packet {
    CR_PACKET(1001, LobbyCreatedPacket)

    LobbyCreatedPacket(LobbyInfo info):
        info(info) {}

    LobbyInfo info;

    CR_SERIALIZE(
        CEREAL_NVP(info)
    )
};

class ReceiveAccountsPacket : public Packet {
    CR_PACKET(1002, ReceiveAccountsPacket)

    ReceiveAccountsPacket(std::vector<Account> accounts):
        accounts(accounts) {}

    std::vector<Account> accounts;

    CR_SERIALIZE(
        CEREAL_NVP(accounts)
    )
};

class ReceiveLobbyInfoPacket : public Packet {
    CR_PACKET(1003, ReceiveLobbyInfoPacket)

    ReceiveLobbyInfoPacket(LobbyInfo info):
        info(info) {}

    LobbyInfo info;

    CR_SERIALIZE(
        CEREAL_NVP(info)
    )
};

class LobbyUpdatedPacket : public Packet {
    CR_PACKET(1004, LobbyUpdatedPacket)

    LobbyUpdatedPacket(LobbyInfo info):
        info(info) {}

    LobbyInfo info;

    CR_SERIALIZE(
        CEREAL_NVP(info)
    )
};

class JoinedLobbyPacket : public Packet {
    CR_PACKET(1007, JoinedLobbyPacket)

    std::string dummy;

    CR_SERIALIZE(dummy)
};

class ReceivePublicLobbiesPacket : public Packet {
    CR_PACKET(1008, ReceivePublicLobbiesPacket)

    ReceivePublicLobbiesPacket(std::vector<LobbyInfo> lobbies) :
        lobbies(lobbies) {}

    std::vector<LobbyInfo> lobbies;

    CR_SERIALIZE(
        CEREAL_NVP(lobbies)
    )
};

class MessageSentPacket : public Packet {
    CR_PACKET(1009, MessageSentPacket)

    MessageSentPacket(Message message) :
        message(message) {}

    Message message;

    CR_SERIALIZE(
        CEREAL_NVP(message)
    )
};
    
// LEVEL SWAP //

class SwapStartedPacket : public Packet {
    CR_PACKET(1005, SwapStartedPacket)

    std::string dummy;

    CR_SERIALIZE(
        dummy
    )
};

class TimeToSwapPacket : public Packet {
    CR_PACKET(1006, TimeToSwapPacket)

    std::string dummy;

    CR_SERIALIZE(dummy)
};

struct SwappedLevel {
    LevelData level;
    int accountID;

    CR_SERIALIZE(
        CEREAL_NVP(level),
        CEREAL_NVP(accountID)
    )
};

class ReceiveSwappedLevelPacket : public Packet {
    CR_PACKET(3002, ReceiveSwappedLevelPacket)

    std::vector<SwappedLevel> levels;

    ReceiveSwappedLevelPacket() = default;

    void decode(std::string_view in) override {
        levels.clear();
        
        auto jsonResult = matjson::parse(in);
        if (!jsonResult.isOk()) {
            log::error("Failed to parse ReceiveSwappedLevelPacket: {}", jsonResult.unwrapErr());
            return;
        }
        
        auto packetObj = jsonResult.unwrap();
        
        matjson::Value packetData;
        if (packetObj.isObject()) {
            if (auto pkt = packetObj.asObject()->get("packet")) {
                packetData = pkt.value();
            } else {
                packetData = packetObj;
            }
        } else {
            return;
        }

        if (!packetData.isObject()) return;

        auto levelsObj = packetData.asObject()->get("levels");
        if (!levelsObj || !levelsObj->isObject()) return;

        for (auto& [key, value] : levelsObj->asObject().unwrap()) {
            if (!value.isObject()) continue;
            
            SwappedLevel sl;
            sl.accountID = geode::utils::numFromString<int>(key).unwrapOr(0);
            
            auto lvlObj = value.asObject().unwrap();
            
            if (auto lvlData = lvlObj.get("level")) {
                if (lvlData->isObject()) {
                    auto& ld = lvlData->asObject().unwrap();
                    if (auto n = ld.get("levelName")) sl.level.levelName = n->asString().unwrapOr("");
                    if (auto s = ld.get("songID")) sl.level.songID = s->asInt().unwrapOr(0);
                    if (auto ss = ld.get("songIDs")) sl.level.songIDs = ss->asString().unwrapOr("");
                    if (auto str = ld.get("levelString")) sl.level.levelString = str->asString().unwrapOr("");
                }
            }
            
            if (!sl.level.levelString.empty()) {
                levels.push_back(sl);
            }
        }
        
        log::info("Parsed {} swapped levels from server", levels.size());
    }

    CR_SERIALIZE(
        CEREAL_NVP(levels)
    )
};

class SwapEndedPacket : public Packet {
    CR_PACKET(3003, SwapEndedPacket)

    std::string dummy;

    CR_SERIALIZE(dummy)
};

class MusicSelectionStartPacket : public Packet {
    CR_PACKET(3010, MusicSelectionStartPacket)

    int countdown = 15;
    std::vector<std::string> players;
    int turnsLeft = 0;
    int previousTurnsLeft = 0;

    CR_SERIALIZE(
        CEREAL_NVP(countdown),
        CEREAL_NVP(players),
        CEREAL_NVP(turnsLeft),
        CEREAL_NVP(previousTurnsLeft)
    )
};

class TimeToSwapMusicPacket : public Packet {
    CR_PACKET(3011, TimeToSwapMusicPacket)

    std::string playerName;
    int iconID = 0;
    int color1 = 0;
    int color2 = 0;

    CR_SERIALIZE(
        CEREAL_NVP(playerName),
        CEREAL_NVP(iconID),
        CEREAL_NVP(color1),
        CEREAL_NVP(color2)
    )
};

// OTHER STUFF //

class ErrorPacket : public Packet {
    CR_PACKET(4001, ErrorPacket)

    ErrorPacket(std::string error):
        error(error) {}
    
    std::string error;

    CR_SERIALIZE(
        CEREAL_NVP(error)
    )
};

class PongPacket : public Packet {
    CR_PACKET(4009, PongPacket)

    std::string dummy;

    CR_SERIALIZE(dummy);
};

// AUTH

class BannedUserPacket : public Packet {
    CR_PACKET(4002, BannedUserPacket)

    std::string dummy;

    CR_SERIALIZE(dummy)
};

class AuthorizedUserPacket : public Packet {
    CR_PACKET(4003, AuthorizedUserPacket)

    std::string dummy;

    CR_SERIALIZE(dummy)
};

class ReceiveAuthCodePacket : public Packet {
    CR_PACKET(4004, ReceiveAuthCodePacket)

    ReceiveAuthCodePacket(std::string code, int botAccID):
        code(code), botAccID(botAccID) {}

    std::string code;
    int botAccID;

    CR_SERIALIZE(
        CEREAL_NVP(code),
        CEREAL_NVP(botAccID)
    )
};

class ReceiveTokenPacket : public Packet {
    CR_PACKET(4005, ReceiveTokenPacket)

    ReceiveTokenPacket(std::string token):
        token(token) {}

    std::string token;

    CR_SERIALIZE(
        CEREAL_NVP(token)
    )
};

class InvalidTokenPacket : public Packet {
    CR_PACKET(4006, InvalidTokenPacket)

    std::string dummy;

    CR_SERIALIZE(
        dummy
    )
};

class LoginNotReceivedPacket : public Packet {
    CR_PACKET(4007, LoginNotReceivedPacket)

    std::string dummy;

    CR_SERIALIZE(
        dummy
    )
};

class LoggedInPacket : public Packet {
    CR_PACKET(4008, LoggedInPacket)

    std::string dummy;

    CR_SERIALIZE(
        dummy
    )
};
