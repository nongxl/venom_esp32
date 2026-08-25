#include "ConfigManager.h"

ConfigManager::ConfigManager() {}

void ConfigManager::init() {
    prefs.begin("venom_cfg", false);

    // 优先读取 NVS 中用户保存的配置
    wifi_ssid     = prefs.getString("ssid", "");
    wifi_pass     = prefs.getString("pass", "");
    llm_url       = prefs.getString("url", "");
    llm_key       = prefs.getString("key", "");
    llm_model     = prefs.getString("model", "");
    llm_provider  = prefs.getString("prov", "deepseek");

    // 若 NVS 为空，则采用 secrets.h / secrets_example.h 的默认配置
    if (wifi_ssid.length() == 0) {
        wifi_ssid = DEFAULT_SSID;
        wifi_pass = DEFAULT_PASSWORD;
    }
    if (llm_url.length() == 0) {
        llm_url = LLM_URL;
    }
    if (llm_key.length() == 0) {
        llm_key = DEFAULT_API_KEY;
    }
    if (llm_model.length() == 0) {
        llm_model = AI_MODEL;
    }

    Serial.printf(">>> [Config] Initialized. SSID: %s | Model: %s | Key: %s\n",
                  wifi_ssid.c_str(), llm_model.c_str(),
                  (llm_key.length() > 6) ? (llm_key.substring(0, 4) + "****").c_str() : "(empty)");
}

void ConfigManager::saveConfig(const String &ssid, const String &pass,
                              const String &url, const String &key,
                              const String &model, const String &provider) {
    wifi_ssid = ssid;
    wifi_pass = pass;
    llm_url = url;
    llm_key = key;
    llm_model = model;
    llm_provider = provider;

    prefs.putString("ssid", wifi_ssid);
    prefs.putString("pass", wifi_pass);
    prefs.putString("url", llm_url);
    prefs.putString("key", llm_key);
    prefs.putString("model", llm_model);
    prefs.putString("prov", llm_provider);

    Serial.println(">>> [Config] New configuration saved to NVS successfully!");
}

void ConfigManager::clearConfig() {
    prefs.clear();
    wifi_ssid = DEFAULT_SSID;
    wifi_pass = DEFAULT_PASSWORD;
    llm_url = LLM_URL;
    llm_key = DEFAULT_API_KEY;
    llm_model = AI_MODEL;
    llm_provider = "deepseek";
    Serial.println(">>> [Config] Configuration cleared, reverted to defaults.");
}
