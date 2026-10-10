#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <array>
#include <cmath>
#include <string>
#include <vector>

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

// ---------------------------------------------------------------------------
// Popup
// Uses geode::Popup (Geode 5, not templated) instead of a raw FLAlertLayer.
// Popup takes care of touch priority, the dark overlay, the close button and
// the back button, which is what was breaking on Android.
// All clickable buttons live in m_buttonMenu so they share Popup's priority.
// ---------------------------------------------------------------------------
class WaveGapPopup : public Popup {
protected:
    static constexpr const char* BG_ON = "GJ_button_01.png";
    static constexpr const char* BG_OFF = "GJ_button_02.png";
    static constexpr const char* BG_SPEED_OFF = "GJ_button_04.png";

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

    static constexpr std::array<WaveSpeed, 5> m_speedValues = {
        WaveSpeed::Speed05, WaveSpeed::Speed1x, WaveSpeed::Speed2x, WaveSpeed::Speed3x, WaveSpeed::Speed4x
    };
    static constexpr std::array<const char*, 5> m_speedNames = { "0.5x", "1x", "2x", "3x", "4x" };

    // Creates a button with a fixed pixel width so rows can be laid out by hand.
    CCMenuItemSpriteExtra* makeButton(char const* text, int width, char const* bg, float textScale, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create(text, width, true, "goldFont.fnt", bg, 30.f, textScale);
        return CCMenuItemSpriteExtra::create(spr, this, cb);
    }

    // Places buttons in a centered horizontal row inside m_buttonMenu.
    void layoutRow(std::vector<CCMenuItemSpriteExtra*> const& btns, std::vector<float> const& widths, float y, float gap) {
        float total = gap * static_cast<float>(btns.size() - 1);
        for (float w : widths) total += w;

        float x = -total / 2.f;
        for (size_t i = 0; i < btns.size(); ++i) {
            m_buttonMenu->addChildAtPosition(btns[i], Anchor::Center, { x + widths[i] / 2.f, y });
            x += widths[i] + gap;
        }
    }

    static void setBtnBG(CCMenuItemSpriteExtra* btn, char const* bg) {
        if (!btn) return;
        if (auto spr = typeinfo_cast<ButtonSprite*>(btn->getNormalImage())) {
            spr->updateBGImage(bg);
        }
    }

    bool init() {
        if (!Popup::init(330.f, 230.f)) return false;

        this->setTitle("Wave Gap Calculator");

        // --- Settings ---
        double defaultTPS = Mod::get()->getSettingValue<double>("default-tps");
        std::string defaultMode = Mod::get()->getSettingValue<std::string>("default-mode");
        std::string defaultSize = Mod::get()->getSettingValue<std::string>("default-size");

        m_isRecommended = (defaultMode == "recommended");
        m_selectedSize = (defaultSize == "mini") ? WaveSize::Mini : WaveSize::Big;

        // --- 1. TPS / FPS field ---
        auto tpsLabel = CCLabelBMFont::create("TPS / FPS:", "bigFont.fnt");
        tpsLabel->setScale(0.35f);
        m_mainLayer->addChildAtPosition(tpsLabel, Anchor::Center, { -52.f, 52.f });

        m_tpsInput = TextInput::create(75.f, "240", "chatFont.fnt");
        m_tpsInput->setString(fmt::format("{:.0f}", defaultTPS));
        m_tpsInput->setFilter("0123456789.");
        m_tpsInput->setCallback([this](std::string const&) {
            this->recalculateAndDisplay();
            });
        m_mainLayer->addChildAtPosition(m_tpsInput, Anchor::Center, { 35.f, 52.f });

        // --- 2. Size / Mode row ---
        m_bigBtn = makeButton("Big", 40, BG_ON, 0.5f, menu_selector(WaveGapPopup::onSelectSizeBig));
        m_miniBtn = makeButton("Mini", 40, BG_OFF, 0.5f, menu_selector(WaveGapPopup::onSelectSizeMini));
        m_smallestBtn = makeButton("Smallest", 70, BG_ON, 0.5f, menu_selector(WaveGapPopup::onSelectSmallest));
        m_recBtn = makeButton("Recommended", 110, BG_OFF, 0.5f, menu_selector(WaveGapPopup::onSelectRecommended));

        layoutRow(
            { m_bigBtn, m_miniBtn, m_smallestBtn, m_recBtn },
            { 40.f, 40.f, 70.f, 110.f },
            16.f, 6.f
        );

        // --- 3. Speed row ---
        std::vector<CCMenuItemSpriteExtra*> speedBtns;
        std::vector<float> speedWidths;
        for (int i = 0; i < 5; ++i) {
            auto btn = makeButton(m_speedNames[i], 45, BG_SPEED_OFF, 0.45f, menu_selector(WaveGapPopup::onSelectSpeed));
            btn->setTag(i); // index into m_speedValues
            m_speedBtns[i] = btn;
            speedBtns.push_back(btn);
            speedWidths.push_back(45.f);
        }
        layoutRow(speedBtns, speedWidths, -18.f, 4.f);

        // --- 4. Result ---
        m_resultLabel = CCLabelBMFont::create("Gap: --- units", "bigFont.fnt");
        m_resultLabel->setScale(0.42f);
        m_mainLayer->addChildAtPosition(m_resultLabel, Anchor::Center, { 0.f, -52.f });

        // --- 5. Place button ---
        auto placeBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Place Gap Blocks", "goldFont.fnt", BG_ON, 0.7f),
            this,
            menu_selector(WaveGapPopup::onPlaceBlocks)
        );
        m_buttonMenu->addChildAtPosition(placeBtn, Anchor::Center, { 0.f, -85.f });

