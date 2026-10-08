#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

enum class WaveSize { Big, Mini };
enum class WaveSpeed { Speed05, Speed1x, Speed2x, Speed3x, Speed4x };

class WaveGapCalculator {
public:
    static double getWaveVertical(WaveSize size, WaveSpeed speed) {
        if (size == WaveSize::Big) {
            switch (speed) {
            case WaveSpeed::Speed05: return 251.159;
            case WaveSpeed::Speed1x: return 311.580;
            case WaveSpeed::Speed2x: return 387.421;
            case WaveSpeed::Speed3x: return 468.001;
            case WaveSpeed::Speed4x: return 576.000;
            }
        }
        else {
            switch (speed) {
            case WaveSpeed::Speed05: return 502.321;
            case WaveSpeed::Speed1x: return 623.162;
            case WaveSpeed::Speed2x: return 774.843;
            case WaveSpeed::Speed3x: return 936.004;
            case WaveSpeed::Speed4x: return 1152.006;
            }
        }
        return 311.580;
    }

    static double calculateGap(WaveSize size, WaveSpeed speed, double tps, bool isRecommended) {
        if (tps <= 0.0) return 0.0;

        constexpr double h = 3.0;
        double V = getWaveVertical(size, speed);
        double deltag = V / tps;

        return isRecommended ? (h + deltag * 2.0) : (h + deltag);
    }
};

class WaveGapPopup : public FLAlertLayer {
protected:
    TextInput* m_tpsInput = nullptr;
    CCLabelBMFont* m_resultLabel = nullptr;

    CCMenuItemSpriteExtra* m_bigBtn = nullptr;
    CCMenuItemSpriteExtra* m_miniBtn = nullptr;
    CCMenuItemSpriteExtra* m_smallestBtn = nullptr;
    CCMenuItemSpriteExtra* m_recBtn = nullptr;
    std::array<CCMenuItemSpriteExtra*, 5> m_speedBtns{};

    WaveSize m_selectedSize = WaveSize::Big;
    WaveSpeed m_selectedSpeed = WaveSpeed::Speed1x;
    bool m_isRecommended = false;

    const WaveSpeed m_speedValues[5] = { WaveSpeed::Speed05, WaveSpeed::Speed1x, WaveSpeed::Speed2x, WaveSpeed::Speed3x, WaveSpeed::Speed4x };
    const char* m_speedNames[5] = { "0.5x", "1x", "2x", "3x", "4x" };

