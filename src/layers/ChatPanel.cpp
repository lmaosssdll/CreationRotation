#include "ChatPanel.hpp"
#include <network/manager.hpp>
#include <network/packets/all.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

ChatPanel* ChatPanel::create() {
    auto ret = new ChatPanel;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void ChatPanel::initialize() {
    if (!hasInitialized) {
        auto& nm = NetworkManager::get();

        nm.on<MessageSentPacket>([](MessageSentPacket packet) {
            messages.push_back(packet.message);
            messagesQueue.push_back(packet.message);

            Notification::create(
                fmt::format("{} sent a message", packet.message.author.name),
                CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png")
            )->show();
        });

        hasInitialized = true;
    }
}

bool ChatPanel::init() {
    if (!Popup::init(350.f, 280.f)) {
        return false;
    }

    this->setTitle("Chat");
    ChatPanel::initialize();

    // Dark container behind the scroll area
    auto scrollContainer = CCScale9Sprite::create("square02b_001.png");
    scrollContainer->setContentSize({
        m_mainLayer->getContentWidth() - 50.f,
        m_mainLayer->getContentHeight() - 85.f
    });
    scrollContainer->setColor(ccc3(0, 0, 0));
    scrollContainer->setOpacity(75);

    scrollLayer = ScrollLayer::create(scrollContainer->getContentSize() - 10.f);
    scrollLayer->ignoreAnchorPointForPosition(false);

    scrollLayer->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setAutoGrowAxis(scrollLayer->getContentHeight())
            ->setAxisAlignment(AxisAlignment::End)
            ->setGap(6.f)
    );
    scrollContainer->addChildAtPosition(scrollLayer, Anchor::Center);
    m_mainLayer->addChildAtPosition(scrollContainer, Anchor::Center, ccp(0, 5.f));

    // Input + send button
    auto inputContainer = CCNode::create();
    inputContainer->setContentSize({ scrollContainer->getContentWidth(), 75.f });
    inputContainer->setAnchorPoint({ 0.5f, 0.f });

    messageInput = TextInput::create(
        inputContainer->getContentWidth(), "Send a message to the lobby!", "chatFont.fnt"
    );

    std::string allowedChars =
        " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\"'?!*/.,";
    messageInput->getInputNode()->setAllowedChars(allowedChars);

    auto sendMsgBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
        "GJ_chatBtn_001.png", 0.75f,
        [this](CCObject*) { this->sendMessage(); }
    );
    sendMsgBtn->ignoreAnchorPointForPosition(true);

    auto msgBtnMenu = CCMenu::create();
    msgBtnMenu->addChild(sendMsgBtn);
    msgBtnMenu->setContentSize(sendMsgBtn->getContentSize());

    inputContainer->addChild(messageInput);
    inputContainer->addChild(msgBtnMenu);
    inputContainer->setLayout(RowLayout::create());
    m_mainLayer->addChildAtPosition(inputContainer, Anchor::Bottom, ccp(0, 10.f));

    for (auto message : ChatPanel::messages) {
        renderMessage(message);
    }

    this->schedule(schedule_selector(ChatPanel::updateMessages));

    return true;
}

