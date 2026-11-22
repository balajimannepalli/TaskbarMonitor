#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path path);
    static std::filesystem::path defaultPath();
    static nlohmann::json defaults();
    static nlohmann::json validate(const nlohmann::json& input);
    void load();
    void save(const nlohmann::json& settings);
    const nlohmann::json& settings() const { return settings_; }
    bool writable() const { return writable_; }
private:
    std::filesystem::path path_;
    nlohmann::json settings_;
    std::string original_;
    bool writable_ = true;
};
