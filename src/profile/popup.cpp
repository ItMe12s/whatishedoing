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
#include <Geode/utils/string.hpp>
#include <optional>
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

        constexpr int kNameLabelTag = 1000;
        constexpr int kStatusTag = 1001;
        constexpr int kLoadButtonTag = 1002;
        constexpr int kSaveButtonTag = 1003;
        constexpr int kClearButtonTag = 1004;
        constexpr int kRenameButtonTag = 1005;

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

        CCMenuItemSpriteExtra* findItemByTag(CCMenu* menu, int tag) {
            if (!menu) return nullptr;
            return typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByTag(tag));
        }

        CCMenu* findMenu(CCNode* row) {
            if (!row || !row->getChildren()) return nullptr;
            return typeinfo_cast<CCMenu*>(row->getChildren()->lastObject());
        }

        std::string profileNodeId(std::string const& local) {
            return fmt::format("{}/{}", Mod::get()->getID(), local);
        }

        CCMenuItemSpriteExtra* makeActionButton(
            ProfileManagerPopup* target, std::size_t index, char const* label,
            char const* background, cocos2d::SEL_MenuHandler callback, int tag, char const* idSuffix
        ) {
            auto* sprite = ButtonSprite::create(label, "bigFont.fnt", background, .8f);
            sprite->setScale(.45f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, target, callback);
            button->setUserObject(CCInteger::create(static_cast<int>(index)));
            button->setTag(tag);
            button->setID(profileNodeId(fmt::format("profile-slot-{}-{}", index, idSuffix)));
            return button;
        }

        cocos2d::CCNode* findGeodeSettingSearchInputDescendant(cocos2d::CCNode* root) {
            return cocos::findFirstChildRecursive<CCNode>(root, [](CCNode* c) {
                return string::contains(std::string_view(c->getID()), "search-input");
            });
        }

        geode::Popup* findGeodeBaseSettingsPopup(cocos2d::CCScene* scene) {
            if (!scene) return nullptr;

            return cocos::findFirstChildRecursive<geode::Popup>(scene, [](geode::Popup* popup) {
                return findGeodeSettingSearchInputDescendant(popup);
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
        row->setUserObject(CCInteger::create(static_cast<int>(idx)));
        row->setID(profileNodeId(fmt::format("profile-slot-row-{}", idx)));

        auto* label = CCLabelBMFont::create(slot.c_str(), "bigFont.fnt");
        label->setScale(kNameLabelScale);
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({4.f, kRowHeight * .5f});
        label->setTag(kNameLabelTag);
        label->setID(profileNodeId(fmt::format("profile-slot-{}-name", idx)));
        row->addChild(label);

        auto* status = CCLabelBMFont::create("", "chatFont.fnt");
        status->setScale(.55f);
        status->setAnchorPoint({0.f, .5f});
        status->setPosition({120.f, kRowHeight * .5f});
        status->setTag(kStatusTag);
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

        menu->addChild(makeActionButton(
            this,
            idx,
            "Rename",
            "GJ_button_04.png",
            menu_selector(ProfileManagerPopup::onRenameSlot),
            kRenameButtonTag,
            "rename"
        ));
        menu->addChild(makeActionButton(
            this,
            idx,
            "Delete",
            "GJ_button_06.png",
            menu_selector(ProfileManagerPopup::onClearSlot),
            kClearButtonTag,
            "delete"
        ));
        menu->addChild(makeActionButton(
            this,
            idx,
            "Save",
            "GJ_button_05.png",
            menu_selector(ProfileManagerPopup::onSaveSlot),
            kSaveButtonTag,
            "save"
        ));
        menu->addChild(makeActionButton(
            this,
            idx,
            "Load",
            "GJ_button_01.png",
            menu_selector(ProfileManagerPopup::onLoadSlot),
            kLoadButtonTag,
            "load"
        ));

        menu->updateLayout();

        refreshRow(row, idx);
        return row;
    }

    void ProfileManagerPopup::refreshRow(cocos2d::CCNode* row, std::size_t idx) {
        if (!row) return;
        auto const slot = slotNameAt(idx);
        bool const filled = slotIsFilled(slot);

        if (auto* label = typeinfo_cast<CCLabelBMFont*>(row->getChildByTag(kNameLabelTag))) {
            label->setString(slot.c_str());
            label->limitLabelWidth(kNameLabelMaxWidth, kNameLabelScale, kNameLabelMinScale);
        }
        if (auto* status = typeinfo_cast<CCLabelBMFont*>(row->getChildByTag(kStatusTag))) {
            status->setString(filled ? "saved" : "empty");
            status->setColor(filled ? ccc3(120, 220, 120) : ccc3(180, 180, 180));
        }
        auto* menu = findMenu(row);
        styleLoadDeleteButton(findItemByTag(menu, kClearButtonTag), filled);
        styleLoadDeleteButton(findItemByTag(menu, kLoadButtonTag), filled);
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

        struct PopupRetainGuard {
            ProfileManagerPopup* popup = nullptr;

            explicit PopupRetainGuard(ProfileManagerPopup* retained) : popup(retained) {}

            ~PopupRetainGuard() {
                if (popup) {
                    popup->release();
                }
            }

            PopupRetainGuard(PopupRetainGuard const&) = delete;
            PopupRetainGuard& operator=(PopupRetainGuard const&) = delete;
        };

        CCNode* findSlotRowForIndex(ProfileManagerPopup* popup, std::size_t idx) {
            if (!popup) {
                return nullptr;
            }
            auto const wantId = profileNodeId(fmt::format("profile-slot-row-{}", idx));
            return popup->getChildByIDRecursive(wantId);
        }

        std::optional<std::size_t> slotIndexFromSender(CCObject* sender) {
            auto* btn = typeinfo_cast<CCMenuItemSpriteExtra*>(sender);
            if (!btn) {
                return std::nullopt;
            }
            auto* idxObj = typeinfo_cast<CCInteger*>(btn->getUserObject());
            if (!idxObj) {
                return std::nullopt;
            }
            auto const rawIdx = idxObj->getValue();
            if (rawIdx < 0) {
                return std::nullopt;
            }
            auto const idx = static_cast<std::size_t>(rawIdx);
            if (idx >= kSlotCount) {
                return std::nullopt;
            }
            return idx;
        }

    } // namespace

    void ProfileManagerPopup::onSaveSlot(cocos2d::CCObject* sender) {
        auto idxOpt = slotIndexFromSender(sender);
        if (!idxOpt) return;
        auto const idx = *idxOpt;
        auto const slot = slotNameAt(idx);
        bool const hadData = slotIsFilled(slot);

        this->retain();
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
            [this, idx, slot](FLAlertLayer*, bool ok) {
                PopupRetainGuard guard(this);
                if (!ok) return;
                snapshotIntoSlot(slot);
                if (auto* row = findSlotRowForIndex(this, idx)) {
                    refreshRow(row, idx);
                }
                Notification::create(fmt::format("Saved to {}", slot), NotificationIcon::Success, 1.5f)
                    ->show();
            }
        );
    }

    void ProfileManagerPopup::onLoadSlot(cocos2d::CCObject* sender) {
        auto idxOpt = slotIndexFromSender(sender);
        if (!idxOpt) return;
        auto const idx = *idxOpt;
        auto const slot = slotNameAt(idx);
        if (!slotIsFilled(slot)) return;

        this->retain();
        createQuickPopup(
            "Load Profile",
            fmt::format(
                "Load <cy>{}</c> now? The settings page will close so the new "
                "values can be applied cleanly.",
                slot
            ),
            "Cancel",
            "Load",
            [slot, this, idx](FLAlertLayer*, bool ok) {
                PopupRetainGuard guard(this);
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
                this->Popup::onClose(nullptr);
            }
        );
    }

    void ProfileManagerPopup::onClearSlot(cocos2d::CCObject* sender) {
        auto idxOpt = slotIndexFromSender(sender);
        if (!idxOpt) return;
        auto const idx = *idxOpt;
        auto const slot = slotNameAt(idx);
        if (!slotIsFilled(slot)) return;

        this->retain();
        createQuickPopup(
            "Delete Profile",
            fmt::format(
                "Permanently clear saved profile <cy>{}</c>? This cannot be "
                "undone.",
                slot
            ),
            "Cancel",
            "Delete",
            [this, idx, slot](FLAlertLayer*, bool ok) {
                PopupRetainGuard guard(this);
                if (!ok) return;
                clearSlot(slot);
                if (activeCustomTextSlotIndex() == idx) {
                    setActiveCustomTextSlotIndex(0);
                }
                if (auto* row = findSlotRowForIndex(this, idx)) {
                    refreshRow(row, idx);
                }
                Notification::create(fmt::format("Cleared {}", slot), NotificationIcon::Info, 1.5f)
                    ->show();
            }
        );
    }

    void ProfileManagerPopup::onRenameSlot(cocos2d::CCObject* sender) {
        auto idxOpt = slotIndexFromSender(sender);
        if (!idxOpt) return;
        auto const idx = *idxOpt;
        auto current = slotNameAt(idx);

        this->retain();
        if (auto* rename = RenamePopup::create(
                idx,
                std::move(current),
                [this, idx](std::string newName) {
                    auto res = renameSlot(idx, std::move(newName));
                    if (res.isErr()) {
                        Notification::create(res.unwrapErr(), NotificationIcon::Error, 2.f)->show();
                        return;
                    }
                    if (auto* row = findSlotRowForIndex(this, idx)) {
                        refreshRow(row, idx);
                    }
                },
                [this]() {
                    this->release();
                }
            )) {
            rename->show();
        }
        else {
            this->release();
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