        updateVisuals();
        recalculateAndDisplay();

        return true;
    }

    void updateVisuals() {
        setBtnBG(m_bigBtn, m_selectedSize == WaveSize::Big ? BG_ON : BG_OFF);
        setBtnBG(m_miniBtn, m_selectedSize == WaveSize::Mini ? BG_ON : BG_OFF);
        setBtnBG(m_smallestBtn, !m_isRecommended ? BG_ON : BG_OFF);
        setBtnBG(m_recBtn, m_isRecommended ? BG_ON : BG_OFF);

        for (int i = 0; i < 5; ++i) {
            setBtnBG(m_speedBtns[i], m_selectedSpeed == m_speedValues[i] ? BG_ON : BG_SPEED_OFF);
        }
    }

    double getTPS() {
        double tps = 240.0;
        std::string s = m_tpsInput->getString();
        try {
            if (!s.empty()) tps = std::stod(s);
        }
        catch (...) {}
        return tps;
    }

    void recalculateAndDisplay() {
        double gap = WaveGapCalculator::calculateGap(m_selectedSize, m_selectedSpeed, getTPS(), m_isRecommended);
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
        auto btn = static_cast<CCNode*>(sender);
        int idx = btn->getTag();
        if (idx < 0 || idx >= 5) return;

        m_selectedSpeed = m_speedValues[idx];
        updateVisuals();
        recalculateAndDisplay();
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
        double gap = WaveGapCalculator::calculateGap(m_selectedSize, m_selectedSpeed, getTPS(), m_isRecommended);

        auto editor = LevelEditorLayer::get();
        if (!editor) {
            this->onClose(nullptr);
            return;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint worldPos = editor->m_objectLayer->convertToNodeSpace(winSize / 2.f);

        float centerX = std::floor(worldPos.x / 30.f) * 30.f + 15.f;
        float centerY = std::floor(worldPos.y / 30.f) * 30.f + 15.f;

        CCPoint bottomPos = { centerX, centerY - 15.f };
        auto bottomBlock = editor->createObject(1, bottomPos, true);

        CCPoint topPos = { centerX, bottomPos.y + 30.f + static_cast<float>(gap) };
        auto topBlock = editor->createObject(1, topPos, true);

        if (Mod::get()->getSettingValue<bool>("auto-select-placed")) {
            if (auto editorUI = EditorUI::get()) {
                editorUI->deselectAll();

                auto selectedArr = CCArray::create();
                if (bottomBlock) selectedArr->addObject(bottomBlock);
                if (topBlock) selectedArr->addObject(topBlock);

                if (selectedArr->count() > 0) {
                    editorUI->selectObjects(selectedArr, true);
                }
            }
        }

        this->onClose(nullptr);
    }

public:
    static WaveGapPopup* create() {
        auto ret = new WaveGapPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Editor button
// Lives in its own CCMenu on EditorUI, so editor layout menus can't move it
// or swallow its touches. It is positioned relative to an existing editor
// button when that exists, with a fixed fallback otherwise.
// ---------------------------------------------------------------------------
class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer * editor) {
        if (!EditorUI::init(editor)) return false;

        // NOTE: "logo.png"_spr only works if the file is listed under
        // "resources" -> "sprites" in mod.json. Your mod.json lists
        // resources/icon.png, so we try that and fall back to a text button.
        CCNode* btnSprite = nullptr;
        if (auto icon = CCSprite::create("icon.png"_spr)) {
            float maxSide = std::max(icon->getContentSize().width, icon->getContentSize().height);
            if (maxSide > 0.f) icon->setScale(30.f / maxSide);
            btnSprite = icon;
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

        auto menu = CCMenu::create();
        menu->setID("wave-gap-calculator-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(button);
        this->addChild(menu, 100);

        auto winSize = CCDirector::get()->getWinSize();
        CCPoint pos = { winSize.width - 114.f, winSize.height / 2.f + 62.f }; // fallback

        if (auto ref = this->getChildByIDRecursive("view-groups-button")) {
            if (auto parent = ref->getParent()) {
                CCPoint world = parent->convertToWorldSpace(ref->getPosition());
                CCPoint local = this->convertToNodeSpace(world);
                pos = ccp(local.x, local.y - 40.f);
            }
        }
        button->setPosition(pos);

        return true;
    }

    void onOpenWaveGapPopup(CCObject*) {
        if (auto popup = WaveGapPopup::create()) {
            popup->show();
        }
    }
};