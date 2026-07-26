#include "state.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>

using namespace geode::prelude;

namespace {

    struct FilterIcons {
        cocos2d::CCSprite* inList = nullptr;
        cocos2d::CCSprite* notInList = nullptr;
    };

    cocos2d::CCSprite* createFilterSprite(FilterIcons& icons) {
        auto* base = cocos2d::CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        if (!base) {
            return nullptr;
        }
        icons.inList = cocos2d::CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        icons.notInList = cocos2d::CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        for (auto* icon : {icons.inList, icons.notInList}) {
            if (icon) {
                icon->setPosition(base->getContentSize() * 0.5f);
                base->addChild(icon);
            }
        }
        base->setScale(0.75f);
        return base;
    }

    void refreshFilterIcons(GJGameLevel* level, FilterIcons const& icons) {
        if (!level || !icons.inList || !icons.notInList) {
            return;
        }
        int const id = EditorIDs::getID(level);
        bool const inList = id > 0 && isIdInFilterList(id);
        icons.inList->setVisible(inList);
        icons.notInList->setVisible(!inList);
    }

    bool toggleFilter(GJGameLevel* level) {
        if (!level) {
            return false;
        }
        int const id = EditorIDs::getID(level);
        if (id <= 0) {
            return false;
        }
        setIdInFilterList(id, !isIdInFilterList(id));
        return true;
    }

} // namespace

struct WIHLevelInfoLayer : Modify<WIHLevelInfoLayer, LevelInfoLayer> {
    struct Fields {
        FilterIcons icons;
        CCMenuItemSpriteExtra* button = nullptr;
        int playVisibleTries = 0;
    };

    void wihRefreshFilterIcon() {
        refreshFilterIcons(m_level, m_fields->icons);
    }

    void onWihFilterToggle(cocos2d::CCObject*) {
        if (toggleFilter(m_level)) {
            wihRefreshFilterIcon();
        }
    }

    void wihCheckIfPlayVisible(float) {
        if (!m_fields->button) {
            this->unschedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
            return;
        }
        if (m_playBtnMenu && m_playBtnMenu->isVisible()) {
            m_fields->button->setVisible(true);
            this->unschedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
            return;
        }
        ++m_fields->playVisibleTries;
        if (m_fields->playVisibleTries >= 180) {
            this->unschedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
        }
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }
        auto* otherMenu = this->getChildByID("other-menu");
        auto* settingsMenu = this->getChildByID("settings-menu");
        if (!otherMenu || !settingsMenu) {
            return true;
        }
        auto* favoriteButton = otherMenu->getChildByID("favorite-button");
        auto* settingsButton = settingsMenu->getChildByID("settings-button");
        if (!favoriteButton || !settingsButton) {
            return true;
        }
        auto* base = createFilterSprite(m_fields->icons);
        if (!base) {
            return true;
        }
        auto* button = CCMenuItemSpriteExtra::create(
            base, nullptr, this, menu_selector(WIHLevelInfoLayer::onWihFilterToggle)
        );
        if (!button) {
            return true;
        }
        m_fields->button = button;
        button->setID("filter-level-button-levelinfo"_spr);
        button->setZOrder(1);
        button->setVisible(false);
        otherMenu->addChild(button);

        // Preserve the existing placement relative to the stock controls.
        float const step = button->getScaledContentSize().width;
        if (favoriteButton->isVisible()) {
            button->setPosition(
                ccp(favoriteButton->getPositionX() + step, settingsButton->getPositionY())
            );
        }
        else {
            auto const position = favoriteButton->getPosition();
            button->setPosition(ccp(position.x + 2.f * step, position.y));
        }

        wihRefreshFilterIcon();
        otherMenu->updateLayout();
        this->schedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
        return true;
    }
};

struct WIHEditLevelLayer : Modify<WIHEditLevelLayer, EditLevelLayer> {
    struct Fields {
        FilterIcons icons;
    };

    void wihRefreshFilterIcon() {
        refreshFilterIcons(m_level, m_fields->icons);
    }

    void onWihFilterToggle(cocos2d::CCObject*) {
        if (toggleFilter(m_level)) {
            wihRefreshFilterIcon();
        }
    }

    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) {
            return false;
        }
        auto* menu = this->getChildByID("info-button-menu");
        if (!menu) {
            return true;
        }
        auto* infoButton = menu->getChildByID("info-button");
        if (!infoButton) {
            return true;
        }
        auto* base = createFilterSprite(m_fields->icons);
        if (!base) {
            return true;
        }
        auto* button = CCMenuItemSpriteExtra::create(
            base, nullptr, this, menu_selector(WIHEditLevelLayer::onWihFilterToggle)
        );
        if (!button) {
            return true;
        }
        button->setID("filter-level-button-editor"_spr);
        button->setZOrder(1);
        menu->addChild(button);

        // Preserve compatibility with the neighboring third-party button.
        float const step = button->getScaledContentSize().width;
        button->setPosition(
            ccp(infoButton->getPositionX() + 2.f * step, infoButton->getPositionY() + step)
        );
        wihRefreshFilterIcon();
        menu->updateLayout();
        return true;
    }
};
