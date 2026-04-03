#include "ChatPanel.hpp"
#include <network/manager.hpp>
#include <network/packets/all.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// Factory method to create and initialize the ChatPanel
ChatPanel* ChatPanel::create() {
    auto ret = new ChatPanel;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

// Sets up the network listener for incoming chat messages
void ChatPanel::initialize() {
    if (!hasInitialized) {
        auto& nm = NetworkManager::get();
        
        // Listen for messages from the server
        nm.on<MessageSentPacket>([](MessageSentPacket packet) {
            messages.push_back(packet.message);
            messagesQueue.push_back(packet.message);
            
            // Show an in-game notification when a new message arrives
            Notification::create(
                fmt::format("{} sent a message", packet.message.author.name),
                CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png")
            )->show();
        });

        hasInitialized = true;
    }
}

// Initializes the popup UI (layout, inputs, buttons)
bool ChatPanel::init() {
    // Initialize base popup with size 350x280
    if (!Popup::init(350.f, 280.f)) {
        return false;
    }

    this->setTitle("Chat");
    ChatPanel::initialize();

    // Create a dark background container for the scroll layer
    auto scrollContainer = CCScale9Sprite::create("square02b_001.png");
    scrollContainer->setContentSize({
        m_mainLayer->getContentWidth() - 50.f,
        m_mainLayer->getContentHeight() - 85.f
    });
    scrollContainer->setColor(ccc3(0,0,0));
    scrollContainer->setOpacity(75);

    // Set up the scrollable list for chat messages
    scrollLayer = ScrollLayer::create(scrollContainer->getContentSize() - 10.f);
    scrollLayer->ignoreAnchorPointForPosition(false);
    
    // Layout for the scroll container (messages stack from bottom to top)
    scrollLayer->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setAutoGrowAxis(scrollLayer->getContentHeight())
            ->setAxisAlignment(AxisAlignment::End)
            ->setGap(6.f) // Gap between message bubbles
    );
    scrollContainer->addChildAtPosition(scrollLayer, Anchor::Center);
    m_mainLayer->addChildAtPosition(scrollContainer, Anchor::Center, ccp(0, 5.f));

    // Create a container for the text input field and the send button
    auto inputContainer = CCNode::create();
    inputContainer->setContentSize({ scrollContainer->getContentWidth(), 75.f });
    inputContainer->setAnchorPoint({ 0.5f, 0.f });

    messageInput = TextInput::create(inputContainer->getContentWidth(), "Send a message to the lobby!", "chatFont.fnt");
    
    // Explicitly allow specific characters including: " ' ? ! * / . ,
    // Add Russian alphabet here if Cyrillic support is needed
    std::string allowedChars = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\"'?!*/.,";
    messageInput->getInputNode()->setAllowedChars(allowedChars);

    // Create the send message button
    auto sendMsgBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
        "GJ_chatBtn_001.png",
        0.75f,
        [this](CCObject*) {
            this->sendMessage();
        }
    );
    sendMsgBtn->ignoreAnchorPointForPosition(true);

    auto msgBtnMenu = CCMenu::create();
    msgBtnMenu->addChild(sendMsgBtn);
    msgBtnMenu->setContentSize(sendMsgBtn->getContentSize());

    inputContainer->addChild(messageInput);
    inputContainer->addChild(msgBtnMenu);
    inputContainer->setLayout(RowLayout::create());
    m_mainLayer->addChildAtPosition(inputContainer, Anchor::Bottom, ccp(0, 10.f));

    // Render any previously loaded messages
    for (auto message : ChatPanel::messages) {
        renderMessage(message);
    }

    // Check for new messages every frame
    this->schedule(schedule_selector(ChatPanel::updateMessages));

    return true;
}

