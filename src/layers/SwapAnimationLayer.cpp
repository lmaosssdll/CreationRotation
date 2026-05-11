#include "SwapAnimationLayer.hpp"
#include <Geode/Geode.hpp>
#include <managers/SwapManager.hpp>
#include <network/manager.hpp>
#include <network/packets/server.hpp>

using namespace geode::prelude;

bool SwapAnimationLayer::init(
    std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
    std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
    int turnsLeft
) {
    if (!CCLayer::init()) return false;

    this->player1Name = p1Name;
    this->player2Name = p2Name;
    this->player1Icon = p1Icon;
    this->player2Icon = p2Icon;
    this->player1Color1 = p1c1;
    this->player1Color2 = p1c2;
    this->player2Color1 = p2c1;
    this->player2Color2 = p2c2;
    this->currentTurnsLeft = turnsLeft;
    this->previousTurnsLeft = turnsLeft;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    fadeLayer = CCLayerColor::create({0, 0, 0, 0});
    fadeLayer->setPosition({0, 0});
    fadeLayer->setContentSize(winSize);
    this->addChild(fadeLayer, 0);

    auto bgPanel = CCLayerColor::create({0, 50, 100, 180});
    bgPanel->setPosition({winSize.width / 2.f - 200.f, winSize.height / 2.f - 150.f});
    bgPanel->setContentSize({400.f, 300.f});
    bgPanel->setCornerRadius(20.f);
    this->addChild(bgPanel, 1, "bgPanel");

    titleLabel = CCLabelBMFont::create("SWAPPING!", "bigFont.fnt");
    titleLabel->setScale(0.0f);
    titleLabel->setPosition({winSize.width / 2.f, winSize.height * 0.85f});
    this->addChild(titleLabel, 10);

    auto createPlayerIcon = [&](SimplePlayer*& sp, int iconID, ccColor3B c1, ccColor3B c2, float scale) {
        sp = SimplePlayer::create(0);
        sp->updatePlayerFrame(iconID, IconType::Cube);
        sp->setColor(c1);
        sp->setSecondColor(c2);
        sp->setScale(scale);
        sp->setZOrder(2);
        sp->setGlowOutline(cocos2d::ccColor4B(c1.r, c1.g, c1.b, 100));
    };

    SimplePlayer* sp1 = nullptr;
    SimplePlayer* sp2 = nullptr;
    createPlayerIcon(sp1, player1Icon, player1Color1, player1Color2, 2.5f);
    createPlayerIcon(sp2, player2Icon, player2Color1, player2Color2, 2.5f);

    player1IconSprite = CCSprite::create();
    player1IconSprite->addChild(sp1);
    sp1->setPosition({sp1->getContentSize().width / 2.f, sp1->getContentSize().height / 2.f});
    player1IconSprite->setContentSize(sp1->getContentSize());
    player1IconSprite->setPosition({winSize.width * 0.25f, winSize.height * 0.55f});
    this->addChild(player1IconSprite, 5);

    player2IconSprite = CCSprite::create();
    player2IconSprite->addChild(sp2);
    sp2->setPosition({sp2->getContentSize().width / 2.f, sp2->getContentSize().height / 2.f});
    player2IconSprite->setContentSize(sp2->getContentSize());
    player2IconSprite->setPosition({winSize.width * 0.75f, winSize.height * 0.55f});
    this->addChild(player2IconSprite, 5);

    auto icon1Folder = CCSprite::createWithSpriteFrameName("folderIcon_002.png");
    if (icon1Folder) {
        icon1Folder->setScale(0.8f);
        icon1Folder->setPosition({player1IconSprite->getPositionX() + 45.f, player1IconSprite->getPositionY() - 45.f});
        icon1Folder->setZOrder(6);
        icon1Folder->setOpacity(0);
        this->addChild(icon1Folder, 5, "folder1");
    }

    auto icon2Folder = CCSprite::createWithSpriteFrameName("folderIcon_002.png");
    if (icon2Folder) {
        icon2Folder->setScale(0.8f);
        icon2Folder->setPosition({player2IconSprite->getPositionX() + 45.f, player2IconSprite->getPositionY() - 45.f});
        icon2Folder->setZOrder(6);
        icon2Folder->setOpacity(0);
        this->addChild(icon2Folder, 5, "folder2");
    }

    player1NameLabel = CCLabelBMFont::create(player1Name.c_str(), "bigFont.fnt");
    player1NameLabel->setScale(0.0f);
    player1NameLabel->setPosition({winSize.width * 0.25f, winSize.height * 0.38f});
    this->addChild(player1NameLabel, 8);

    player2NameLabel = CCLabelBMFont::create(player2Name.c_str(), "bigFont.fnt");
    player2NameLabel->setScale(0.0f);
    player2NameLabel->setPosition({winSize.width * 0.75.f, winSize.height * 0.38f});
    this->addChild(player2NameLabel, 8);

    auto turnBg = CCLayerColor::create({0, 0, 0, 150});
    turnBg->setPosition({winSize.width / 2.f - 80.f, winSize.height * 0.18f});
    turnBg->setContentSize({160.f, 50.f});
    turnBg->setCornerRadius(10.f);
    turnBg->setOpacity(0);
    this->addChild(turnBg, 6, "turnBg");

    turnsLabel = CCLabelBMFont::create(fmt::format("TURNS: {}", turnsLeft).c_str(), "bigFont.fnt");
    turnsLabel->setScale(0.0f);
    turnsLabel->setPosition({winSize.width / 2.f, winSize.height * 0.2f});
    this->addChild(turnsLabel, 9);

    this->setKeypadEnabled(true);
    this->setKeyboardEnabled(true);

    return true;
}

