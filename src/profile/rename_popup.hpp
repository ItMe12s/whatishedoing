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
    std::function<void()> m_onClosed;

    bool init(
        std::size_t idx,
        std::string current,
        std::function<void(std::string)> onAccept,
        std::function<void()> onClosed
    );

    void onAccept(cocos2d::CCObject* sender);
    void onClose(cocos2d::CCObject* sender) override;

public:
    static RenamePopup* create(
        std::size_t idx,
        std::string current,
        std::function<void(std::string)> onAccept,
        std::function<void()> onClosed = {}
    );
};

} // namespace profile
