#pragma once

#include <Geode/loader/SettingV3.hpp>

namespace profile {

class ProfileManagerSettingV3 final : public geode::SettingV3 {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string key,
        std::string modID,
        matjson::Value const& json
    );

    bool load(matjson::Value const& json) override;
    bool save(matjson::Value& json) const override;
    geode::SettingNodeV3* createNode(float width) override;

    bool isDefaultValue() const override;
    void reset() override;
};

} // namespace profile
