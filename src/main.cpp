#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

#ifdef GEODE_IS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include "embed_colors.hpp"
#include "profile/data.hpp"
#include "profile/popup.hpp"
#include "state.hpp"
#include "webhook.hpp"

#include <Geode/modify/UploadPopup.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <filesystem>
#include <system_error>

using namespace geode::prelude;

static const char* DEFAULT_TEMPLATE = R"(## {isUploaded"New Level!"}{isUpdated"Level Updated!"}
**{creator} {isUploaded"dropped a new"}{isUpdated"updated a"} level!**
- Name: {name}
- ID: {id}
-# {lengh} ({objects} objects)
||{role}||)";

class OpenFileButtonSettingV3 : public SettingV3 {
public:
    static Result<std::shared_ptr<SettingV3>> parse(
        std::string const& key,
        std::string const& modID,
        matjson::Value const& json
    ) {
        auto res = std::make_shared<OpenFileButtonSettingV3>();
        auto root = checkJson(json, "OpenFileButtonSettingV3");
        res->init(key, modID, root);
        res->parseNameAndDescription(root);
        root.checkUnknownKeys();
        return root.ok(std::static_pointer_cast<SettingV3>(std::move(res)));
    }

    bool load(matjson::Value const&) override { return true; }
    bool save(matjson::Value&) const override  { return true; }
    bool isDefaultValue() const override        { return true; }
    void reset() override {}

    SettingNodeV3* createNode(float width) override;
};

class OpenFileButtonSettingNodeV3 : public SettingNodeV3 {
protected:
    ButtonSprite* m_buttonSprite;
    CCMenuItemSpriteExtra* m_button;

    bool init(std::shared_ptr<OpenFileButtonSettingV3> setting, float width) {
        if (!SettingNodeV3::init(setting, width)) return false;

        m_buttonSprite = ButtonSprite::create("Edit File", "goldFont.fnt", "GJ_button_01.png", .8f);
        m_buttonSprite->setScale(.65f);
        m_button = CCMenuItemSpriteExtra::create(
            m_buttonSprite, this,
            menu_selector(OpenFileButtonSettingNodeV3::onButton)
        );
        this->getButtonMenu()->addChildAtPosition(m_button, Anchor::Center);
        this->getButtonMenu()->setContentWidth(80);
        this->getButtonMenu()->updateLayout();
        this->updateState(nullptr);
        return true;
    }

    void updateState(CCNode* invoker) override {
        SettingNodeV3::updateState(invoker);
        auto ok = this->getSetting()->shouldEnable();
        m_button->setEnabled(ok);
        m_buttonSprite->setCascadeColorEnabled(true);
        m_buttonSprite->setCascadeOpacityEnabled(true);
        m_buttonSprite->setOpacity(ok ? 255 : 155);
        m_buttonSprite->setColor(ok ? ccWHITE : ccGRAY);
    }

    void onButton(CCObject*) {
        auto path = Mod::get()->getConfigDir() / "customtext.txt";
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            (void)geode::utils::file::writeString(path, DEFAULT_TEMPLATE);
        }
#ifdef GEODE_IS_WINDOWS
        ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOW);
#else
        utils::file::openFolder(Mod::get()->getConfigDir());
#endif
        auto& session = gameSession();
        if (session.eventCount < session.trackedEvents.size()) {
            session.trackedEvents[session.eventCount++] = Tracked::CustomTextEdit;
        }
    }

    void onCommit() override {}
    void onResetToDefault() override {}

public:
    static OpenFileButtonSettingNodeV3* create(
        std::shared_ptr<OpenFileButtonSettingV3> setting, float width
    ) {
        auto ret = new OpenFileButtonSettingNodeV3();
        if (ret->init(setting, width)) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }

    bool hasUncommittedChanges() const override { return false; }
    bool hasNonDefaultValue() const override { return false; }

    std::shared_ptr<OpenFileButtonSettingV3> getSetting() const {
        return std::static_pointer_cast<OpenFileButtonSettingV3>(SettingNodeV3::getSetting());
    }
};

SettingNodeV3* OpenFileButtonSettingV3::createNode(float width) {
    return OpenFileButtonSettingNodeV3::create(
        std::static_pointer_cast<OpenFileButtonSettingV3>(shared_from_this()), width
    );
}

static std::string lengthString(int len) {
    switch (len) {
        case 0: return "Tiny";
        case 1: return "Short";
        case 2: return "Medium";
        case 3: return "Long";
        case 4: return "XL";
        case 5: return "Plat";
        default: return "Unknown";
    }
}

// Processes {isUploaded"..."} and {isUpdated"..."} conditional vars
static std::string processConditionals(std::string text, bool isUpdate) {
    auto process = [&](std::string& str, std::string const& tag, bool show) {
        std::string open = "{" + tag + "\"";
        size_t pos = 0;
        while ((pos = str.find(open, pos)) != std::string::npos) {
            size_t contentStart = pos + open.size();
            size_t closing = str.find("\"}", contentStart);
            if (closing == std::string::npos) break;
            std::string inner = str.substr(contentStart, closing - contentStart);
            std::string replacement = show ? inner : "";
            str.replace(pos, closing + 2 - pos, replacement);
            pos += replacement.size();
        }
    };
    process(text, "isUploaded", !isUpdate);
    process(text, "isUpdated",  isUpdate);
    return text;
}

// reads customtext.txt from config dir, creates it with a default template if missing
static std::string getCustomTextFromFile() {
    auto path = Mod::get()->getConfigDir() / "customtext.txt";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        (void)geode::utils::file::writeString(path, DEFAULT_TEMPLATE);
    }
    auto res = geode::utils::file::readString(path);
    if (res.isOk()) return res.unwrap();
    return std::string(DEFAULT_TEMPLATE);
}