SwapAnimationLayer* SwapAnimationLayer::create(
    std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
    std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
    int turnsLeft
) {
    auto ret = new SwapAnimationLayer();
    if (ret->init(p1Name, p1Icon, p1c1, p1c2, p2Name, p2Icon, p2c1, p2c2, turnsLeft)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void SwapAnimationLayer::animateSwap(std::function<void()> onComplete) {
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto* sp1 = static_cast<SimplePlayer*>(player1IconSprite->getChildren()->objectAtIndex(0));
    auto* sp2 = static_cast<SimplePlayer*>(player2IconSprite->getChildren()->objectAtIndex(0));

    auto moveOut = CCEaseBackIn::create(CCMoveTo::create(0.6f, {winSize.width * -0.4f, winSize.height * 0.55f}));
    auto moveOut2 = CCEaseBackIn::create(CCMoveTo::create(0.6f, {winSize.width * 1.4f, winSize.height * 0.55f}));

    auto scaleUp1 = CCScaleTo::create(0.4f, 3.0f);
    auto scaleUp2 = CCScaleTo::create(0.4f, 3.0f);

    auto spawn1 = CCSpawn::create(moveOut, scaleUp1, nullptr);
    auto spawn2 = CCSpawn::create(moveOut2, scaleUp2, nullptr);

    auto flashCallback = CCCallFunc::create([this]() {
        player1IconSprite->setVisible(false);
        player2IconSprite->setVisible(false);
        
        auto folder1 = this->getChildByID("folder1");
        auto folder2 = this->getChildByID("folder2");
        if (folder1) folder1->setVisible(false);
        if (folder2) folder2->setVisible(false);

        auto flash = CCLayerColor::create({255, 255, 255, 255});
        flash->setPosition({0, 0});
        flash->setContentSize(CCDirector::sharedDirector()->getWinSize());
        flash->setOpacity(0);
        this->addChild(flash, 100, "flash");

        flash->runAction(CCSequence::create(
            CCEaseIn::create(CCFadeIn::create(0.08f), 2.0f),
            CCEaseOut::create(CCFadeOut::create(0.3f), 2.0f),
            CCRemoveSelf::create(),
            nullptr
        ));
    });

    auto swapCallback = CCCallFunc::create([this]() {
        player1IconSprite->setVisible(true);
        player2IconSprite->setVisible(true);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto* sp1 = static_cast<SimplePlayer*>(player1IconSprite->getChildren()->objectAtIndex(0));
        auto* sp2 = static_cast<SimplePlayer*>(player2IconSprite->getChildren()->objectAtIndex(0));

        sp1->updatePlayerFrame(this->player2Icon, IconType::Cube);
        sp1->setColor(this->player2Color1);
        sp1->setSecondColor(this->player2Color2);

        sp2->updatePlayerFrame(this->player1Icon, IconType::Cube);
        sp2->setColor(this->player1Color1);
        sp2->setSecondColor(this->player1Color2);

        auto folder1 = this->getChildByID("folder1");
        auto folder2 = this->getChildByID("folder2");
        if (folder1) folder1->setVisible(true);
        if (folder2) folder2->setVisible(true);

        player1NameLabel->setString(this->player2Name.c_str());
        player2NameLabel->setString(this->player1Name.c_str());
    });

    auto moveIn = CCEaseElasticOut::create(CCMoveTo::create(0.8f, {winSize.width * 0.75f, winSize.height * 0.55f}), 0.6f);
    auto moveIn2 = CCEaseElasticOut::create(CCMoveTo::create(0.8f, {winSize.width * 0.25f, winSize.height * 0.55f}), 0.6f);

    auto spawnIn1 = CCSpawn::create(moveIn, CCScaleTo::create(0.4f, 2.5f), nullptr);
    auto spawnIn2 = CCSpawn::create(moveIn2, CCScaleTo::create(0.4f, 2.5f), nullptr);

    auto nameFadeOut = CCEaseInOut::create(CCFadeOut::create(0.2f), 2.0f);
    auto nameFadeIn = CCEaseInOut::create(CCFadeIn::create(0.4f), 2.0f);

    player1IconSprite->runAction(CCSequence::create(
        spawn1, 
        flashCallback, 
        CCCallFunc::create([this]() {
            player1IconSprite->setPositionX(CCDirector::sharedDirector()->getWinSize().width * 1.4f);
        }), 
        spawnIn1, 
        nullptr
    ));

    player2IconSprite->runAction(CCSequence::create(
        spawn2, 
        swapCallback, 
        spawnIn2, 
        nullptr
    ));

    player1NameLabel->runAction(CCSequence::create(nameFadeOut, nameFadeIn, nullptr));
    player2NameLabel->runAction(CCSequence::create(nameFadeOut->clone(), nameFadeIn, nullptr));

    auto folder1 = getChildByID("folder1");
    auto folder2 = getChildByID("folder2");
    if (folder1) {
        auto folderMove = CCEaseBackIn::create(CCMoveTo::create(0.6f, {winSize.width * -0.4f + 45.f, winSize.height * 0.55f - 45.f}));
        folder1->runAction(CCSequence::create(folderMove, nullptr));
    }
    if (folder2) {
        auto folderMove = CCEaseBackIn::create(CCMoveTo::create(0.6f, {winSize.width * 1.4f + 45.f, winSize.height * 0.55f - 45.f}));
        folder2->runAction(CCSequence::create(folderMove, nullptr));
    }

    this->runAction(CCSequence::create(
        CCDelayTime::create(1.8f),
        CCCallFunc::create([onComplete]() {
            if (onComplete) onComplete();
        }),
        nullptr
    ));
}

void SwapAnimationLayer::animateTurnsDecrease(int from, int to, std::function<void()> onComplete) {
    if (from <= to) {
        if (onComplete) onComplete();
        return;
    }

    auto shakeAnim = CCEaseInOut::create(CCSequence::create(
        CCScaleTo::create(0.15f, 1.8f),
        CCScaleTo::create(0.1f, 0.6f),
        CCScaleTo::create(0.15f, 1.5f),
        CCScaleTo::create(0.1f, 0.7f),
        nullptr
    ), 2.0f);

    auto pulseRed = CCEaseInOut::create(CCSequence::create(
        CCCallFunc::create([this]() { turnsLabel->setColor({255, 50, 50}); }),
        CCScaleTo::create(0.1f, 2.0f),
        nullptr
    ), 2.0f);

    turnsLabel->runAction(CCSequence::create(
        shakeAnim,
        CCCallFunc::create([this, from, to]() {
            turnsLabel->setString(fmt::format("TURNS: {}", to).c_str());
            turnsLabel->setColor({255, 255, 255});
        }),
        CCEaseBackOut::create(CCScaleTo::create(0.4f, 0.5f)),
        CCCallFunc::create([onComplete]() {
            if (onComplete) onComplete();
        }),
        nullptr
    ));
}

void SwapAnimationLayer::fadeOutAndRemove() {
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto bgPanel = getChildByID("bgPanel");
    auto turnBg = getChildByID("turnBg");

    if (bgPanel) {
        bgPanel->runAction(CCSequence::create(
            CCEaseIn::create(CCMoveTo::create(0.5f, {winSize.width / 2.f, -300.f}), 2.0f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }

    if (turnBg) {
        turnBg->runAction(CCFadeOut::create(0.3f));
    }

    auto titleMove = CCEaseIn::create(CCMoveTo::create(0.5f, {winSize.width / 2.f, winSize.height + 100.f}), 2.0f);
    auto titleScale = CCScaleTo::create(0.5f, 0.0f);
    auto titleSpawn = CCSpawn::create(titleMove, titleScale, CCFadeOut::create(0.5f), nullptr);

    auto name1Move = CCEaseIn::create(CCMoveTo::create(0.4f, {winSize.width * 0.25f, -200.f}), 2.0f);
    auto name2Move = CCEaseIn::create(CCMoveTo::create(0.4f, {winSize.width * 0.75f, -200.f}), 2.0f);

    titleLabel->runAction(titleSpawn);
    player1NameLabel->runAction(CCSequence::create(CCFadeOut::create(0.3f), name1Move, nullptr));
    player2NameLabel->runAction(CCSequence::create(CCFadeOut::create(0.3f), name2Move, nullptr));
    turnsLabel->runAction(CCFadeOut::create(0.3f));

    auto folder1 = getChildByID("folder1");
    auto folder2 = getChildByID("folder2");
    if (folder1) folder1->runAction(CCFadeOut::create(0.3f));
    if (folder2) folder2->runAction(CCFadeOut::create(0.3f));

    this->runAction(CCSequence::create(
        CCDelayTime::create(0.6f),
        CCRemoveSelf::create(),
        nullptr
    ));
}

void SwapAnimationLayer::showSwapAnimation(
    std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
    std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
    int turnsLeft, int previousTurns, std::function<void()> onComplete
) {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    auto layer = SwapAnimationLayer::create(
        p1Name, p1Icon, p1c1, p1c2,
        p2Name, p2Icon, p2c1, p2c2,
        turnsLeft
    );

    scene->addChild(layer, 9999);

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    layer->fadeLayer->runAction(CCFadeTo::create(0.4f, 200));

    auto bgPanel = layer->getChildByID("bgPanel");
    if (bgPanel) {
        bgPanel->setPositionY(-300.f);
        bgPanel->runAction(CCEaseBackOut::create(CCMoveTo::create(0.5f, 
            {winSize.width / 2.f - 200.f, winSize.height / 2.f - 150.f})));
    }

    layer->titleLabel->runAction(CCSequence::create(
        CCScaleTo::create(0.3f, 1.5f),
        CCEaseBackOut::create(CCScaleTo::create(0.2f, 0.8f)),
        nullptr
    ));

    auto nameInAnim = CCEaseBackOut::create(CCScaleTo::create(0.4f, 0.6f));
    layer->player1NameLabel->runAction(nameInAnim->clone());
    layer->player2NameLabel->runAction(nameInAnim->clone());

    auto folder1 = layer->getChildByID("folder1");
    auto folder2 = layer->getChildByID("folder2");
    if (folder1) folder1->runAction(CCFadeIn::create(0.5f));
    if (folder2) folder2->runAction(CCFadeIn::create(0.5f));

    auto turnBg = layer->getChildByID("turnBg");
    if (turnBg) {
        turnBg->runAction(CCFadeIn::create(0.5f));
    }
    
    auto turnsInAnim = CCEaseBackOut::create(CCScaleTo::create(0.5f, 0.5f));
    layer->turnsLabel->runAction(CCSequence::create(
        CCDelayTime::create(0.3f),
        turnsInAnim,
        nullptr
    ));

    layer->runAction(CCSequence::create(
        CCDelayTime::create(1.2f),
        CCCallFunc::create([layer, onComplete]() {
            layer->animateSwap([layer, onComplete]() {
                if (previousTurns > turnsLeft) {
                    layer->animateTurnsDecrease(previousTurns, turnsLeft, [layer, onComplete]() {
                        layer->runAction(CCSequence::create(
                            CCDelayTime::create(0.8f),
                            CCCallFunc::create([layer, onComplete]() {
                                layer->fadeOutAndRemove();
                                if (onComplete) onComplete();
                            }),
                            nullptr
                        ));
                    });
                } else {
                    layer->runAction(CCSequence::create(
                        CCDelayTime::create(0.8f),
                        CCCallFunc::create([layer, onComplete]() {
                            layer->fadeOutAndRemove();
                            if (onComplete) onComplete();
                        }),
                        nullptr
                    ));
                }
            });
        }),
        nullptr
    ));
}