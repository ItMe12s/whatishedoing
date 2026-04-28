#pragma once

#include <Geode/ui/Popup.hpp>
#include <cstddef>
#include <string>

namespace profile {

class ProfileManagerPopup : public geode::Popup {
protected:
    bool init();

    cocos2d::CCNode* makeSlotRow(std::size_t idx, float width);
    void refreshRow(cocos2d::CCNode* row, std::size_t idx);

    void onSaveSlot(cocos2d::CCObject* sender);
    void onLoadSlot(cocos2d::CCObject* sender);
    void onClearSlot(cocos2d::CCObject* sender);
    void onRenameSlot(cocos2d::CCObject* sender);

public:
    static ProfileManagerPopup* create();
};

} // namespace profile
