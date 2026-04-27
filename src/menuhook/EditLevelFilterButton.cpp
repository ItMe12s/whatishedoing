#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <Geode/Geode.hpp>
#include <Geode/loader/Priority.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include "state.hpp"

#include <cmath>
#include <limits>

using namespace geode::prelude;

namespace {

constexpr float kWihButtonGap = 6.f;
auto constexpr kWihButtonId = "whatishedoing-filter-level-button";
auto constexpr kWihInfoMenuId = "info-button-menu";
auto constexpr kWihInfoButtonId = "info-button";

float wihRightmostMenuEdge(cocos2d::CCNode* menu, cocos2d::CCNode* exclude) {
    float maxR = -std::numeric_limits<float>::infinity();
    if (!menu) {
        return maxR;
    }
    int const n = static_cast<int>(menu->getChildrenCount());
    for (int i = 0; i < n; ++i) {
        cocos2d::CCNode* child = menu->getChildByIndex(i);
        if (!child || child == exclude) {
            continue;
        }
        cocos2d::CCRect const r = child->boundingBox();
        maxR = std::max(maxR, r.getMaxX());
    }
    return maxR;
}

void wihPlaceFilterButton(
    cocos2d::CCNode* menu,
    CCMenuItemSpriteExtra* btn,
    cocos2d::CCNode* infoBtn
) {
    if (!menu || !btn || !infoBtn) {
        return;
    }
    float maxR = wihRightmostMenuEdge(menu, btn);
    if (!std::isfinite(maxR)) {
        cocos2d::CCRect const ir = infoBtn->boundingBox();
        maxR = ir.getMaxX();
    }
    float const ax = btn->getAnchorPoint().x;
    float const w = btn->getScaledContentSize().width;
    float const x = maxR + kWihButtonGap + w * ax;
    btn->setPosition(ccp(x, infoBtn->getPositionY()));
}

} // namespace

struct WIHEditLevelLayer : Modify<WIHEditLevelLayer, EditLevelLayer> {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EditLevelLayer::init", Priority::VeryLate);
    }

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

    void wihDeferredReposition(float) {
        auto* menu = this->getChildByID(kWihInfoMenuId);
        auto* infoBtn = menu ? menu->getChildByID(kWihInfoButtonId) : nullptr;
        auto* btn = m_fields->m_wihFilterButton;
        if (!menu || !infoBtn || !btn) {
            return;
        }
        wihPlaceFilterButton(menu, btn, infoBtn);
        menu->updateLayout();
    }

    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) {
            return false;
        }
        auto* menu = this->getChildByID(kWihInfoMenuId);
        if (!menu) {
            return true;
        }
        auto* infoBtn = menu->getChildByID(kWihInfoButtonId);
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
        btn->setID(kWihButtonId);
        btn->setZOrder(1);
        menu->addChild(btn);
        m_fields->m_wihFilterButton = btn;
        wihPlaceFilterButton(menu, btn, infoBtn);
        wihRefreshFilterIcon();
        menu->updateLayout();
        this->scheduleOnce(
            schedule_selector(WIHEditLevelLayer::wihDeferredReposition),
            0.f
        );
        return true;
    }
};
