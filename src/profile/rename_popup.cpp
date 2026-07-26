#include "rename_popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

namespace profile {

    bool RenamePopup::init(std::string current, geode::Function<void(std::string)> onAccept) {
        if (!Popup::init(280.f, 130.f, "GJ_square01.png")) {
            return false;
        }
        m_onAccept = std::move(onAccept);

        this->setTitle("Rename Profile");
        this->setID("profile-rename-popup"_spr);

        m_input = TextInput::create(220.f, "Profile name", "chatFont.fnt");
        m_input->setMaxCharCount(32);
        m_input->setFilter(
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            " _-.,!?'()"
        );
        m_input->setString(std::move(current));
        m_input->setID("profile-rename-input"_spr);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, ccp(0.f, 5.f));

        auto cancelSpr = ButtonSprite::create("Cancel", "goldFont.fnt", "GJ_button_06.png", .8f);
        cancelSpr->setScale(.7f);
        auto cancelBtn = cocos::CCMenuItemExt::createSpriteExtra(cancelSpr, [this](auto*) {
            this->Popup::onClose(nullptr);
        });
        cancelBtn->setID("profile-rename-cancel"_spr);
        m_buttonMenu->addChildAtPosition(cancelBtn, Anchor::Bottom, ccp(-50.f, 30.f));

        auto saveSpr = ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", .8f);
        saveSpr->setScale(.7f);
        auto saveBtn = cocos::CCMenuItemExt::createSpriteExtra(saveSpr, [this](auto*) {
            this->onAccept();
        });
        saveBtn->setID("profile-rename-apply"_spr);
        m_buttonMenu->addChildAtPosition(saveBtn, Anchor::Bottom, ccp(50.f, 30.f));

        return true;
    }

    void RenamePopup::onAccept() {
        if (!m_input) return;
        std::string const value = m_input->getString();
        if (m_onAccept) {
            m_onAccept(value);
        }
        this->Popup::onClose(nullptr);
    }

    RenamePopup* RenamePopup::create(std::string current, geode::Function<void(std::string)> onAccept) {
        auto* ret = new RenamePopup();
        if (ret->init(std::move(current), std::move(onAccept))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

} // namespace profile
