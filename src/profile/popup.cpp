#include "popup.hpp"

#include "data.hpp"
#include "rename_popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/cocos.hpp>
#include <string>

using namespace geode::prelude;

namespace profile {

    namespace {

        constexpr float kPopupWidth = 380.f;
        constexpr float kPopupHeight = 290.f;
        constexpr float kRowHeight = 22.f;
        constexpr float kButtonRowGap = 6.f;
        constexpr float kActionsMenuRightPadding = 10.f;
        constexpr float kNameLabelMaxWidth = 110.f;
        constexpr float kNameLabelScale = .45f;
        constexpr float kNameLabelMinScale = .05f;

        void styleLoadDeleteButton(CCMenuItemSpriteExtra* btn, bool slotFilled) {
            if (!btn) return;
            btn->setEnabled(slotFilled);
            GLubyte const alpha = slotFilled ? 255 : 128;
            if (auto* spr = typeinfo_cast<CCSprite*>(btn->getNormalImage())) {
                spr->setCascadeOpacityEnabled(true);
                spr->setOpacity(alpha);
            }
            btn->setCascadeOpacityEnabled(true);
            btn->setOpacity(alpha);
        }

        std::string profileNodeId(std::string const& local) {
            return Mod::get()->expandSpriteName(local);
        }

        CCMenuItemSpriteExtra* makeActionButton(
            std::size_t index, char const* label, char const* background, char const* idSuffix,
            geode::Function<void(CCMenuItemSpriteExtra*)> callback
        ) {
            auto* sprite = ButtonSprite::create(label, "bigFont.fnt", background, .8f);
            sprite->setScale(.45f);
            auto* button = cocos::CCMenuItemExt::createSpriteExtra(sprite, std::move(callback));
            button->setID(profileNodeId(fmt::format("profile-slot-{}-{}", index, idSuffix)));
            return button;
        }

        geode::Popup* findGeodeBaseSettingsPopup(cocos2d::CCScene* scene) {
            if (!scene) return nullptr;

            return cocos::findFirstChildRecursive<geode::Popup>(scene, [](geode::Popup* popup) {
                return popup->getChildByIDRecursive("search-input");
            });
        }

        // Popup::onClose is protected, so mirror its teardown for the settings popup.
        void closeGeodePopupLikePopup(geode::Popup* p) {
            if (!p) return;
            geode::Popup::CloseEvent(p).send();
            p->setKeypadEnabled(false);
            p->setTouchEnabled(false);
            p->removeFromParent();
        }

    } // namespace

    cocos2d::CCNode* ProfileManagerPopup::makeSlotRow(std::size_t idx, float width) {
        auto const slot = slotNameAt(idx);

        auto* row = CCNode::create();
        row->setContentSize({width, kRowHeight});
        row->setID(profileNodeId(fmt::format("profile-slot-row-{}", idx)));

        auto* label = CCLabelBMFont::create(slot.c_str(), "bigFont.fnt");
        label->setScale(kNameLabelScale);
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({4.f, kRowHeight * .5f});
        label->setID(profileNodeId(fmt::format("profile-slot-{}-name", idx)));
        row->addChild(label);

        auto* status = CCLabelBMFont::create("", "chatFont.fnt");
        status->setScale(.55f);
        status->setAnchorPoint({0.f, .5f});
        status->setPosition({120.f, kRowHeight * .5f});
        status->setID(profileNodeId(fmt::format("profile-slot-{}-status", idx)));
        row->addChild(status);

        auto* menu = CCMenu::create();
        menu->ignoreAnchorPointForPosition(false);
        menu->setContentSize({width, kRowHeight});
        menu->setAnchorPoint({0.f, 0.f});
        menu->setPosition({0.f, 0.f});
        menu->setLayout(
            RowLayout::create()
                ->setGap(kButtonRowGap)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisAlignment(AxisAlignment::Center)
                ->setPadding(Padding(0.f, 0.f, kActionsMenuRightPadding, 0.f))
        );
        menu->setID(profileNodeId(fmt::format("profile-slot-{}-actions", idx)));
        row->addChild(menu);

        menu->addChild(makeActionButton(idx, "Rename", "GJ_button_04.png", "rename", [this, idx](auto*) {
            this->onRenameSlot(idx);
        }));
        menu->addChild(makeActionButton(idx, "Delete", "GJ_button_06.png", "delete", [this, idx](auto*) {
            this->onClearSlot(idx);
        }));
        menu->addChild(makeActionButton(idx, "Save", "GJ_button_05.png", "save", [this, idx](auto*) {
            this->onSaveSlot(idx);
        }));
        menu->addChild(makeActionButton(idx, "Load", "GJ_button_01.png", "load", [this, idx](auto*) {
            this->onLoadSlot(idx);
        }));

        menu->updateLayout();

        refreshRow(row, idx);
        return row;
    }

