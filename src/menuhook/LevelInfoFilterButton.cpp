#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include "state.hpp"

using namespace geode::prelude;

struct WIHLevelInfoLayer : Modify<WIHLevelInfoLayer, LevelInfoLayer> {
    struct Fields {
        cocos2d::CCSprite* m_wihFilterInList = nullptr;
        cocos2d::CCSprite* m_wihFilterNotInList = nullptr;
        CCMenuItemSpriteExtra* m_wihFilterButton = nullptr;
    };

    void wihRefreshFilterIcon() {
        if (!m_level) {
            return;
        }
        if (!m_fields->m_wihFilterInList || !m_fields->m_wihFilterNotInList) {
            return;
        }
        int const id = EditorIDs::getID(m_level);
        if (id <= 0) {
            m_fields->m_wihFilterInList->setVisible(false);
            m_fields->m_wihFilterNotInList->setVisible(true);
            return;
        }
        bool const inList = isIdInFilterList(id);
        m_fields->m_wihFilterInList->setVisible(inList);
        m_fields->m_wihFilterNotInList->setVisible(!inList);
    }

    void onWihFilterToggle(cocos2d::CCObject*) {
        if (!m_level) {
            return;
        }
        int const id = EditorIDs::getID(m_level);
        if (id <= 0) {
            return;
        }
        if (isIdInFilterList(id)) {
            setIdInFilterList(id, false);
        } else {
            setIdInFilterList(id, true);
        }
        wihRefreshFilterIcon();
    }

    void wihCheckIfPlayVisible(float) {
        if (!m_fields->m_wihFilterButton) {
            return;
        }
        if (m_playBtnMenu && m_playBtnMenu->isVisible()) {
            m_fields->m_wihFilterButton->setVisible(true);
            this->unschedule(
                schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible)
            );
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
        auto* fav = otherMenu->getChildByID("favorite-button");
        if (!fav) {
            return true;
        }
        auto* settingsBtn = settingsMenu->getChildByID("settings-button");
        if (!settingsBtn) {
            return true;
        }

        auto* base = cocos2d::CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        if (!base) {
            return true;
        }
        m_fields->m_wihFilterInList =
            cocos2d::CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        m_fields->m_wihFilterNotInList =
            cocos2d::CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        if (m_fields->m_wihFilterInList) {
            m_fields->m_wihFilterInList->setPosition(base->getContentSize() * 0.5f);
            base->addChild(m_fields->m_wihFilterInList);
        }
        if (m_fields->m_wihFilterNotInList) {
            m_fields->m_wihFilterNotInList->setPosition(base->getContentSize() * 0.5f);
            base->addChild(m_fields->m_wihFilterNotInList);
        }
        base->setScale(0.75f);
        auto* btn = CCMenuItemSpriteExtra::create(
            base,
            nullptr,
            this,
            menu_selector(WIHLevelInfoLayer::onWihFilterToggle)
        );
        if (!btn) {
            return true;
        }
        m_fields->m_wihFilterButton = btn;
        btn->setID("whatishedoing-filter-level-button-levelinfo");
        btn->setZOrder(1);
        btn->setVisible(false);
        otherMenu->addChild(btn);

        // https://github.com/abb2k/death-tracker/blob/main/src/hooks/DTLevelInfoLayer.cpp
        // plus 2x button width on X so it clears death-tracker’s same-row slot
        float const step = btn->getScaledContentSize().width;
        if (fav->isVisible()) {
            btn->setPosition(
                ccp(
                    fav->getPositionX() + 2.f * step,
                    settingsBtn->getPositionY()
                )
            );
        } else {
            cocos2d::CCPoint const p = fav->getPosition();
            btn->setPosition(ccp(p.x + 2.f * step, p.y));
        }

        wihRefreshFilterIcon();
        otherMenu->updateLayout();
        this->schedule(
            schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible)
        );
        return true;
    }
};