static std::string buildUploadMessage(GJGameLevel* level, bool isUpdate) {
    auto mod = Mod::get();
    bool rolePing   = mod->getSettingValue<bool>("upload-role-ping");
    std::string roleID  = mod->getSettingValue<std::string>("upload-role-id");
    geode::utils::string::trimIP(roleID);
    bool const wantRolePing = rolePing && !roleID.empty();
    std::string creator = level->m_creatorName;
    std::string name    = level->m_levelName;
    std::string id      = geode::utils::numToString((int)level->m_levelID);
    std::string length  = lengthString(level->m_levelLength);
    std::string objects = geode::utils::numToString((int)level->m_objectCount);
    std::string text;

    if (mod->getSettingValue<bool>("upload-use-custom-text")) {
        text = getCustomTextFromFile();
        auto replace = [&](std::string_view from, std::string_view to) {
            geode::utils::string::replaceIP(text, from, to);
        };
        replace("{creator}", creator);
        replace("{name}",    name);
        replace("{id}",      id);
        replace("{lengh}",   length);
        replace("{length}",  length);
        replace("{objects}", objects);
        replace("{role}", wantRolePing ? fmt::format("<@&{}>", roleID) : "");
        text = processConditionals(text, isUpdate);
    } else {
        text = isUpdate
            ? fmt::format("**{}** updated a level!\n- Name: {}\n- ID:   {}\n-# {} ({} objects)", creator, name, id, length, objects)
            : fmt::format("**{}** dropped a new level!\n- Name: {}\n- ID:  {}\n-# {} ({} objects)", creator, name, id, length, objects);
        if (wantRolePing)
            text += fmt::format("\n||<@&{}>||", roleID);
    }
    return text;
}

class $modify(UploadPopup) {
    void levelUploadFinished(GJGameLevel* level) {
        UploadPopup::levelUploadFinished(level);
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("notify-level-upload")) return;

        // m_levelVersion > 1 means the level already on the servers
        bool isUpdate = level->m_levelVersion > 1;
        if (isUpdate && !mod->getSettingValue<bool>("upload-send-on-update")) return;

        auto& session = gameSession();
        if (session.eventCount < session.trackedEvents.size()) {
            session.trackedEvents[session.eventCount++] = isUpdate ? Tracked::LevelUpdate : Tracked::LevelUpload;
        }

        std::string content = buildUploadMessage(level, isUpdate);
        
        if (mod->getSettingValue<bool>("upload-use-custom-text")) {
            sendWebhookContent(content);
        } else {
            sendWebhookDirect(
                isUpdate ? "Level Updated" : "New Level Uploaded",
                content,
                isUpdate ? embed_color::kEditorExit : embed_color::kEditorOpen
            );
        }
    }
};

$execute
{
    GameEvent(GameEventType::Loaded)
        .listen(
            [] {
                auto& session = gameSession();
                if (session.started) {
                    return;
                }
                session.started = true;
                session.startTime = Clock::now();
                session.eventCount = 0;
                if (session.eventCount < session.trackedEvents.size()) {
                    session.trackedEvents[session.eventCount++] = Tracked::GameOpen;
                }
                auto const playerName = getPlayerName();
                sendWebhook(
                    "notify-game-session",
                    "Opened Geometry Dash",
                    fmt::format(
                        "{} opened Geometry Dash!",
                        playerName
                    ),
                    embed_color::kGameOpen
                );
            }
        )
        .leak();

    GameEvent(GameEventType::Exiting)
        .listen(
            [] {
                auto& session = gameSession();
                if (!session.started) {
                    return;
                }
                if (!Mod::get()
                         ->getSettingValue<bool>("notify-game-session")) {
                    return;
                }
                if (session.eventCount < session.trackedEvents.size()) {
                    session.trackedEvents[session.eventCount++] = Tracked::GameClose;
                }
                auto const playerName = getPlayerName();
                auto const elapsed =
                    formatDuration(secondsSince(session.startTime));
                std::string footer = elapsed;
                auto const summary = formatSessionTrackedSummary(session);
                if (!summary.empty()) {
                    footer += "\nSession: " + summary;
                    if (footer.size() > 2048) {
                        footer.resize(2045);
                        footer += "...";
                    }
                }
                if (Mod::get()
                        ->getSettingValue<bool>("blocking-webhook")) {
                    sendWebhookDirectSync(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} closed Geometry Dash.",
                            playerName
                        ),
                        embed_color::kGameClose,
                        {},
                        footer
                    );
                } else {
                    sendWebhookDirect(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} closed Geometry Dash.",
                            playerName
                        ),
                        embed_color::kGameClose,
                        {},
                        footer
                    );
                }
            }
        )
        .leak();
}

$on_mod(Loaded)
{
    (void)Mod::get()->registerCustomSettingType("open-file-button", &OpenFileButtonSettingV3::parse);

    ButtonSettingPressedEventV3(Mod::get(), "profile-manager")
        .listen(
            [](std::string_view buttonKey) {
                if (buttonKey == "manage") {
                    profile::ProfileManagerPopup::create()->show();
                }
            }
        )
        .leak();

    listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
        if (!enabled) {
            return;
        }
        auto const playerName = getPlayerName();
        auto& session = gameSession();
        if (session.eventCount < session.trackedEvents.size()) {
            session.trackedEvents[session.eventCount++] = Tracked::TestWebhook;
        }
        sendWebhookDirect(
            "Test Webhook",
            fmt::format(
                "{} is testing the webhook!",
                playerName
            ),
            embed_color::kTestWebhook
        );
        Mod::get()->setSettingValue<bool>("test-webhook", false);
    });
}
