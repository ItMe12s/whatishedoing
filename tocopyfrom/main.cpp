#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/UploadPopup.hpp>
#include <fstream>
#include <sstream>

using namespace geode::prelude;

static const char* DEFAULT_TEMPLATE = R"(## {isUploaded"New Level!"}{isUpdated"Level Updated!"}
**{creator} {isUploaded"dropped a new"}{isUpdated"updated a"} level!**
- Name: {name}
- ID: {id}
-# {lengh} ({objects} objects)
||{role}||)";

// button setting that opens customtext.txt in notepad (windows) or the config folder (other)
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
        // create the file with a default template if it doesn't exist yet
        if (!std::filesystem::exists(path))
            std::ofstream(path) << DEFAULT_TEMPLATE;
#ifdef GEODE_IS_WINDOWS
        ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOW);
#else
        utils::file::openFolder(Mod::get()->getConfigDir());
#endif
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

$on_mod(Loaded) {
    (void)Mod::get()->registerCustomSettingType("open-file-button", &OpenFileButtonSettingV3::parse);
}

// Converts m_levelLength int to a readable string so i don't fuck out mid code
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
    if (!std::filesystem::exists(path))
        std::ofstream(path) << DEFAULT_TEMPLATE;
    std::ostringstream ss;
    ss << std::ifstream(path).rdbuf();
    return ss.str();
}

// fills in all the template vars in the message
static std::string buildMessage(GJGameLevel* level, bool isUpdate) {
    auto mod = Mod::get();
    bool rolePing   = mod->getSettingValue<bool>("role-ping");
    std::string roleID  = mod->getSettingValue<std::string>("role-id");
    std::string creator = level->m_creatorName;
    std::string name    = level->m_levelName;
    std::string id      = std::to_string((int)level->m_levelID);
    std::string length  = lengthString(level->m_levelLength);
    std::string objects = std::to_string((int)level->m_objectCount);
    std::string text;

    if (mod->getSettingValue<bool>("use-custom-text")) {
        text = getCustomTextFromFile();
        // replace simple vars
        auto replace = [&](std::string const& from, std::string const& to) {
            size_t pos = 0;
            while ((pos = text.find(from, pos)) != std::string::npos) {
                text.replace(pos, from.size(), to);
                pos += to.size();
            }
        };
        replace("{creator}", creator);
        replace("{name}",    name);
        replace("{id}",      id);
        replace("{lengh}",   length);
        replace("{objects}", objects);
        // role ping in custom text via {role}
        replace("{role}", rolePing ? fmt::format("<@&{}>", roleID) : "");
        text = processConditionals(text, isUpdate);
    } else {
        text = isUpdate
            ? fmt::format("## Level Updated!\n{} Updated a level!\n- Name: {}\n- ID:   {}\n-# {} ({} objects)", creator, name, id, length, objects)
            : fmt::format("## New Level!\n{} dropped a new level!\n- Name: {}\n- ID:  {}\n-# {} ({} objects)", creator, name, id, length, objects);
        // role ping goes at the bottom spoilered in the preset
        if (rolePing)
            text += fmt::format("\n||<@&{}>||", roleID);
    }
    return text;
}

// Fire and forget, sends the webhook, logs success/failure
static void sendWebhook(GJGameLevel* level, bool isUpdate) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    std::string webhookURL = Mod::get()->getSettingValue<std::string>("webhook-url");
    if (webhookURL.empty()) {
        log::warn("Webhook URL is empty, skipping");
        return;
    }
    if (!webhookURL.starts_with("https://"))
        webhookURL = "https://" + webhookURL;
    auto json = matjson::Value();
    json["content"] = buildMessage(level, isUpdate);
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(json);
    // async::spawn
    async::spawn(
        req.post(webhookURL),
        [](web::WebResponse res) {
            if (res.ok()) {
                log::info("Webhook sent successfully ({})", res.code());
            } else {
                log::warn("Webhook failed with status {}: {}",
                    res.code(), res.string().unwrapOr("no body"));
            }
        }
    );
}

class $modify(UploadPopup) {
    void levelUploadFinished(GJGameLevel* level) {
        UploadPopup::levelUploadFinished(level);
        // m_levelVersion > 1 means the level already on the servers
        bool isUpdate = level->m_levelVersion > 1;
        if (isUpdate && !Mod::get()->getSettingValue<bool>("send-on-update")) return;
        sendWebhook(level, isUpdate);
    }
};
