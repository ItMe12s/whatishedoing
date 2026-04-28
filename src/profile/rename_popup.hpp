#pragma once

#include <Geode/ui/Popup.hpp>
#include <cstddef>
#include <functional>
#include <string>

namespace geode {
class TextInput;
}

namespace profile {

class RenamePopup : public geode::Popup {
protected:
    std::size_t m_idx = 0;
    geode::TextInput* m_input = nullptr;
    std::function<void(std::string)> m_onAccept;

    bool init(
        std::size_t idx,
        std::string current,
        std::function<void(std::string)> onAccept
    );

    void onAccept(cocos2d::CCObject* sender);

public:
    static RenamePopup* create(
        std::size_t idx,
        std::string current,
        std::function<void(std::string)> onAccept
    );
};

} // namespace profile
