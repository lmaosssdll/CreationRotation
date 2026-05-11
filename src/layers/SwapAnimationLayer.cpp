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

    titleLabel = CCLabelBMFont::create("SWAPPING!", "bigFont.fnt");
    titleLabel->setScale(0.8f);
    titleLabel->setPosition({winSize.width / 2.f, winSize.height * 0.85f});
    titleLabel->setOpacity(0);
    this->addChild(titleLabel, 3);

    auto createPlayerIcon = [&](SimplePlayer*& sp, int iconID, ccColor3B c1, ccColor3B c2, float scale) {
        sp = SimplePlayer::create(0);
        sp->updatePlayerFrame(iconID, IconType::Cube);
        sp->setColor(c1);
        sp->setSecondColor(c2);
        sp->setScale(scale);
        sp->setZOrder(2);
    };

    SimplePlayer* sp1 = nullptr;
    SimplePlayer* sp2 = nullptr;
    createPlayerIcon(sp1, player1Icon, player1Color1, player1Color2, 2.0f);
    createPlayerIcon(sp2, player2Icon, player2Color1, player2Color2, 2.0f);

    player1IconSprite = CCSprite::create();
    player1IconSprite->addChild(sp1);
    sp1->setPosition({sp1->getContentSize().width / 2.f, sp1->getContentSize().height / 2.f});
    player1IconSprite->setContentSize(sp1->getContentSize());
    player1IconSprite->setPosition({winSize.width * 0.25f, winSize.height * 0.55f});
    this->addChild(player1IconSprite, 2);

    player2IconSprite = CCSprite::create();
    player2IconSprite->addChild(sp2);
    sp2->setPosition({sp2->getContentSize().width / 2.f, sp2->getContentSize().height / 2.f});
    player2IconSprite->setContentSize(sp2->getContentSize());
    player2IconSprite->setPosition({winSize.width * 0.75f, winSize.height * 0.55f});
    this->addChild(player2IconSprite, 2);

    auto icon1Folder = CCSprite::createWithSpriteFrameName("folderIcon_002.png");
    if (icon1Folder) {
        icon1Folder->setScale(0.6f);
        icon1Folder->setPosition({player1IconSprite->getPositionX() + 35.f, player1IconSprite->getPositionY() - 35.f});
        icon1Folder->setZOrder(3);
        this->addChild(icon1Folder, 2, "folder1");
    }

    auto icon2Folder = CCSprite::createWithSpriteFrameName("folderIcon_002.png");
    if (icon2Folder) {
        icon2Folder->setScale(0.6f);
        icon2Folder->setPosition({player2IconSprite->getPositionX() + 35.f, player2IconSprite->getPositionY() - 35.f});
        icon2Folder->setZOrder(3);
        this->addChild(icon2Folder, 2, "folder2");
    }

    player1NameLabel = CCLabelBMFont::create(player1Name.c_str(), "bigFont.fnt");
    player1NameLabel->setScale(0.6f);
    player1NameLabel->setPosition({winSize.width * 0.25f, winSize.height * 0.38f});
    player1NameLabel->setOpacity(0);
    this->addChild(player1NameLabel, 3);

    player2NameLabel = CCLabelBMFont::create(player2Name.c_str(), "bigFont.fnt");
    player2NameLabel->setScale(0.6f);
    player2NameLabel->setPosition({winSize.width * 0.75f, winSize.height * 0.38f});
    player2NameLabel->setOpacity(0);
    this->addChild(player2NameLabel, 3);

    turnsLabel = CCLabelBMFont::create(fmt::format("TURNS: {}", turnsLeft).c_str(), "bigFont.fnt");
    turnsLabel->setScale(0.5f);
    turnsLabel->setPosition({winSize.width / 2.f, winSize.height * 0.2f});
    turnsLabel->setOpacity(0);
    this->addChild(turnsLabel, 3);

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

    auto moveOut = CCEaseBackIn::create(CCMoveTo::create(0.5f, {winSize.width * -0.3f, winSize.height * 0.55f}));
    auto moveOut2 = CCEaseBackIn::create(CCMoveTo::create(0.5f, {winSize.width * 1.3f, winSize.height * 0.55f}));

    auto scaleUp1 = CCScaleTo::create(0.3f, 2.5f);
    auto scaleUp2 = CCScaleTo::create(0.3f, 2.5f);
    auto scaleDown1 = CCScaleTo::create(0.3f, 2.0f);
    auto scaleDown2 = CCScaleTo::create(0.3f, 2.0f);

    auto spawn1 = CCSpawn::create(moveOut, scaleUp1, nullptr);
    auto spawn2 = CCSpawn::create(moveOut2, scaleUp2, nullptr);

    auto flash1 = CCCallFunc::create([this]() {
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
            CCFadeIn::create(0.1f),
            CCFadeOut::create(0.2f),
            CCRemoveSelf::create(),
            nullptr
        ));
    });

    auto swapVisibility = CCCallFunc::create([this]() {
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

    auto moveIn = CCEaseBackOut::create(CCMoveTo::create(0.5f, {winSize.width * 0.75f, winSize.height * 0.55f}));
    auto moveIn2 = CCEaseBackOut::create(CCMoveTo::create(0.5f, {winSize.width * 0.25f, winSize.height * 0.55f}));

    auto spawnIn1 = CCSpawn::create(moveIn, CCScaleTo::create(0.3f, 2.0f), nullptr);
    auto spawnIn2 = CCSpawn::create(moveIn2, CCScaleTo::create(0.3f, 2.0f), nullptr);

    player1IconSprite->runAction(CCSequence::create(spawn1, flash1, CCCallFunc::create([this]() {
        player1IconSprite->setPositionX(CCDirector::sharedDirector()->getWinSize().width * 1.3f);
    }), spawnIn1, nullptr));

    player2IconSprite->runAction(CCSequence::create(spawn2, swapVisibility, spawnIn2, nullptr));

    player1NameLabel->runAction(CCSequence::create(
        CCFadeOut::create(0.25f),
        CCFadeIn::create(0.5f),
        nullptr
    ));

    player2NameLabel->runAction(CCSequence::create(
        CCFadeOut::create(0.25f),
        CCFadeIn::create(0.5f),
        nullptr
    ));

    auto folder1 = getChildByID("folder1");
    auto folder2 = getChildByID("folder2");
    if (folder1) folder1->runAction(CCSequence::create(spawn1->clone(), nullptr));
    if (folder2) folder2->runAction(CCSequence::create(spawn2->clone(), nullptr));

    this->runAction(CCSequence::create(
        CCDelayTime::create(1.5f),
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

    turnsLabel->runAction(CCSequence::create(
        CCScaleTo::create(0.2f, 1.5f),
        CCCallFunc::create([this, from, to]() {
            turnsLabel->setString(fmt::format("TURNS: {}", to).c_str());
        }),
        CCEaseBackOut::create(CCScaleTo::create(0.3f, 0.5f)),
        CCCallFunc::create([onComplete]() {
            if (onComplete) onComplete();
        }),
        nullptr
    ));
}

void SwapAnimationLayer::fadeOutAndRemove() {
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto bgMove = CCEaseIn::create(CCMoveTo::create(0.5f, {winSize.width / 2.f, winSize.height * 1.5f}), 2.0f);
    auto scaleDown = CCScaleTo::create(0.5f, 0.3f);
    auto fadeOut = CCFadeOut::create(0.5f);

    auto spawn = CCSpawn::create(bgMove, scaleDown, fadeOut, nullptr);

    titleLabel->runAction(CCFadeOut::create(0.3f));
    player1NameLabel->runAction(CCFadeOut::create(0.3f));
    player2NameLabel->runAction(CCFadeOut::create(0.3f));
    turnsLabel->runAction(CCFadeOut::create(0.3f));

    this->runAction(CCSequence::create(
        spawn,
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

    layer->fadeLayer->runAction(CCFadeTo::create(0.3f, 180));

    layer->titleLabel->runAction(CCSequence::create(
        CCFadeIn::create(0.3f),
        CCScaleTo::create(0.3f, 1.2f),
        CCEaseBackOut::create(CCScaleTo::create(0.2f, 0.8f)),
        nullptr
    ));

    layer->player1NameLabel->runAction(CCFadeIn::create(0.4f));
    layer->player2NameLabel->runAction(CCFadeIn::create(0.4f));
    layer->turnsLabel->runAction(CCFadeIn::create(0.5f));

    auto folder1 = layer->getChildByID("folder1");
    auto folder2 = layer->getChildByID("folder2");
    if (folder1) folder1->runAction(CCFadeIn::create(0.4f));
    if (folder2) folder2->runAction(CCFadeIn::create(0.4f));

    layer->runAction(CCSequence::create(
        CCDelayTime::create(1.0f),
        CCCallFunc::create([layer, onComplete]() {
            layer->animateSwap([layer, onComplete]() {
                if (previousTurns > turnsLeft) {
                    layer->animateTurnsDecrease(previousTurns, turnsLeft, [layer, onComplete]() {
                        layer->runAction(CCSequence::create(
                            CCDelayTime::create(0.5f),
                            CCCallFunc::create([layer, onComplete]() {
                                layer->fadeOutAndRemove();
                                if (onComplete) onComplete();
                            }),
                            nullptr
                        ));
                    });
                } else {
                    layer->runAction(CCSequence::create(
                        CCDelayTime::create(0.5f),
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