// Renders a single message with Player Icon (Cube) and Chat Bubble
void ChatPanel::renderMessage(Message const& message) {
    // 1. Create row container for Icon + Bubble
    auto rowNode = CCNode::create();
    rowNode->setAnchorPoint({0.f, 0.f});

    // 2. Create Player Icon (Cube) using logic from PlayerCell
    auto playerIcon = SimplePlayer::create(0);
    auto gm = GameManager::get();

    playerIcon->updatePlayerFrame(message.author.iconID, IconType::Cube);
    playerIcon->setColor(gm->colorForIdx(message.author.color1));
    playerIcon->setSecondColor(gm->colorForIdx(message.author.color2));

    if (message.author.color3 == -1) {
        playerIcon->disableGlowOutline();
    } else {
        playerIcon->setGlowOutline(gm->colorForIdx(message.author.color3));
    }
    
    playerIcon->setScale(0.65f);
    
    // Wrap icon in a CCNode to keep Layout predictable
    auto iconContainer = CCNode::create();
    iconContainer->setContentSize({30.f, 30.f});
    playerIcon->setPosition(iconContainer->getContentSize() / 2);
    iconContainer->addChild(playerIcon);

    // 3. Create Text
    float maxTextWidth = scrollLayer->getContentWidth() - 50.f; // Account for icon size
    
    auto msgText = TextArea::create(
        fmt::format("<cy>{}</c>: {}", message.author.name, message.message),
        "chatFont.fnt", 0.5f, maxTextWidth, {0.f, 1.f}, 17.f, false
    );
    msgText->setAnchorPoint({ 0.f, 1.f }); // Anchor top-left

    float textHeight = msgText->m_label->m_lines->count() * 17.f;
    msgText->setContentSize({maxTextWidth, textHeight});

    // 4. Create Chat Bubble (Background)
    auto bubble = CCScale9Sprite::create("square02b_001.png");
    bubble->setColor(ccc3(0, 0, 0)); // Black bubble
    bubble->setOpacity(120);         // Semi-transparent
    
    float paddingX = 8.f;
    float paddingY = 8.f;
    bubble->setContentSize({maxTextWidth + (paddingX * 2), textHeight + (paddingY * 2)});
    
    // Position text inside bubble
    msgText->setPosition({paddingX, bubble->getContentHeight() - paddingY});
    bubble->addChild(msgText);

    // 5. Build the Row Layout
    rowNode->addChild(iconContainer);
    rowNode->addChild(bubble);
    
    // Make row container match bubble's height (or icon height, whichever is larger)
    float rowHeight = std::max(bubble->getContentHeight(), iconContainer->getContentHeight());
    rowNode->setContentSize({scrollLayer->getContentWidth(), rowHeight});
    
    // Align items horizontally
    rowNode->setLayout(
        RowLayout::create()
            ->setGap(5.f)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisOverflow(false)
    );
    rowNode->updateLayout();

    // 6. Add to Scroll Layer
    scrollLayer->m_contentLayer->addChild(rowNode);
    scrollLayer->m_contentLayer->updateLayout();
}

// Processes the message queue and renders new incoming messages
void ChatPanel::updateMessages(float dt) {
    if (messagesQueue.empty()) return;

    for (auto const& message : messagesQueue) {
        renderMessage(message);
    }
    messagesQueue.clear();
}

// Clears the message history and unbinds the network listener
void ChatPanel::clearMessages() {
    messages.clear();
    auto& nm = NetworkManager::get();
    nm.unbind<MessageSentPacket>();
    hasInitialized = false;
}

// Validates and sends the message to the server
void ChatPanel::sendMessage() {
    auto msgInput = messageInput->getString();
    
    // Prevent sending empty messages or messages containing only spaces
    if (msgInput.empty()) return;
    if (geode::utils::string::replace(" ", msgInput, "") == "") return;

    auto& nm = NetworkManager::get();
    nm.send(SendMessagePacket::create(msgInput));

    // Clear the input field after sending
    messageInput->setString("");
}

// Allows sending a message by pressing the "Enter" key
void ChatPanel::keyDown(cocos2d::enumKeyCodes keycode, double timestamp) {
    if (keycode == cocos2d::KEY_Enter && CCIMEDispatcher::sharedDispatcher()->hasDelegate()) {
        sendMessage();
    } else {
        // Forward other keys to the base Popup class
        Popup::keyDown(keycode, timestamp);
    }
}