// ─────────────────────────────────────────────────────────────
//  Render a single message: [Icon + Name]  [Comment Bubble]
// ─────────────────────────────────────────────────────────────
void ChatPanel::renderMessage(Message const& message) {
    auto gm = GameManager::get();

    // ── Left column: cube icon + username underneath ──────────
    constexpr float iconColumnWidth = 55.f;

    auto playerIcon = SimplePlayer::create(0);
    playerIcon->updatePlayerFrame(message.author.iconID, IconType::Cube);
    playerIcon->setColor(gm->colorForIdx(message.author.color1));
    playerIcon->setSecondColor(gm->colorForIdx(message.author.color2));
    if (message.author.color3 == -1) {
        playerIcon->disableGlowOutline();
    } else {
        playerIcon->setGlowOutline(gm->colorForIdx(message.author.color3));
    }
    playerIcon->setScale(0.65f);

    // Gold username label (like GD comments)
    auto nameLabel = CCLabelBMFont::create(
        message.author.name.c_str(), "goldFont.fnt"
    );
    nameLabel->setAnchorPoint({0.5f, 0.5f});
    nameLabel->setScale(0.45f);
    // Clamp so the name fits under the icon
    if (nameLabel->getContentWidth() * nameLabel->getScale() > iconColumnWidth) {
        nameLabel->setScale(iconColumnWidth / nameLabel->getContentWidth());
    }

    // ── Right side: comment-style bubble ─────────────────────
    constexpr float padX = 10.f;
    constexpr float padY = 8.f;
    float maxBubbleW = scrollLayer->getContentWidth() - iconColumnWidth - 10.f;
    float maxTextW   = maxBubbleW - padX * 2.f;

    // Message label — main GD font
    auto msgLabel = CCLabelBMFont::create(
        message.message.c_str(), "bigFont.fnt"
    );
    constexpr float textScale = 0.35f;
    msgLabel->setScale(textScale);
    msgLabel->setAnchorPoint({0.f, 0.5f});

    // Enable word-wrap when the line is too long
    if (msgLabel->getContentWidth() * textScale > maxTextW) {
        msgLabel->setWidth(maxTextW / textScale);
    }

    float textH    = msgLabel->getContentHeight() * textScale;
    float bubbleH  = std::max(textH + padY * 2.f, 30.f);

    // Brown bubble background (classic GD comment look)
    auto bubble = CCScale9Sprite::create("square02b_001.png");
    bubble->setContentSize({maxBubbleW, bubbleH});
    bubble->setColor(ccc3(130, 64, 33));   // GD-comment brown
    bubble->setOpacity(210);

    // Place text centred vertically inside the bubble
    msgLabel->setPosition({padX, bubbleH / 2.f});
    bubble->addChild(msgLabel);

    // ── Assemble the row ─────────────────────────────────────
    float rowH = std::max(bubbleH, 48.f);

    // Icon column (icon on top, name below, both centred)
    auto iconColumn = CCNode::create();
    iconColumn->setContentSize({iconColumnWidth, rowH});
    iconColumn->addChild(playerIcon);
    iconColumn->addChild(nameLabel);
    iconColumn->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)          // top → bottom
            ->setGap(2.f)
            ->setAxisAlignment(AxisAlignment::Center)
            ->setCrossAxisAlignment(AxisAlignment::Center)
    );
    iconColumn->updateLayout();

    // Row node holds icon column + bubble side-by-side
    auto rowNode = CCNode::create();
    rowNode->setAnchorPoint({0.f, 0.f});
    rowNode->setContentSize({scrollLayer->getContentWidth(), rowH});
    rowNode->addChild(iconColumn);
    rowNode->addChild(bubble);
    rowNode->setLayout(
        RowLayout::create()
            ->setGap(5.f)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisOverflow(false)
    );
    rowNode->updateLayout();

    // Add to the scrollable message list
    scrollLayer->m_contentLayer->addChild(rowNode);
    scrollLayer->m_contentLayer->updateLayout();
}

void ChatPanel::updateMessages(float dt) {
    if (messagesQueue.empty()) return;

    for (auto const& message : messagesQueue) {
        renderMessage(message);
    }
    messagesQueue.clear();
}

void ChatPanel::clearMessages() {
    messages.clear();
    auto& nm = NetworkManager::get();
    nm.unbind<MessageSentPacket>();
    hasInitialized = false;
}

void ChatPanel::sendMessage() {
    auto msgInput = messageInput->getString();

    if (msgInput.empty()) return;
    if (geode::utils::string::replace(" ", msgInput, "") == "") return;

    auto& nm = NetworkManager::get();
    nm.send(SendMessagePacket::create(msgInput));

    messageInput->setString("");
}

void ChatPanel::keyDown(cocos2d::enumKeyCodes keycode, double timestamp) {
    if (keycode == cocos2d::KEY_Enter &&
        CCIMEDispatcher::sharedDispatcher()->hasDelegate()) {
        sendMessage();
    } else {
        Popup::keyDown(keycode, timestamp);
    }
}