    void ProfileManagerPopup::refreshRow(cocos2d::CCNode* row, std::size_t idx) {
        if (!row) return;
        auto const slot = slotNameAt(idx);
        bool const filled = slotIsFilled(slot);

        if (auto* label = typeinfo_cast<CCLabelBMFont*>(
                row->getChildByID(profileNodeId(fmt::format("profile-slot-{}-name", idx)))
            )) {
            label->setString(slot.c_str());
            label->limitLabelWidth(kNameLabelMaxWidth, kNameLabelScale, kNameLabelMinScale);
        }
        if (auto* status = typeinfo_cast<CCLabelBMFont*>(
                row->getChildByID(profileNodeId(fmt::format("profile-slot-{}-status", idx)))
            )) {
            status->setString(filled ? "saved" : "empty");
            status->setColor(filled ? ccc3(120, 220, 120) : ccc3(180, 180, 180));
        }
        auto* menu = typeinfo_cast<CCMenu*>(
            row->getChildByID(profileNodeId(fmt::format("profile-slot-{}-actions", idx)))
        );
        styleLoadDeleteButton(
            typeinfo_cast<CCMenuItemSpriteExtra*>(
                menu ? menu->getChildByID(profileNodeId(fmt::format("profile-slot-{}-delete", idx))) :
                       nullptr
            ),
            filled
        );
        styleLoadDeleteButton(
            typeinfo_cast<CCMenuItemSpriteExtra*>(
                menu ? menu->getChildByID(profileNodeId(fmt::format("profile-slot-{}-load", idx))) :
                       nullptr
            ),
            filled
        );
    }

    bool ProfileManagerPopup::init() {
        if (!Popup::init(kPopupWidth, kPopupHeight, "GJ_square01.png")) {
            return false;
        }
        this->setTitle("Profile Manager");
        this->setID("profile-manager-popup"_spr);

        float const innerWidth = kPopupWidth - 30.f;
        float const listH = kRowHeight * kSlotCount + 8.f;

        auto* listBg = CCLayerColor::create({0, 0, 0, 75});
        listBg->setID("profile-manager-list-bg"_spr);
        listBg->setContentSize({innerWidth, listH});
        listBg->ignoreAnchorPointForPosition(false);
        listBg->setAnchorPoint({.5f, .5f});
        m_mainLayer->addChildAtPosition(listBg, Anchor::Center, ccp(0.f, -14.f));

        auto* hint = CCLabelBMFont::create(
            "Save: snapshot of last-applied settings. | "
            "Load: close this menu and apply your settings.",
            "chatFont.fnt"
        );
        hint->setScale(.55f);
        hint->setColor(ccc3(180, 180, 180));
        hint->setID("profile-manager-hint"_spr);
        m_mainLayer->addChildAtPosition(hint, Anchor::Center, ccp(0.f, listH * .5f - 6.f));

        float y = listH - kRowHeight * .5f - 2.f;
        for (std::size_t i = 0; i < kSlotCount; ++i) {
            auto* row = this->makeSlotRow(i, innerWidth);
            row->setAnchorPoint({.5f, .5f});
            row->setPosition({innerWidth * .5f, y});
            listBg->addChild(row);
            y -= kRowHeight;
        }
        return true;
    }

    namespace {

