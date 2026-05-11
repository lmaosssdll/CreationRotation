#pragma once

#include <Geode/Geode.hpp>
#include <types/lobby.hpp>
#include <defs.hpp>
#include <functional>

using namespace geode::prelude;

class SwapAnimationLayer : public CCLayer {
protected:
    std::string player1Name;
    std::string player2Name;
    int player1Icon;
    int player2Icon;
    cocos2d::ccColor3B player1Color1;
    cocos2d::ccColor3B player1Color2;
    cocos2d::ccColor3B player2Color1;
    cocos2d::ccColor3B player2Color2;

    CCSprite* player1IconSprite = nullptr;
    CCSprite* player2IconSprite = nullptr;
    CCLabelBMFont* player1NameLabel = nullptr;
    CCLabelBMFont* player2NameLabel = nullptr;
    CCLabelBMFont* titleLabel = nullptr;
    CCLabelBMFont* turnsLabel = nullptr;
    CCLayerColor* fadeLayer = nullptr;

    int currentTurnsLeft = 0;
    int previousTurnsLeft = 0;
    int targetTurnsLeft = 0;

    std::function<void()> m_animCallback;

    virtual bool init(
        std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
        std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
        int turnsLeft
    );
    
    void onFlash(CCNode*);
    void onSwap(CCNode*);
    void resetPos();
    void onSwapDone();
    void onTurnsDone();
    void doFadeOut();

    void animateSwap();
    void animateTurns();
    void fadeOutAndRemove();

public:
    static SwapAnimationLayer* create(
        std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
        std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
        int turnsLeft
    );

    static void showSwapAnimation(
        std::string p1Name, int p1Icon, cocos2d::ccColor3B p1c1, cocos2d::ccColor3B p1c2,
        std::string p2Name, int p2Icon, cocos2d::ccColor3B p2c1, cocos2d::ccColor3B p2c2,
        int turnsLeft, int previousTurns, std::function<void()> onComplete = nullptr
    );
};