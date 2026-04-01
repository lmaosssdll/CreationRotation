#pragma once

#include <Geode/Geode.hpp>
#include <types/lobby.hpp>
#include <defs.hpp>
#include <memory>

using namespace geode::prelude;

class CR_DLL PlayerCell : public CCLayer {
protected:
    Account m_account{};  

    bool init(Account account, float width, bool canKick);
    void onKickUser(CCObject*);
public:
    static constexpr int CELL_HEIGHT = 50.f;
    static PlayerCell* create(Account account, float width, bool canKick);
};

class CR_DLL LobbyLayer : public CCLayer {
protected:
    std::string lobbyCode;
    bool isOwner = false;
    std::string lobbyNspace;
    std::shared_ptr<bool> m_alive;
    std::vector<int> m_currentPlayers;

    CCMenuItemSpriteExtra* closeBtn = nullptr;         
    CCSprite* background = nullptr;                    
    CCArray* playerListItems = nullptr;                
    CustomListView* playerList = nullptr;             
    CCLabelBMFont* titleLabel = nullptr;               
    CCMenuItemSpriteExtra* settingsBtn = nullptr;     
    CCMenuItemSpriteExtra* startBtn = nullptr;         
    LoadingCircle* loadingCircle = nullptr;            
    CCNode* mainLayer = nullptr;                     

    bool init(std::string code);
    void keyBackClicked();
    void createBorders();
    void refresh(LobbyInfo info, bool isFirstRefresh = false);
    void registerListeners();
    void unregisterListeners();
    void onDisconnect(CCObject*);
    void onStart(CCObject*);
    void onSettings(CCObject*);

    virtual ~LobbyLayer();
public:
    static LobbyLayer* create(std::string code);
};