        CCNode* findSlotRowForIndex(ProfileManagerPopup* popup, std::size_t idx) {
            if (!popup) {
                return nullptr;
            }
            auto const wantId = profileNodeId(fmt::format("profile-slot-row-{}", idx));
            return popup->getChildByIDRecursive(wantId);
        }

    } // namespace

    void ProfileManagerPopup::onSaveSlot(std::size_t idx) {
        auto const slot = slotNameAt(idx);
        bool const hadData = slotIsFilled(slot);

        Ref<ProfileManagerPopup> popup(this);
        createQuickPopup(
            "Save Profile",
            hadData ? fmt::format(
                          "Overwrite saved data in <cy>{}</c> with a snapshot of "
                          "your current (last-applied) settings?",
                          slot
                      ) :
                      fmt::format("Save a snapshot of your current settings to <cy>{}</c>?", slot),
            "Cancel",
            "Save",
            [popup, idx, slot](FLAlertLayer*, bool ok) {
                if (!ok) return;
                snapshotIntoSlot(slot);
                if (auto* row = findSlotRowForIndex(popup, idx)) {
                    popup->refreshRow(row, idx);
                }
                Notification::create(fmt::format("Saved to {}", slot), NotificationIcon::Success, 1.5f)
                    ->show();
            }
        );
    }

    void ProfileManagerPopup::onLoadSlot(std::size_t idx) {
        auto const slot = slotNameAt(idx);
        if (!slotIsFilled(slot)) return;

        Ref<ProfileManagerPopup> popup(this);
        createQuickPopup(
            "Load Profile",
            fmt::format(
                "Load <cy>{}</c> now? The settings page will close so the new "
                "values can be applied cleanly.",
                slot
            ),
            "Cancel",
            "Load",
            [slot, popup, idx](FLAlertLayer*, bool ok) {
                if (!ok) return;
                if (!applyProfileNow(slot)) {
                    Notification::create("Profile load failed", NotificationIcon::Error, 2.f)->show();
                    return;
                }
                setActiveCustomTextSlotIndex(idx);
                auto* scene = CCDirector::sharedDirector()->getRunningScene();
                if (auto* settings = findGeodeBaseSettingsPopup(scene)) {
                    closeGeodePopupLikePopup(settings);
                }
                geode::queueInMainThread([slot]() {
                    Notification::create(fmt::format("Loaded {}", slot), NotificationIcon::Success, 1.5f)
                        ->show();
                });
                popup->Popup::onClose(nullptr);
            }
        );
    }

    void ProfileManagerPopup::onClearSlot(std::size_t idx) {
        auto const slot = slotNameAt(idx);
        if (!slotIsFilled(slot)) return;

        Ref<ProfileManagerPopup> popup(this);
        createQuickPopup(
            "Delete Profile",
            fmt::format(
                "Permanently clear saved profile <cy>{}</c>? This cannot be "
                "undone.",
                slot
            ),
            "Cancel",
            "Delete",
            [popup, idx, slot](FLAlertLayer*, bool ok) {
                if (!ok) return;
                clearSlot(slot);
                if (activeCustomTextSlotIndex() == idx) {
                    setActiveCustomTextSlotIndex(0);
                }
                if (auto* row = findSlotRowForIndex(popup, idx)) {
                    popup->refreshRow(row, idx);
                }
                Notification::create(fmt::format("Cleared {}", slot), NotificationIcon::Info, 1.5f)
                    ->show();
            }
        );
    }

    void ProfileManagerPopup::onRenameSlot(std::size_t idx) {
        auto current = slotNameAt(idx);

        if (auto* rename = RenamePopup::create(
                std::move(current), [popup = Ref<ProfileManagerPopup>(this), idx](std::string newName) {
                    auto res = renameSlot(idx, std::move(newName));
                    if (res.isErr()) {
                        Notification::create(res.unwrapErr(), NotificationIcon::Error, 2.f)->show();
                        return;
                    }
                    if (auto* row = findSlotRowForIndex(popup, idx)) {
                        popup->refreshRow(row, idx);
                    }
                }
            )) {
            rename->show();
        }
    }

    ProfileManagerPopup* ProfileManagerPopup::create() {
        auto* ret = new ProfileManagerPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

} // namespace profile
