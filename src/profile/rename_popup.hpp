#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/utils/function.hpp>
#include <string>

namespace geode {
    class TextInput;
}

namespace profile {

    class RenamePopup : public geode::Popup {
    protected:
        geode::TextInput* m_input = nullptr;
        geode::Function<void(std::string)> m_onAccept;

        bool init(std::string current, geode::Function<void(std::string)> onAccept);

        void onAccept();

    public:
        static RenamePopup* create(std::string current, geode::Function<void(std::string)> onAccept);
    };

} // namespace profile
