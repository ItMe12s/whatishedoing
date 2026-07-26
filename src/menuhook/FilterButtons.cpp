#include "state.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/utils/cocos.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>

using namespace geode::prelude;

namespace {

    cocos2d::CCSprite* createFilterSprite(char const* checkFrame) {
        auto* base = cocos2d::CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        if (!base) {
            return nullptr;
        }
        if (auto* check = cocos2d::CCSprite::createWithSpriteFrameName(checkFrame)) {
            check->setPosition(base->getContentSize() * 0.5f);
            base->addChild(check);
        }
        base->setScale(0.75f);
        return base;
    }

    CCMenuItemToggler* createFilterButton(GJGameLevel* level) {
        auto* on = createFilterSprite("GJ_checkOn_001.png");
        auto* off = createFilterSprite("GJ_checkOff_001.png");
        if (!on || !off) {
            return nullptr;
        }
        auto* button = CCMenuItemExt::createToggler(on, off, [level](auto* toggler) {
            int const id = level ? EditorIDs::getID(level) : 0;
            if (id > 0) {
                setIdInFilterList(id, !isIdInFilterList(id));
            }
            else {
                toggler->toggle(false);
            }
        });
        button->updateSprite();
        int const id = level ? EditorIDs::getID(level) : 0;
        button->toggle(id > 0 && isIdInFilterList(id));
        return button;
    }

} // namespace

struct WIHLevelInfoLayer : Modify<WIHLevelInfoLayer, LevelInfoLayer> {
    struct Fields {
        CCMenuItemToggler* button = nullptr;
        int playVisibleTries = 0;
    };

    void wihCheckIfPlayVisible(float) {
        if (!m_fields->button) {
            this->unschedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
            return;
        }
        if (m_playBtnMenu && cocos::nodeIsVisible(m_playBtnMenu)) {
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
        auto* button = createFilterButton(m_level);
        if (!button) {
            return true;
        }
        m_fields->button = button;
        button->setID("filter-level-button-levelinfo"_spr);
        button->setZOrder(1);
        button->setVisible(false);
        otherMenu->addChild(button);

        float const step = button->getScaledContentSize().width;
        if (cocos::nodeIsVisible(favoriteButton)) {
            button->setPosition(
                ccp(favoriteButton->getPositionX() + step, settingsButton->getPositionY())
            );
        }
        else {
            auto const position = favoriteButton->getPosition();
            button->setPosition(ccp(position.x + 2.f * step, position.y));
        }

        otherMenu->updateLayout();
        this->schedule(schedule_selector(WIHLevelInfoLayer::wihCheckIfPlayVisible));
        return true;
    }
};

struct WIHEditLevelLayer : Modify<WIHEditLevelLayer, EditLevelLayer> {
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
        auto* button = createFilterButton(m_level);
        if (!button) {
            return true;
        }
        button->setID("filter-level-button-editor"_spr);
        button->setZOrder(1);
        menu->addChild(button);

        float const step = button->getScaledContentSize().width;
        button->setPosition(
            ccp(infoButton->getPositionX() + 2.f * step, infoButton->getPositionY() + step)
        );
        menu->updateLayout();
        return true;
    }
};
