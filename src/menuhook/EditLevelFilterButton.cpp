#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <Geode/Geode.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include "state.hpp"

using namespace geode::prelude;

struct WIHEditLevelLayer : Modify<WIHEditLevelLayer, EditLevelLayer> {
    struct Fields {
        cocos2d::CCSprite* m_wihFilterInList = nullptr;
        cocos2d::CCSprite* m_wihFilterNotInList = nullptr;
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
            m_fields->m_wihFilterNotInList->setVisible(false);
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

    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) {
            return false;
        }
        auto* menu = this->getChildByID("info-button-menu");
        if (!menu) {
            return true;
        }
        auto* infoBtn = menu->getChildByID("info-button");
        if (!infoBtn) {
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
            menu_selector(WIHEditLevelLayer::onWihFilterToggle)
        );
        if (!btn) {
            return true;
        }
        btn->setID("whatishedoing-filter-level-button");
        btn->setZOrder(1);
        menu->addChild(btn);
        btn->setPosition(
            ccp(
                infoBtn->getPositionX() + btn->getScaledContentSize().width,
                infoBtn->getPositionY()
            )
        );
        wihRefreshFilterIcon();
        menu->updateLayout();
        return true;
    }
};
