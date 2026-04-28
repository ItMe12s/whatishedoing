#include "popup.hpp"

#include "data.hpp"
#include "rename_popup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/utils/cocos.hpp>
#include <string>
#include <vector>

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

void ensureRenameSaveButton(CCMenuItemSpriteExtra* btn) {
    if (!btn) return;
    btn->setEnabled(true);
    btn->setOpacity(255);
    if (auto* spr = typeinfo_cast<CCSprite*>(btn->getNormalImage())) {
        spr->setOpacity(255);
    }
}

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
    return static_cast<CCMenuItemSpriteExtra*>(menu->getChildByTag(tag));
}

CCMenu* findMenu(CCNode* row) {
    if (!row || !row->getChildren()) return nullptr;
    return static_cast<CCMenu*>(row->getChildren()->lastObject());
}

std::string profileNodeId(std::string const& local) {
    return fmt::format("{}/{}", Mod::get()->getID(), local);
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
    renameBtn->setTag(kRenameButtonTag);
    renameBtn->setID(profileNodeId(fmt::format("profile-slot-{}-rename", idx)));
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
    clearBtn->setTag(kClearButtonTag);
    clearBtn->setID(profileNodeId(fmt::format("profile-slot-{}-delete", idx)));
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
    saveBtn->setTag(kSaveButtonTag);
    saveBtn->setID(profileNodeId(fmt::format("profile-slot-{}-save", idx)));
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
    loadBtn->setTag(kLoadButtonTag);
    loadBtn->setID(profileNodeId(fmt::format("profile-slot-{}-load", idx)));
    menu->addChild(loadBtn);

    menu->updateLayout();

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
        label->limitLabelWidth(
            kNameLabelMaxWidth,
            kNameLabelScale,
            kNameLabelMinScale
        );
    }
    if (auto* status = static_cast<CCLabelBMFont*>(
            row->getChildByTag(kStatusTag))) {
        status->setString(filled ? "saved" : "empty");
        status->setColor(
            filled ? ccc3(120, 220, 120) : ccc3(180, 180, 180)
        );
    }
    auto* menu = findMenu(row);
    ensureRenameSaveButton(findItemByTag(menu, kRenameButtonTag));
    styleLoadDeleteButton(findItemByTag(menu, kClearButtonTag), filled);
    ensureRenameSaveButton(findItemByTag(menu, kSaveButtonTag));
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
    m_mainLayer->addChildAtPosition(
        listBg,
        Anchor::Center,
        ccp(0.f, -14.f)
    );

    auto* hint = CCLabelBMFont::create(
        "Save: snapshot of last-applied settings. | Load: close this menu and apply your settings.",
        "chatFont.fnt"
    );
    hint->setScale(.55f);
    hint->setColor(ccc3(180, 180, 180));
    hint->setID("profile-manager-hint"_spr);
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
    auto* row = rowFromButton(sender);
    bool const hadData = slotIsFilled(slot);

    createQuickPopup(
        "Save Profile",
        hadData
            ? fmt::format(
                  "Overwrite saved data in <cy>{}</c> with a snapshot of "
                  "your current (last-applied) settings?",
                  slot
              )
            : fmt::format(
                  "Save a snapshot of your current settings to <cy>{}</c>?",
                  slot
              ),
        "Cancel",
        "Save",
        [this, idx, row, slot](FLAlertLayer*, bool ok) {
            if (!ok) return;
            snapshotIntoSlot(slot);
            refreshRow(row, idx);
            Notification::create(
                fmt::format("Saved to {}", slot),
                NotificationIcon::Success,
                1.5f
            )->show();
        }
    );
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
            std::vector<FLAlertLayer*> toClose;
            if (scene && scene->getChildren()) {
                for (auto* child : CCArrayExt<CCNode*>(
                         scene->getChildren())) {
                    if (auto* alert = typeinfo_cast<FLAlertLayer*>(child)) {
                        toClose.push_back(alert);
                    }
                }
            }
            for (auto* p : toClose) {
                p->keyBackClicked();
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

    auto* row = rowFromButton(sender);
    createQuickPopup(
        "Delete Profile",
        fmt::format(
            "Permanently clear saved profile <cy>{}</c>? This cannot be "
            "undone.",
            slot
        ),
        "Cancel",
        "Delete",
        [this, idx, row, slot](FLAlertLayer*, bool ok) {
            if (!ok) return;
            clearSlot(slot);
            refreshRow(row, idx);
            Notification::create(
                fmt::format("Cleared {}", slot),
                NotificationIcon::Info,
                1.5f
            )->show();
        }
    );
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