    bool init(float width, float height) {
        if (!FLAlertLayer::init(150)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto bg = CCScale9Sprite::create("GJ_square01.png", { 0, 0, 80, 80 });
        bg->setContentSize({ width, height });
        bg->setPosition(winSize / 2.f);
        m_mainLayer->addChild(bg);

        auto closeBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this,
            menu_selector(WaveGapPopup::onClose)
        );
        auto closeMenu = CCMenu::create();
        closeMenu->addChild(closeBtn);
        closeMenu->setPosition({ winSize.width / 2.f - width / 2.f + 14.f, winSize.height / 2.f + height / 2.f - 14.f });
        m_mainLayer->addChild(closeMenu);

        auto title = CCLabelBMFont::create("Wave Gap Calculator", "goldFont.fnt");
        title->setScale(0.6f);
        title->setPosition({ winSize.width / 2.f, winSize.height / 2.f + height / 2.f - 18.f });
        m_mainLayer->addChild(title);

        this->setKeypadEnabled(true);
        this->setTouchEnabled(true);

        double defaultTPS = 240.0;
        if (Mod::get()->hasSetting("default-tps")) {
            defaultTPS = Mod::get()->getSettingValue<double>("default-tps");
        }

        std::string defaultMode = "smallest";
        if (Mod::get()->hasSetting("default-mode")) {
            defaultMode = Mod::get()->getSettingValue<std::string>("default-mode");
        }

        std::string defaultSize = "big";
        if (Mod::get()->hasSetting("default-size")) {
            defaultSize = Mod::get()->getSettingValue<std::string>("default-size");
        }

        m_isRecommended = (defaultMode == "recommended");
        m_selectedSize = (defaultSize == "mini") ? WaveSize::Mini : WaveSize::Big;

        // --- 1. Поле TPS / FPS ---
        auto tpsLabel = CCLabelBMFont::create("TPS / FPS:", "bigFont.fnt");
        tpsLabel->setScale(0.35f);
        tpsLabel->setPosition({ winSize.width / 2.f - 52.f, winSize.height / 2.f + 52.f });
        m_mainLayer->addChild(tpsLabel);

        m_tpsInput = TextInput::create(75.f, "240", "chatFont.fnt");
        m_tpsInput->setString(fmt::format("{:.0f}", defaultTPS));
        m_tpsInput->setFilter("0123456789.");
        m_tpsInput->setPosition({ winSize.width / 2.f + 35.f, winSize.height / 2.f + 52.f });
        m_tpsInput->setCallback([this](const std::string&) {
            this->recalculateAndDisplay();
            });
        m_mainLayer->addChild(m_tpsInput);

        // --- 2. Ряд выбора Size и Mode ---
        auto optionsMenu = CCMenu::create();
        optionsMenu->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 16.f });

        m_bigBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Big", "goldFont.fnt", "GJ_button_01.png", 0.5f),
            this,
            menu_selector(WaveGapPopup::onSelectSizeBig)
        );
        m_miniBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Mini", "goldFont.fnt", "GJ_button_02.png", 0.5f),
            this,
            menu_selector(WaveGapPopup::onSelectSizeMini)
        );
        m_smallestBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Smallest", "goldFont.fnt", "GJ_button_01.png", 0.5f),
            this,
            menu_selector(WaveGapPopup::onSelectSmallest)
        );
        m_recBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Recommended", "goldFont.fnt", "GJ_button_02.png", 0.5f),
            this,
            menu_selector(WaveGapPopup::onSelectRecommended)
        );

        optionsMenu->addChild(m_bigBtn);
        optionsMenu->addChild(m_miniBtn);
        optionsMenu->addChild(m_smallestBtn);
        optionsMenu->addChild(m_recBtn);

        optionsMenu->alignItemsHorizontallyWithPadding(6.f);
        m_mainLayer->addChild(optionsMenu);

        // --- 3. Ряд скоростей ---
        auto speedMenu = CCMenu::create();
        speedMenu->setPosition({ winSize.width / 2.f, winSize.height / 2.f - 18.f });

        for (int i = 0; i < 5; ++i) {
            auto btn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(m_speedNames[i], "goldFont.fnt", "GJ_button_04.png", 0.45f),
                this,
                menu_selector(WaveGapPopup::onSelectSpeed)
            );
            btn->setTag(static_cast<int>(m_speedValues[i]));
            m_speedBtns[i] = btn;
            speedMenu->addChild(btn);
        }
        speedMenu->alignItemsHorizontallyWithPadding(4.f);
        m_mainLayer->addChild(speedMenu);

        // --- 4. Результат ---
        m_resultLabel = CCLabelBMFont::create("Gap: --- units", "bigFont.fnt");
        m_resultLabel->setScale(0.42f);
        m_resultLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f - 52.f });
        m_mainLayer->addChild(m_resultLabel);

        // --- 5. Кнопка установки блоков ---
        auto actionMenu = CCMenu::create();
        actionMenu->setPosition({ winSize.width / 2.f, winSize.height / 2.f - 85.f });

        auto placeBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Place Gap Blocks", "goldFont.fnt", "GJ_button_01.png", 0.7f),
            this,
            menu_selector(WaveGapPopup::onPlaceBlocks)
        );
        actionMenu->addChild(placeBtn);
        m_mainLayer->addChild(actionMenu);

        updateVisuals();
        recalculateAndDisplay();

        return true;
    }

    void updateVisuals() {
        m_bigBtn->setNormalImage(ButtonSprite::create("Big", "goldFont.fnt", m_selectedSize == WaveSize::Big ? "GJ_button_01.png" : "GJ_button_02.png", 0.5f));
        m_miniBtn->setNormalImage(ButtonSprite::create("Mini", "goldFont.fnt", m_selectedSize == WaveSize::Mini ? "GJ_button_01.png" : "GJ_button_02.png", 0.5f));

        m_smallestBtn->setNormalImage(ButtonSprite::create("Smallest", "goldFont.fnt", !m_isRecommended ? "GJ_button_01.png" : "GJ_button_02.png", 0.5f));
        m_recBtn->setNormalImage(ButtonSprite::create("Recommended", "goldFont.fnt", m_isRecommended ? "GJ_button_01.png" : "GJ_button_02.png", 0.5f));

        for (int i = 0; i < 5; ++i) {
            bool isSelected = (m_selectedSpeed == m_speedValues[i]);
            m_speedBtns[i]->setNormalImage(ButtonSprite::create(m_speedNames[i], "goldFont.fnt", isSelected ? "GJ_button_01.png" : "GJ_button_04.png", 0.45f));
        }
    }

    void onClose(CCObject*) {
        this->keyBackClicked();
    }

    void recalculateAndDisplay() {
        std::string tpsStr = m_tpsInput->getString();
        double tps = 240.0;
        try {
            if (!tpsStr.empty()) tps = std::stod(tpsStr);
        }
        catch (...) {}

        double gap = WaveGapCalculator::calculateGap(m_selectedSize, m_selectedSpeed, tps, m_isRecommended);
        std::string modeText = m_isRecommended ? "Rec Gap" : "Min Gap";

        if (m_resultLabel) {
            m_resultLabel->setString(fmt::format("{}: {:.6f} units", modeText, gap).c_str());
        }
    }

    void onSelectSizeBig(CCObject*) {
        m_selectedSize = WaveSize::Big;
        updateVisuals();
        recalculateAndDisplay();
    }

    void onSelectSizeMini(CCObject*) {
        m_selectedSize = WaveSize::Mini;
        updateVisuals();
        recalculateAndDisplay();
    }

    void onSelectSpeed(CCObject* sender) {
        if (auto btn = static_cast<CCNode*>(sender)) {
            m_selectedSpeed = static_cast<WaveSpeed>(btn->getTag());
            updateVisuals();
            recalculateAndDisplay();
        }
    }

    void onSelectSmallest(CCObject*) {
        m_isRecommended = false;
        updateVisuals();
        recalculateAndDisplay();
    }

    void onSelectRecommended(CCObject*) {
        m_isRecommended = true;
        updateVisuals();
        recalculateAndDisplay();
    }

    void onPlaceBlocks(CCObject*) {
        std::string tpsVal = m_tpsInput->getString();
        double tps = 240.0;
        try {
            if (!tpsVal.empty()) tps = std::stod(tpsVal);
        }
        catch (...) {}

        double gap = WaveGapCalculator::calculateGap(m_selectedSize, m_selectedSpeed, tps, m_isRecommended);

        auto editor = LevelEditorLayer::get();
        if (!editor) {
            this->onClose(nullptr);
            return;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint screenCenter = winSize / 2.f;
        CCPoint worldPos = editor->m_objectLayer->convertToNodeSpace(screenCenter);

        float centerX = std::floor(worldPos.x / 30.f) * 30.f + 15.f;
        float centerY = std::floor(worldPos.y / 30.f) * 30.f + 15.f;

        CCPoint bottomPos = { centerX, centerY - 15.f };
        auto bottomBlock = editor->createObject(1, bottomPos, true);

        CCPoint topPos = { centerX, bottomPos.y + 30.f + static_cast<float>(gap) };
        auto topBlock = editor->createObject(1, topPos, true);

        if (auto editorUI = EditorUI::get()) {
            editorUI->deselectAll();

            auto selectedArr = CCArray::create();
            if (bottomBlock) selectedArr->addObject(bottomBlock);
            if (topBlock) selectedArr->addObject(topBlock);

            if (selectedArr->count() > 0) {
                editorUI->selectObjects(selectedArr, true);
            }
        }

        this->onClose(nullptr);
    }

public:
    static WaveGapPopup* create() {
        auto ret = new WaveGapPopup();
        if (ret && ret->init(330.f, 230.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer * editor) {
        if (!EditorUI::init(editor)) return false;

        CCNode* btnSprite = nullptr;

        if (auto customLogo = CCSprite::create("logo.png"_spr)) {
            customLogo->setScale(30.f / customLogo->getContentSize().width);
            btnSprite = customLogo;
        }
        else {
            btnSprite = ButtonSprite::create("WGC", 30, true, "goldFont.fnt", "GJ_button_01.png", 30.f, 0.35f);
        }

        auto button = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(MyEditorUI::onOpenWaveGapPopup)
        );
        button->setID("wave-gap-calculator-button"_spr);

        CCNode* refButton = this->getChildByIDRecursive("view-groups-button");

        if (refButton && refButton->getParent()) {
            auto gridMenu = refButton->getParent();
            gridMenu->addChild(button);
            button->setPosition({
                refButton->getPositionX(),
                refButton->getPositionY() - 40.f
                });
        }
        else {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            auto customMenu = CCMenu::create();
            customMenu->setID("wave-gap-calculator-menu"_spr);
            customMenu->addChild(button);

            customMenu->setPosition({ winSize.width - 114.f, winSize.height / 2.f + 62.f });
            this->addChild(customMenu, 100);
        }

        return true;
    }

    void onOpenWaveGapPopup(CCObject*) {
        WaveGapPopup::create()->show();
    }
};