#include "setting.hpp"

#include "popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/JsonValidation.hpp>

using namespace geode::prelude;

namespace profile {

namespace {

class ProfileManagerSettingNode final : public SettingNodeV3 {
protected:
    CCMenuItemSpriteExtra* m_button = nullptr;

    bool init(
        std::shared_ptr<ProfileManagerSettingV3> setting,
        float width
    ) {
        if (!SettingNodeV3::init(setting, width)) return false;

        auto spr = ButtonSprite::create(
            "Manage Profiles",
            "bigFont.fnt",
            "GJ_button_01.png",
            .8f
        );
        spr->setScale(.6f);
        m_button = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(ProfileManagerSettingNode::onManage)
        );
        auto* menu = this->getButtonMenu();
        menu->addChildAtPosition(m_button, Anchor::Center);
        menu->updateLayout();

        this->updateState(nullptr);
        return true;
    }

    void onManage(CCObject*) {
        ProfileManagerPopup::create()->show();
    }

    void updateState(CCNode* invoker) override {
        SettingNodeV3::updateState(invoker);
        if (m_button) {
            m_button->setEnabled(this->getSetting()->shouldEnable());
        }
    }

    void onCommit() override {}
    void onResetToDefault() override {}

public:
    bool hasUncommittedChanges() const override { return false; }
    bool hasNonDefaultValue() const override { return false; }

    static ProfileManagerSettingNode* create(
        std::shared_ptr<ProfileManagerSettingV3> setting,
        float width
    ) {
        auto ret = new ProfileManagerSettingNode();
        if (ret->init(setting, width)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

} // namespace

Result<std::shared_ptr<SettingV3>> ProfileManagerSettingV3::parse(
    std::string key,
    std::string modID,
    matjson::Value const& json
) {
    auto ret = std::make_shared<ProfileManagerSettingV3>();
    auto root = checkJson(json, "ProfileManagerSettingV3");
    ret->parseBaseProperties(key, modID, root);
    root.checkUnknownKeys();
    return root.ok(std::static_pointer_cast<SettingV3>(ret));
}

bool ProfileManagerSettingV3::load(matjson::Value const&) {
    return true;
}

bool ProfileManagerSettingV3::save(matjson::Value&) const {
    return true;
}

SettingNodeV3* ProfileManagerSettingV3::createNode(float width) {
    return ProfileManagerSettingNode::create(
        std::static_pointer_cast<ProfileManagerSettingV3>(
            shared_from_this()
        ),
        width
    );
}

bool ProfileManagerSettingV3::isDefaultValue() const {
    return true;
}

void ProfileManagerSettingV3::reset() {}

} // namespace profile
