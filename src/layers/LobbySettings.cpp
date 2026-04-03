#include "LobbySettings.hpp"

enum LobbySettingType {
    Name,
    Turns,
    MinsPerTurn,
    Password,
    IsPublic 
};

class LobbySettingsCell : public CCNode {
protected:
    bool init(float width, std::string name, LobbySettingType type, std::string desc, std::string defaultStr = "", std::string filter = "") {
        this->type = type;
        
        this->setContentSize({ width, CELL_HEIGHT });

        auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        nameLabel->setPosition({ 5.f, CELL_HEIGHT / 2.f });
        nameLabel->setAnchorPoint({ 0.f, 0.5f });
        nameLabel->limitLabelWidth(80.f, 0.5f, 0.1f);
        this->addChild(nameLabel);

        auto infoBtn = InfoAlertButton::create(name, desc, 0.5f);
        auto menu = CCMenu::create();
        menu->addChild(infoBtn);
        menu->setPosition({ nameLabel->getScaledContentWidth() + 15.f, CELL_HEIGHT / 2.f });
        menu->setAnchorPoint({ 0.f, 0.5f });
        this->addChild(menu);

        switch (this->type) {
            case Name: [[fallthrough]];
            case Turns: [[fallthrough]];
            case MinsPerTurn: [[fallthrough]];
            case Password: {
                input = TextInput::create(95.f, name);
                input->setString(defaultStr);
                if (filter != "") input->getInputNode()->setAllowedChars(filter);
                input->setPosition({ width - input->getContentWidth() - 5.f, CELL_HEIGHT / 2.f });
                input->setAnchorPoint({ 0.f, 0.5f });
                this->addChild(input);
                break;
            }
            case IsPublic: {
                toggler = CCMenuItemExt::createTogglerWithStandardSprites(
                    0.65f,
                    [this](CCObject*) {
                        togglerVal = !toggler->isOn();
                    }
                );
                toggler->toggle(geode::utils::numFromString<int>(defaultStr).unwrapOr(0));
                auto menu2 = CCMenu::create();
                menu2->addChild(toggler);
                menu2->setPosition({ width - toggler->getContentWidth() - 5.f, CELL_HEIGHT / 2.f });
                menu2->setAnchorPoint({ 0.f, 0.5f });
                this->addChild(menu2);
                break;
            }
            case Tag: { // Логика для кнопки выбора тегов
                tagIdx = geode::utils::numFromString<int>(defaultStr).unwrapOr(0);
                if (tagIdx < 0 || tagIdx >= TAG_NAMES.size()) tagIdx = 0;

                auto btnSpr = ButtonSprite::create(TAG_NAMES[tagIdx].c_str(), 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.6f);
                tagBtn = CCMenuItemExt::createSpriteExtra(btnSpr, [this](CCObject*) {
                    tagIdx = (tagIdx + 1) % TAG_NAMES.size();
                    auto newSpr = ButtonSprite::create(TAG_NAMES[tagIdx].c_str(), 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.6f);
                    tagBtn->setNormalImage(newSpr);
                });

                auto menu3 = CCMenu::create();
                menu3->addChild(tagBtn);
                menu3->setPosition({ width - 50.f, CELL_HEIGHT / 2.f });
                this->addChild(menu3);
                break;
            }
        }

        return true;
    }
public:
    static constexpr int CELL_HEIGHT = 35.f;

    TextInput* input = nullptr;
    CCMenuItemToggler* toggler = nullptr;
    CCMenuItemSpriteExtra* tagBtn = nullptr;

    LobbySettingType type;
    bool togglerVal = false;
    int tagIdx = 0; // Сохраняем индекс тега

    void save(LobbySettings& settings) {
        switch (this->type) {
            case Name: settings.name = this->input->getString(); break;
            case Turns: settings.turns = geode::utils::numFromString<int>(this->input->getString()).unwrapOr(0); break;
            case MinsPerTurn: settings.minutesPerTurn = geode::utils::numFromString<int>(this->input->getString()).unwrapOr(1); break;
            case Password: break;
            case IsPublic: settings.isPublic = togglerVal; break;
            case Tag: settings.tag = tagIdx; break; // Сохраняем выбранный тег
        }
    }

    static LobbySettingsCell* create(float width, std::string name, LobbySettingType type, std::string desc, std::string defaultStr = "", std::string filter = "") {
        auto ret = new LobbySettingsCell;
        if (ret->init(width, name, type, desc, defaultStr, filter)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

LobbySettingsPopup* LobbySettingsPopup::create(LobbySettings const& settings, Callback callback) {
    auto ret = new LobbySettingsPopup;
    if (ret->init(settings, std::move(callback))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LobbySettingsPopup::init(LobbySettings const& settings, Callback callback) {
    if (!Popup::init(250.f, 250.f)) { // Немного увеличил высоту для новой кнопки
        return false;
    }

    m_noElasticity = true;
    this->setTitle("Lobby Settings");

    auto settingsContents = CCArray::create();

    #define ADD_SETTING(name, element, desc, type) settingsContents->addObject( \
        LobbySettingsCell::create(220.f, name, LobbySettingType::type, desc, settings.element));

    #define ADD_SETTING_INT(name, element, desc, type) settingsContents->addObject( \
        LobbySettingsCell::create(220.f, name, LobbySettingType::type, desc, std::to_string(settings.element), "0123456789"));

    #define ADD_SETTING_BOOL(name, element, desc, type) settingsContents->addObject( \
        LobbySettingsCell::create(220.f, name, LobbySettingType::type, desc, std::to_string(settings.element), ""));

    ADD_SETTING("Name", name, "Name of the lobby", Name)
    ADD_SETTING_INT("Turns", turns, "Number of turns per level", Turns)
    ADD_SETTING_INT("Minutes/Turn", minutesPerTurn, "Amount of minutes per turn", MinsPerTurn)
    ADD_SETTING_BOOL("Public", isPublic, "Whether the swap is public. If it is, it will be shown in the room list for others to join.", IsPublic)
    
    // Добавляем тег в меню
    ADD_SETTING_INT("Tag", tag, "Select a tag to describe your lobby", Tag)

    auto settingsList = ListView::create(settingsContents, LobbySettingsCell::CELL_HEIGHT, 220.f, 200.f);
    settingsList->ignoreAnchorPointForPosition(false);

    auto border = Border::create(settingsList, {0, 0, 0, 75}, {220.f, 200.f});
    if(auto borderSprite = typeinfo_cast<NineSlice*>(border->getChildByID("geode.loader/border_sprite"))) {
        float scaleFactor = 1.7f;
        borderSprite->setContentSize(CCSize{borderSprite->getContentSize().width, borderSprite->getContentSize().height + 3} / scaleFactor);
        borderSprite->setScale(scaleFactor);
        borderSprite->setPositionY(-0.5);
    }
    border->ignoreAnchorPointForPosition(false);

    m_mainLayer->addChildAtPosition(border, Anchor::Center, ccp(0, -10.f));

    auto submitBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Submit"),
        [this, settingsContents, callback = std::move(callback)](CCObject* sender) mutable {
            this->onClose(this->m_closeBtn);

            LobbySettings newSettings = SwapManager::createDefaultSettings();

            for (auto cell : CCArrayExt<LobbySettingsCell*>(settingsContents)) {
                cell->save(newSettings);
            }
            callback(newSettings);
        }
    );
    auto menu = CCMenu::create();
    menu->addChild(submitBtn);
    m_mainLayer->addChildAtPosition(menu, Anchor::Bottom);

    return true;
}
