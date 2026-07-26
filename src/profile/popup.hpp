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

        void onSaveSlot(std::size_t idx);
        void onLoadSlot(std::size_t idx);
        void onClearSlot(std::size_t idx);
        void onRenameSlot(std::size_t idx);

    public:
        static ProfileManagerPopup* create();
    };

} // namespace profile
