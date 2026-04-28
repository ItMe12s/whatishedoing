#include "popup.hpp"

#include "data.hpp"
#include "rename_popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/utils/cocos.hpp>
#include <vector>

using namespace geode::prelude;

namespace profile {

namespace {

constexpr float kPopupWidth = 380.f;
constexpr float kPopupHeight = 290.f;
constexpr float kRowHeight = 22.f;

constexpr int kNameLabelTag = 1000;
constexpr int kStatusTag = 1001;
constexpr int kLoadButtonTag = 1002;
constexpr int kSaveButtonTag = 1003;
constexpr int kClearButtonTag = 1004;

void styleButton(CCMenuItemSpriteExtra* btn, bool enabled) {
    if (!btn) return;
    btn->setEnabled(enabled);
    if (auto* spr = typeinfo_cast<CCSprite*>(btn->getNormalImage())) {
        spr->setColor(enabled ? ccc3(255, 255, 255) : ccc3(120, 120, 120));
        spr->setOpacity(enabled ? 255 : 160);
    }
}

CCMenuItemSpriteExtra* findItemByTag(CCMenu* menu, int tag) {
    if (!menu) return nullptr;
    return static_cast<CCMenuItemSpriteExtra*>(menu->getChildByTag(tag));
}

CCMenu* findMenu(CCNode* row) {
    if (!row || !row->getChildren()) return nullptr;
    return static_cast<CCMenu*>(row->getChildren()->lastObject());
}

} // namespace

cocos2d::CCNode* ProfileManagerPopup::makeSlotRow(
    std::size_t idx,
    float width
) {
    auto const slot = slotNameAt(idx);

    auto* row = CCNode::create();
    row->setContentSize({width, kRowHeight});
    row->setUserObject(CCInteger::create(static_cast<int>(idx)));

    auto* label = CCLabelBMFont::create(slot.c_str(), "bigFont.fnt");
    label->setScale(.45f);
    label->setAnchorPoint({0.f, .5f});
    label->setPosition({4.f, kRowHeight * .5f});
    label->setTag(kNameLabelTag);
    row->addChild(label);

    auto* status = CCLabelBMFont::create("", "chatFont.fnt");
    status->setScale(.55f);
    status->setAnchorPoint({0.f, .5f});
    status->setPosition({124.f, kRowHeight * .5f});
    status->setTag(kStatusTag);
    row->addChild(status);

    auto* menu = CCMenu::create();
    menu->ignoreAnchorPointForPosition(false);
    menu->setContentSize({width, kRowHeight});
    menu->setAnchorPoint({0.f, 0.f});
    menu->setPosition({0.f, 0.f});
    row->addChild(menu);

    auto* renameSpr = ButtonSprite::create(
        "Rename",
        "bigFont.fnt",
        "GJ_button_04.png",
        .8f
    );
    renameSpr->setScale(.45f);
    auto* renameBtn = CCMenuItemSpriteExtra::create(
        renameSpr,
        this,
        menu_selector(ProfileManagerPopup::onRenameSlot)
    );
    renameBtn->setUserObject(CCInteger::create(static_cast<int>(idx)));
    renameBtn->setPosition({width - 168.f, kRowHeight * .5f});
    menu->addChild(renameBtn);

    auto* clearSpr = ButtonSprite::create(
        "Delete",
        "bigFont.fnt",
        "GJ_button_06.png",
        .8f
    );
    clearSpr->setScale(.45f);
    auto* clearBtn = CCMenuItemSpriteExtra::create(
        clearSpr,
        this,
        menu_selector(ProfileManagerPopup::onClearSlot)
    );
    clearBtn->setUserObject(CCInteger::create(static_cast<int>(idx)));
    clearBtn->setPosition({width - 116.f, kRowHeight * .5f});
    clearBtn->setTag(kClearButtonTag);
    menu->addChild(clearBtn);

    auto* saveSpr = ButtonSprite::create(
        "Save",
        "bigFont.fnt",
        "GJ_button_05.png",
        .8f
    );
    saveSpr->setScale(.45f);
    auto* saveBtn = CCMenuItemSpriteExtra::create(
        saveSpr,
        this,
        menu_selector(ProfileManagerPopup::onSaveSlot)
    );
    saveBtn->setUserObject(CCInteger::create(static_cast<int>(idx)));
    saveBtn->setPosition({width - 64.f, kRowHeight * .5f});
    saveBtn->setTag(kSaveButtonTag);
    menu->addChild(saveBtn);

    auto* loadSpr = ButtonSprite::create(
        "Load",
        "bigFont.fnt",
        "GJ_button_01.png",
        .8f
    );
    loadSpr->setScale(.45f);
    auto* loadBtn = CCMenuItemSpriteExtra::create(
        loadSpr,
        this,
        menu_selector(ProfileManagerPopup::onLoadSlot)
    );
    loadBtn->setUserObject(CCInteger::create(static_cast<int>(idx)));
    loadBtn->setPosition({width - 18.f, kRowHeight * .5f});
    loadBtn->setTag(kLoadButtonTag);
    menu->addChild(loadBtn);

    refreshRow(row, idx);
    return row;
}

void ProfileManagerPopup::refreshRow(
    cocos2d::CCNode* row,
    std::size_t idx
) {
    auto const slot = slotNameAt(idx);
    bool const filled = slotIsFilled(slot);

    if (auto* label = static_cast<CCLabelBMFont*>(
            row->getChildByTag(kNameLabelTag))) {
        label->setString(slot.c_str());
    }
    if (auto* status = static_cast<CCLabelBMFont*>(
            row->getChildByTag(kStatusTag))) {
        status->setString(filled ? "saved" : "empty");
        status->setColor(
            filled ? ccc3(120, 220, 120) : ccc3(180, 180, 180)
        );
    }
    auto* menu = findMenu(row);
    styleButton(findItemByTag(menu, kLoadButtonTag), filled);
    styleButton(findItemByTag(menu, kClearButtonTag), filled);
}

bool ProfileManagerPopup::init() {
    if (!Popup::init(kPopupWidth, kPopupHeight, "GJ_square01.png")) {
        return false;
    }
    this->setTitle("Profile Manager");

    float const innerWidth = kPopupWidth - 30.f;
    float const listH = kRowHeight * kSlotCount + 8.f;

    auto* listBg = CCLayerColor::create({0, 0, 0, 75});
    listBg->setContentSize({innerWidth, listH});
    listBg->ignoreAnchorPointForPosition(false);
    listBg->setAnchorPoint({.5f, .5f});
    m_mainLayer->addChildAtPosition(
        listBg,
        Anchor::Center,
        ccp(0.f, -14.f)
    );

    auto* hint = CCLabelBMFont::create(
        "Save: snapshot of last-applied settings.  Load: applies on next launch.",
        "chatFont.fnt"
    );
    hint->setScale(.55f);
    hint->setColor(ccc3(180, 180, 180));
    m_mainLayer->addChildAtPosition(
        hint,
        Anchor::Center,
        ccp(0.f, listH * .5f - 6.f)
    );

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

std::size_t indexFromSender(CCObject* sender) {
    auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    auto* idxObj = static_cast<CCInteger*>(btn->getUserObject());
    return idxObj
        ? static_cast<std::size_t>(idxObj->getValue())
        : 0;
}

CCNode* rowFromButton(CCObject* sender) {
    return static_cast<CCNode*>(sender)->getParent()->getParent();
}

} // namespace

void ProfileManagerPopup::onSaveSlot(cocos2d::CCObject* sender) {
    auto const idx = indexFromSender(sender);
    auto const slot = slotNameAt(idx);
    snapshotIntoSlot(slot);
    refreshRow(rowFromButton(sender), idx);
    Notification::create(
        fmt::format("Saved to {}", slot),
        NotificationIcon::Success,
        1.5f
    )->show();
}

void ProfileManagerPopup::onLoadSlot(cocos2d::CCObject* sender) {
    auto const idx = indexFromSender(sender);
    auto const slot = slotNameAt(idx);
    if (!slotIsFilled(slot)) return;

    createQuickPopup(
        "Load Profile",
        fmt::format(
            "Load <cy>{}</c> now? The settings page will close so the new "
            "values can be applied cleanly.",
            slot
        ),
        "Cancel",
        "Load",
        [slot](FLAlertLayer*, bool ok) {
            if (!ok) return;
            if (!applyProfileNow(slot)) {
                Notification::create(
                    "Profile load failed",
                    NotificationIcon::Error,
                    2.f
                )->show();
                return;
            }
            auto* scene =
                CCDirector::sharedDirector()->getRunningScene();
            std::vector<CCNode*> toClose;
            if (scene && scene->getChildren()) {
                for (auto* child : CCArrayExt<CCNode*>(
                         scene->getChildren())) {
                    if (typeinfo_cast<geode::Popup*>(child)) {
                        toClose.push_back(child);
                    }
                }
            }
            for (auto* p : toClose) {
                p->removeFromParentAndCleanup(true);
            }
            Notification::create(
                fmt::format("Loaded {}", slot),
                NotificationIcon::Success,
                1.5f
            )->show();
        }
    );
}

void ProfileManagerPopup::onClearSlot(cocos2d::CCObject* sender) {
    auto const idx = indexFromSender(sender);
    auto const slot = slotNameAt(idx);
    if (!slotIsFilled(slot)) return;

    clearSlot(slot);
    refreshRow(rowFromButton(sender), idx);
    Notification::create(
        fmt::format("Cleared {}", slot),
        NotificationIcon::Info,
        1.5f
    )->show();
}

void ProfileManagerPopup::onRenameSlot(cocos2d::CCObject* sender) {
    auto const idx = indexFromSender(sender);
    auto* row = rowFromButton(sender);
    auto current = slotNameAt(idx);

    RenamePopup::create(
        idx,
        std::move(current),
        [this, idx, row](std::string newName) {
            auto res = renameSlot(idx, std::move(newName));
            if (res.isErr()) {
                Notification::create(
                    res.unwrapErr(),
                    NotificationIcon::Error,
                    2.f
                )->show();
                return;
            }
            this->refreshRow(row, idx);
        }
    )->show();
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
