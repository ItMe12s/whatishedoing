#include "open_file_setting.hpp"

#include "custom_text.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/file.hpp>

#ifdef GEODE_IS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include "state.hpp"

using namespace geode::prelude;

namespace level_upload {

class OpenFileButtonSettingV3 : public SettingV3 {
public:
    static Result<std::shared_ptr<SettingV3>> parse(
        std::string const& key,
        std::string const& modID,
        matjson::Value const& json
    ) {
        auto res = std::make_shared<OpenFileButtonSettingV3>();
        auto root = checkJson(json, "OpenFileButtonSettingV3");
        res->init(key, modID, root);
        res->parseNameAndDescription(root);
        root.checkUnknownKeys();
        return root.ok(std::static_pointer_cast<SettingV3>(std::move(res)));
    }

    bool load(matjson::Value const&) override { return true; }
    bool save(matjson::Value&) const override  { return true; }
    bool isDefaultValue() const override        { return true; }
    void reset() override {}

    SettingNodeV3* createNode(float width) override;
};

class OpenFileButtonSettingNodeV3 : public SettingNodeV3 {
protected:
    ButtonSprite* m_buttonSprite;
    CCMenuItemSpriteExtra* m_button;

    bool init(std::shared_ptr<OpenFileButtonSettingV3> setting, float width) {
        if (!SettingNodeV3::init(setting, width)) return false;

        m_buttonSprite = ButtonSprite::create("Edit File", "goldFont.fnt", "GJ_button_01.png", .8f);
        m_buttonSprite->setScale(.65f);
        m_button = CCMenuItemSpriteExtra::create(
            m_buttonSprite, this,
            menu_selector(OpenFileButtonSettingNodeV3::onButton)
        );
        this->getButtonMenu()->addChildAtPosition(m_button, Anchor::Center);
        this->getButtonMenu()->setContentWidth(80);
        this->getButtonMenu()->updateLayout();
        this->updateState(nullptr);
        return true;
    }

    void updateState(CCNode* invoker) override {
        SettingNodeV3::updateState(invoker);
        auto ok = this->getSetting()->shouldEnable();
        m_button->setEnabled(ok);
        m_buttonSprite->setCascadeColorEnabled(true);
        m_buttonSprite->setCascadeOpacityEnabled(true);
        m_buttonSprite->setOpacity(ok ? 255 : 155);
        m_buttonSprite->setColor(ok ? ccWHITE : ccGRAY);
    }

    void onButton(CCObject*) {
        ensureDefaultCustomTextFile();
        auto path = customTextFilePath();
#ifdef GEODE_IS_WINDOWS
        ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOW);
#else
        utils::file::openFolder(Mod::get()->getConfigDir());
#endif
        auto& session = gameSession();
        if (session.eventCount < session.trackedEvents.size()) {
            session.trackedEvents[session.eventCount++] = Tracked::CustomTextEdit;
        }
    }

    void onCommit() override {}
    void onResetToDefault() override {}

public:
    static OpenFileButtonSettingNodeV3* create(
        std::shared_ptr<OpenFileButtonSettingV3> setting, float width
    ) {
        auto ret = new OpenFileButtonSettingNodeV3();
        if (ret->init(setting, width)) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }

    bool hasUncommittedChanges() const override { return false; }
    bool hasNonDefaultValue() const override { return false; }

    std::shared_ptr<OpenFileButtonSettingV3> getSetting() const {
        return std::static_pointer_cast<OpenFileButtonSettingV3>(SettingNodeV3::getSetting());
    }
};

SettingNodeV3* OpenFileButtonSettingV3::createNode(float width) {
    return OpenFileButtonSettingNodeV3::create(
        std::static_pointer_cast<OpenFileButtonSettingV3>(shared_from_this()), width
    );
}

void registerCustomSettings() {
    (void)Mod::get()->registerCustomSettingType("open-file-button", &OpenFileButtonSettingV3::parse);
}

} // namespace level_upload
