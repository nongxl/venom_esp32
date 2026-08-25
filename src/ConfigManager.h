#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class ConfigManager {
public:
    static ConfigManager& instance() {
        static ConfigManager inst;
        return inst;
    }

    void init();

    String getWifiSSID() const { return wifi_ssid; }
    String getWifiPass() const { return wifi_pass; }
    String getLLMUrl() const { return llm_url; }
    String getLLMKey() const { return llm_key; }
    String getLLMModel() const { return llm_model; }
    String getLLMProvider() const { return llm_provider; }

    bool isConfigured() const { return wifi_ssid.length() > 0 && llm_key.length() > 0; }

    void saveConfig(const String &ssid, const String &pass,
                    const String &url, const String &key,
                    const String &model, const String &provider = "");

    void clearConfig();

private:
    ConfigManager();
    Preferences prefs;

    String wifi_ssid;
    String wifi_pass;
    String llm_url;
    String llm_key;
    String llm_model;
    String llm_provider;
};
