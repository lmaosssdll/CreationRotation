#include "Lobby.hpp"
#include <managers/SwapManager.hpp>
#include <network/manager.hpp>
#include <network/packets/server.hpp>
#include <network/packets/client.hpp>
#include "LobbySettings.hpp"
#include "ChatPanel.hpp"
#include <cvolton.level-id-api/include/EditorIDs.hpp>

PlayerCell* PlayerCell::create(Account account, float width, bool canKick) {
    auto ret = new PlayerCell;
    if (ret->init(account, width, canKick)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool PlayerCell::init(Account account, float width, bool canKick) {
    m_account = account;

    this->setContentSize({ width, CELL_HEIGHT });

    auto player = SimplePlayer::create(0);
    auto gm = GameManager::get();

    player->updatePlayerFrame(account.iconID, IconType::Cube);
    player->setColor(gm->colorForIdx(account.color1));
    player->setSecondColor(gm->colorForIdx(account.color2));

    if (account.color3 == -1) {
        player->disableGlowOutline();
    } else {
        player->setGlowOutline(gm->colorForIdx(account.color3));
    }

    player->setScale(0.65f);
    player->setPosition({ 25.f, CELL_HEIGHT / 2.f });
    player->setAnchorPoint({ 0.5f, 0.5f });

    this->addChild(player);

    auto nameLabel = CCLabelBMFont::create(account.name.c_str(), "bigFont.fnt");
    nameLabel->limitLabelWidth(225.f, 0.8f, 0.1f);
    nameLabel->setAnchorPoint({ 0.f, 0.5f });

    auto nameBtn = CCMenuItemExt::createSpriteExtra(
        nameLabel,
        [this](CCObject*) {
            ProfilePage::create(m_account.accountID, false)->show();
        }
    );
    nameBtn->setAnchorPoint({ 0.f, 0.5f });
    nameBtn->setPosition({ 45.f, CELL_HEIGHT / 2.f });

    auto cellMenu = CCMenu::create();
    cellMenu->setPosition({ 0.f, 0.f });
    cellMenu->setContentSize({ width, CELL_HEIGHT });
    cellMenu->addChild(nameBtn);

    if (canKick && account.userID != GameManager::get()->m_playerUserID.value()) {
        auto kickSpr = CCSprite::createWithSpriteFrameName("accountBtn_removeFriend_001.png");
        kickSpr->setScale(0.725f);
        auto kickBtn = CCMenuItemSpriteExtra::create(
            kickSpr, this, menu_selector(PlayerCell::onKickUser)
        );
        kickBtn->setPosition({ width - 25.f, CELL_HEIGHT / 2.f });
        cellMenu->addChild(kickBtn);
    }

    this->addChild(cellMenu);

    return true;
}

void PlayerCell::onKickUser(CCObject* sender) {
    geode::createQuickPopup(
        "Kick User",
        fmt::format("Would you like to kick user <cy>\"{}\"</c>? Doing so will <cr>not allow them</c> to join back.", m_account.name),
        "Close", "Kick",
        [this](auto, bool btn2) {
            if (!btn2) return;
            auto& nm = NetworkManager::get();
            nm.send(KickUserPacket::create(m_account.userID));
        }
    );
}

LobbyLayer* LobbyLayer::create(std::string code) {
    auto ret = new LobbyLayer();
    if (ret->init(code)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LobbyLayer::init(std::string code) {
    lobbyCode = code;
    m_alive = std::make_shared<bool>(true);

    ChatPanel::initialize();

    auto director = CCDirector::sharedDirector();
    auto size = director->getWinSize();

    mainLayer = CCNode::create();
    mainLayer->setContentSize(size);

    background = CCSprite::create("GJ_gradientBG.png");
    background->setScaleX(size.width / background->getContentWidth());
    background->setScaleY(size.height / background->getContentHeight());
    background->setAnchorPoint({ 0, 0 });
    background->setColor({ 0, 102, 255 });
    background->setZOrder(-10);
    mainLayer->addChild(background);

    auto closeBtnSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    closeBtn = CCMenuItemExt::createSpriteExtra(
        closeBtnSprite, [this](CCObject* target) {
            keyBackClicked();
        }
    );

    auto disconnectBtnSprite = CCSprite::create("disconnect-btn.png"_spr);
    disconnectBtnSprite->setScale(0.2f);
    auto disconnectBtn = CCMenuItemExt::createSpriteExtra(
        disconnectBtnSprite,
        [this](CCObject* target) {
            onDisconnect(target);
        }
    );
    auto closeMenu = CCMenu::create();
    closeMenu->addChild(closeBtn);
    closeMenu->addChild(disconnectBtn);
    closeMenu->setLayout(RowLayout::create());
    closeMenu->setPosition({ 45, size.height - 25 });
    mainLayer->addChild(closeMenu);

    geode::addSideArt(mainLayer, SideArt::Bottom);
    geode::addSideArt(mainLayer, SideArt::TopRight);

    auto startBtnSpr = CCSprite::create("swap-btn.png"_spr);
    startBtnSpr->setScale(0.3f);
    startBtn = CCMenuItemSpriteExtra::create(
        startBtnSpr, this, menu_selector(LobbyLayer::onStart)
    );
    auto startMenu = CCMenu::create();
    startMenu->setZOrder(5);
    startMenu->addChild(startBtn);
    startMenu->setPosition({ size.width / 2, 30.f });

    mainLayer->addChild(startMenu);

    auto msgButtonSpr = CCSprite::create("messagesBtn.png"_spr);
    auto msgButton = CCMenuItemExt::createSpriteExtra(
        msgButtonSpr, [](CCObject*) { ChatPanel::create()->show(); }
    );

    auto settingsBtnSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
    settingsBtn = CCMenuItemSpriteExtra::create(
        settingsBtnSpr, this, menu_selector(LobbyLayer::onSettings)
    );

    auto refreshBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
        "GJ_getSongInfoBtn_001.png", 1.f,
        [this, alive = m_alive](CCObject* sender) {
            SwapManager::get().getLobbyInfo([this, alive](LobbyInfo info) {
                if (!*alive) return;
                refresh(info);
            });
        }
    );

    cr::utils::scaleToMatch(settingsBtnSpr, msgButtonSpr, true);

    auto bottomMenu = CCMenu::create();
    bottomMenu->addChild(msgButton);
    bottomMenu->addChild(settingsBtn);
    bottomMenu->addChild(refreshBtn);
    bottomMenu->setPosition(size.width - 25.f, bottomMenu->getChildrenCount() * 25.f);
    bottomMenu->setLayout(ColumnLayout::create()->setAxisReverse(true));

    auto discordSpr = CCSprite::createWithSpriteFrameName("gj_discordIcon_001.png");
    discordSpr->setScale(1.25f);
    auto discordBtn = CCMenuItemExt::createSpriteExtra(
        discordSpr,
        [](CCObject* target) {
            geode::createQuickPopup(
                "Discord Server",
                "We have a <cb>Discord Server</c>! If you have questions, want to suggest a feature, or find someone to swap with, join here!",
                "Cancel", "Join!",
                [](auto, bool btn2) {
                    if (!btn2) return;
                    CCApplication::get()->openURL(Mod::get()->getMetadata().getLinks().getCommunityURL()->c_str());
                }
            );
        }
    );
    auto discordMenu = CCMenu::create();
    discordMenu->setPosition({ 25.f, 25.f });
    discordMenu->addChild(discordBtn);
    mainLayer->addChild(discordMenu);

    this->setKeyboardEnabled(true);
    this->setKeypadEnabled(true);
    this->addChild(mainLayer);
    this->addChild(bottomMenu);

    SwapManager::get().getLobbyInfo([this, alive = m_alive](LobbyInfo info) {
        if (!*alive) return;
        refresh(info, true);
    });
    registerListeners();

    return true;
}

void LobbyLayer::registerListeners() {
    auto& nm = NetworkManager::get();
    nm.on<LobbyUpdatedPacket>([this, alive = m_alive](LobbyUpdatedPacket packet) {
        if (!*alive) return;
        this->refresh(std::move(packet.info));
    });
    nm.on<SwapStartedPacket>([](SwapStartedPacket packet) {
        auto& sm = SwapManager::get();
        sm.startSwap(std::move(packet));
        NetworkManager::get().unbind<SwapStartedPacket>();
    });
    nm.showDisconnectPopup = true;
    nm.setDisconnectCallback([this, alive = m_alive](std::string reason) {
        if (!*alive) return;
        unregisterListeners();
        auto& nm = NetworkManager::get();
        nm.isConnected = false;
        nm.showDisconnectPopup = false;
        cr::utils::popScene();
    });
}

void LobbyLayer::unregisterListeners() {
    auto& nm = NetworkManager::get();
    nm.unbind<LobbyUpdatedPacket>();
    nm.unbind<SwapStartedPacket>();
    nm.setDisconnectCallback(nullptr);
}

LobbyLayer::~LobbyLayer() {
    if (m_alive) {
        *m_alive = false;
    }
    unregisterListeners();
}

void LobbyLayer::refresh(LobbyInfo info, bool isFirstRefresh) {
    isOwner = GameManager::get()->m_playerUserID == info.settings.owner.userID;
    auto size = CCDirector::sharedDirector()->getWinSize();
    auto listWidth = size.width / 1.5f;

    if (!mainLayer) return;

    // --- Логика Уведомлений Входа/Выхода ---
    if (isFirstRefresh) {
        m_currentPlayers.clear();
        for (auto& acc : info.accounts) m_currentPlayers.push_back(acc.userID);
    } else {
        std::vector<int> newPlayers;
        for (auto& acc : info.accounts) newPlayers.push_back(acc.userID);

        for (auto& acc : info.accounts) {
            if (std::find(m_currentPlayers.begin(), m_currentPlayers.end(), acc.userID) == m_currentPlayers.end()) {
                Notification::create(fmt::format("<cg>{} joined</c>", acc.name), CCSprite::createWithSpriteFrameName("GJ_sStarsIcon_001.png"))->show();
            }
        }
        for (int id : m_currentPlayers) {
            if (std::find(newPlayers.begin(), newPlayers.end(), id) == newPlayers.end()) {
                Notification::create("<cr>Someone left</c>", CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png"))->show();
            }
        }
        m_currentPlayers = newPlayers;
    }

    // --- Обновление визуальных настроек лобби (СЛЕВА СВЕРХУ) ---
    // Удаляем старый UI настроек, если он есть
    if (auto oldSettingsUI = mainLayer->getChildByID("lobby-settings-display")) {
        oldSettingsUI->removeFromParent();
    }

    // Создаем новый контейнер для настроек
    auto settingsDisplay = CCNode::create();
    settingsDisplay->setID("lobby-settings-display");

    // Текст: Ходы и время
    auto statsLabel = CCLabelBMFont::create(
        fmt::format("Turns: {} | Mins/Turn: {}", info.settings.turns, info.settings.minutesPerTurn).c_str(),
        "chatFont.fnt"
    );
    statsLabel->setAnchorPoint({ 0.f, 1.f });

    // Текст: Выбранный тег с цветом
    std::string tagNames[] = {"None", "Short", "Medium", "Long", "Layout", "Deco", "Impossible", "Triggers"};
    ccColor3B tagColors[] = {
        {255, 255, 255}, // 0 None (Белый)
        {100, 255, 100}, // 1 Short (Светло-зеленый)
        {255, 255, 100}, // 2 Medium (Желтый)
        {255, 150, 50},  // 3 Long (Оранжевый)
        {100, 255, 255}, // 4 Layout (Бирюзовый)
        {255, 100, 255}, // 5 Deco (Розовый/Пурпурный)
        {255, 50, 50},   // 6 Impossible (Красный)
        {200, 100, 255}  // 7 Triggers (Фиолетовый)
    };
    
    int tagId = info.settings.tag;
    if (tagId < 0 || tagId >= 8) tagId = 0;

    auto tagPrefix = CCLabelBMFont::create("Tag: ", "chatFont.fnt");
    auto tagValue = CCLabelBMFont::create(tagNames[tagId].c_str(), "chatFont.fnt");
    tagValue->setColor(tagColors[tagId]);

    auto tagContainer = CCMenu::create(); // Используем меню просто как Layout контейнер
    tagContainer->addChild(tagPrefix);
    tagContainer->addChild(tagValue);
    tagContainer->setLayout(RowLayout::create()->setGap(2.f)->setAxisAlignment(AxisAlignment::Start));
    tagContainer->setAnchorPoint({ 0.f, 1.f });
    tagContainer->setContentSize({ 200.f, tagPrefix->getContentHeight() });

    settingsDisplay->addChild(statsLabel);
    settingsDisplay->addChild(tagContainer);
    settingsDisplay->setLayout(ColumnLayout::create()->setGap(5.f)->setAxisReverse(true)->setAxisAlignment(AxisAlignment::Start));
    settingsDisplay->setContentSize({ 200.f, 50.f });
    settingsDisplay->setPosition({ 15.f, size.height - 15.f });
    settingsDisplay->setAnchorPoint({ 0.f, 1.f });

    mainLayer->addChild(settingsDisplay);
    // -----------------------------------------------------------

    // Заголовок (только при первом заходе)
    if (isFirstRefresh) {
        titleLabel = CCLabelBMFont::create(info.settings.name.c_str(), "bigFont.fnt");
        titleLabel->limitLabelWidth(275.f, 1.f, 0.1f);

        auto titleBtn = CCMenuItemExt::createSpriteExtra(
            titleLabel,
            [info](CCObject* sender) {
                geode::utils::clipboard::write(info.code);
                Notification::create("Copied code to clipboard")->show();
            }
        );

        auto menu = CCMenu::create();
        menu->setPosition({ size.width / 2, size.height - 25 });
        menu->addChild(titleBtn);
        mainLayer->addChild(menu);
    }

    if (titleLabel) titleLabel->setString(
        fmt::format("{} ({})", info.settings.name, Mod::get()->getSettingValue<bool>("hide-code") ? "......" : info.code).c_str()
    );

    if (!playerList && !isFirstRefresh) return;
    if (playerList) playerList->removeFromParent();

    playerListItems = CCArray::create();
    for (auto& acc : info.accounts) {  
        auto cell = PlayerCell::create(acc, listWidth, isOwner);
        if (cell) playerListItems->addObject(cell);  
    }

    if (playerListItems->count() == 0) {
        playerList = nullptr;
        return;
    }

    playerList = ListView::create(playerListItems, PlayerCell::CELL_HEIGHT, listWidth);
    if (!playerList) return;  

    playerList->setPosition({ size.width / 2, size.height / 2 - 10.f });
    playerList->setAnchorPoint({ 0.5f, 0.5f });
    playerList->ignoreAnchorPointForPosition(false);

    createBorders();

    mainLayer->addChild(playerList);

    auto listBG = static_cast<CCLayerColor*>(mainLayer->getChildByIDRecursive("list-bg"));
    if (!listBG) {
        listBG = CCLayerColor::create({ 0, 0, 0, 85 });
        listBG->ignoreAnchorPointForPosition(false);
        listBG->setAnchorPoint({ 0.5f, 0.5f });
        listBG->setZOrder(-1);
        listBG->setID("list-bg");
        mainLayer->addChild(listBG);
    }
    listBG->setPosition(playerList->getPosition());
    listBG->setContentSize(playerList->getContentSize());

    if (settingsBtn) settingsBtn->setVisible(isOwner);
    if (startBtn) startBtn->setVisible(isOwner);
}

void LobbyLayer::onStart(CCObject* sender) {
    if (!isOwner) return;

    geode::createQuickPopup(
        "Start Swap?",
        "Are you sure you want to start the Creation Rotation <cy>now?</c>",
        "No", "Let's go!",
        [this](auto, bool btn2) {
            if (!btn2) return;
            auto& nm = NetworkManager::get();
            nm.send(StartSwapPacket::create(lobbyCode));
        }
    );
}

void LobbyLayer::onSettings(CCObject* sender) {
    auto& lm = SwapManager::get();
    lm.getLobbyInfo([this, alive = m_alive](LobbyInfo info) {
        if (!*alive) return;
        auto settingsPopup = LobbySettingsPopup::create(info.settings, [this, alive](LobbySettings settings) {
            if (!*alive) return;
            auto& lm = SwapManager::get();
            lm.updateLobby(settings);
        });
        settingsPopup->show();
    });
}

void LobbyLayer::createBorders() {
    #define CREATE_SIDE() CCSprite::createWithSpriteFrameName("GJ_table_side_001.png")

    const int SIDE_OFFSET = 7;
    const int TOP_BOTTOM_OFFSET = 8;

    auto topSide = CREATE_SIDE();
    topSide->setScaleY(playerList->getContentWidth() / topSide->getContentHeight());
    topSide->setRotation(90.f);
    topSide->setPosition({ playerList->m_width / 2, playerList->m_height + TOP_BOTTOM_OFFSET });
    topSide->setID("top-border");
    topSide->setZOrder(3);

    auto bottomSide = CREATE_SIDE();
    bottomSide->setScaleY(playerList->getContentWidth() / bottomSide->getContentHeight());
    bottomSide->setRotation(-90.f);
    bottomSide->setPosition({ playerList->m_width / 2, 0.f - TOP_BOTTOM_OFFSET });
    bottomSide->setID("bottom-border");
    bottomSide->setZOrder(3);

    auto leftSide = CREATE_SIDE();
    leftSide->setScaleY((playerList->getContentHeight() + TOP_BOTTOM_OFFSET) / leftSide->getContentHeight());
    leftSide->setPosition({ -SIDE_OFFSET, playerList->m_height / 2 });
    leftSide->setID("left-border");

    auto rightSide = CREATE_SIDE();
    rightSide->setScaleY((playerList->getContentHeight() + TOP_BOTTOM_OFFSET) / rightSide->getContentHeight());
    rightSide->setRotation(180.f);
    rightSide->setPosition({ playerList->m_width + SIDE_OFFSET, playerList->m_height / 2 });
    rightSide->setID("right-border");

    playerList->addChild(topSide);
    playerList->addChild(bottomSide);
    playerList->addChild(leftSide);
    playerList->addChild(rightSide);

    #define CREATE_CORNER() CCSprite::createWithSpriteFrameName("GJ_table_corner_001.png")

    auto topLeftCorner = CREATE_CORNER();
    topLeftCorner->setPosition({ leftSide->getPositionX(), topSide->getPositionY() });
    topLeftCorner->setZOrder(2);
    topLeftCorner->setID("top-left-corner");

    auto topRightCorner = CREATE_CORNER();
    topRightCorner->setFlipX(true);
    topRightCorner->setPosition({ rightSide->getPositionX(), topSide->getPositionY() });
    topRightCorner->setZOrder(2);
    topRightCorner->setID("top-right-corner");

    auto bottomLeftCorner = CREATE_CORNER();
    bottomLeftCorner->setFlipY(true);
    bottomLeftCorner->setPosition({ leftSide->getPositionX(), bottomSide->getPositionY() });
    bottomLeftCorner->setZOrder(2);
    bottomLeftCorner->setID("bottom-left-corner");

    auto bottomRightCorner = CREATE_CORNER();
    bottomRightCorner->setFlipX(true);
    bottomRightCorner->setFlipY(true);
    bottomRightCorner->setPosition({ rightSide->getPositionX(), bottomSide->getPositionY() });
    bottomRightCorner->setZOrder(2);
    bottomRightCorner->setID("bottom-right-corner");

    playerList->addChild(topLeftCorner);
    playerList->addChild(topRightCorner);
    playerList->addChild(bottomLeftCorner);
    playerList->addChild(bottomRightCorner);
}

void LobbyLayer::onDisconnect(CCObject* sender) {
    geode::createQuickPopup(
        "Disconnect",
        "Are you sure you want to <cr>disconnect</c> from the <cy>lobby</c> and <cy>server</c>?",
        "Cancel", "Yes",
        [](auto, bool btn2) {
            if (!btn2) return;
            auto& nm = NetworkManager::get();
            nm.showDisconnectPopup = false;
            auto& lm = SwapManager::get();
            lm.disconnectLobby();
            cr::utils::popScene();
        }
    );
}

void LobbyLayer::keyBackClicked() {
    geode::createQuickPopup(
        "Leave Layer?",
        "Are you sure you want to <cy>leave the layer?</c> This will not disconnect you and will allow you to do other things ingame. You will be booted to the level page when the swap begins.",
        "No", "Yes",
        [](auto, bool btn2) {
            if (!btn2) return;
            cr::utils::popScene();
        }
    );
}
