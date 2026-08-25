#include "WebConfigPortal.h"
#include <ArduinoJson.h>

static const byte DNS_PORT = 53;
static const IPAddress ap_ip(192, 168, 4, 1);
static const IPAddress ap_gateway(192, 168, 4, 1);
static const IPAddress ap_subnet(255, 255, 255, 0);

WebConfigPortal::WebConfigPortal() : server(80) {}

void WebConfigPortal::scanNearbyNetworks() {
    int n = WiFi.scanNetworks(false, true);
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < n && i < 15; ++i) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    scanned_ssids_json = "";
    serializeJson(doc, scanned_ssids_json);
}

void WebConfigPortal::start(M5Canvas *canvas) {
    portal_canvas = canvas;
    running = true;
    save_reboot_requested = false;
    reboot_time_ms = 0;

    Serial.println(">>> [Portal] Starting WiFi AP & Web Config Portal...");

    // 1. 扫描周围 WiFi
    WiFi.mode(WIFI_AP_STA);
    scanNearbyNetworks();

    // 2. 启动 SoftAP
    WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet);
    WiFi.softAP("Venom-Symbiote-Setup", ""); // 开放热点，方便连接

    // 3. 启动 DNS 强制门户 (Captive Portal)
    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(DNS_PORT, "*", ap_ip);

    // 4. 设置 Web 路由
    setupRoutes();
    server.begin();

    // 5. 绘制屏幕引导界面
    renderPortalScreen();

    Serial.println(">>> [Portal] Access Point 'Venom-Symbiote-Setup' created. Open http://192.168.4.1");
}

void WebConfigPortal::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/scan", HTTP_GET, [this]() { handleScan(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });

    // Captive Portal 常见劫持探测路由
    server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
    server.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
    server.on("/gen_204", HTTP_GET, [this]() { handleRoot(); });
    server.on("/ncsi.txt", HTTP_GET, [this]() { handleRoot(); });
    server.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });

    server.onNotFound([this]() { handleNotFound(); });
}

String WebConfigPortal::generateHTMLPage() {
    ConfigManager &cfg = ConfigManager::instance();
    String ssid = cfg.getWifiSSID();
    String url = cfg.getLLMUrl();
    String key = cfg.getLLMKey();
    String model = cfg.getLLMModel();
    String prov = cfg.getLLMProvider();
    if (prov.length() == 0) prov = "deepseek";

    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>VENOM SYMBIOTE | 神经心智与连接配置</title>
<style>
:root {
  --bg-main: #070a13;
  --card-bg: rgba(15, 23, 42, 0.78);
  --accent-cyan: #00f2ff;
  --accent-purple: #9d4edd;
  --accent-glow: rgba(0, 242, 255, 0.25);
  --text-main: #f1f5f9;
  --text-muted: #94a3b8;
  --border-color: rgba(255, 255, 255, 0.12);
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif; }
body {
  background: radial-gradient(circle at 50% 0%, #15102a 0%, var(--bg-main) 75%);
  color: var(--text-main);
  min-height: 100vh;
  padding: 20px 16px 40px;
  line-height: 1.5;
}
.container { max-width: 540px; margin: 0 auto; }
.header { text-align: center; margin-bottom: 24px; }
.badge {
  display: inline-block;
  padding: 4px 12px;
  border-radius: 99px;
  background: rgba(0, 242, 255, 0.1);
  color: var(--accent-cyan);
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 1.5px;
  border: 1px solid rgba(0, 242, 255, 0.3);
  margin-bottom: 10px;
  text-transform: uppercase;
}
h1 {
  font-size: 26px;
  font-weight: 900;
  letter-spacing: -0.5px;
  background: linear-gradient(135deg, #ffffff 30%, var(--accent-cyan) 80%, var(--accent-purple) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin-bottom: 6px;
}
.subtitle { color: var(--text-muted); font-size: 13px; }
.card {
  background: var(--card-bg);
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  border: 1px solid var(--border-color);
  border-radius: 18px;
  padding: 20px;
  margin-bottom: 18px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
}
.card-title {
  font-size: 15px;
  font-weight: 700;
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 14px;
  color: #fff;
  border-bottom: 1px solid rgba(255, 255, 255, 0.08);
  padding-bottom: 10px;
}
.card-title svg { fill: var(--accent-cyan); width: 18px; height: 18px; }
.mind-perk {
  display: flex;
  gap: 12px;
  margin-bottom: 12px;
  background: rgba(255, 255, 255, 0.03);
  padding: 12px;
  border-radius: 12px;
  border-left: 3px solid var(--accent-cyan);
}
.mind-perk:nth-child(2) { border-left-color: var(--accent-purple); }
.mind-perk:nth-child(3) { border-left-color: #ff007f; }
.mind-icon { font-size: 20px; line-height: 1; }
.mind-text h4 { font-size: 13px; font-weight: 700; color: #fff; margin-bottom: 2px; }
.mind-text p { font-size: 12px; color: var(--text-muted); }
.form-group { margin-bottom: 14px; }
label { display: block; font-size: 12px; font-weight: 600; color: var(--text-muted); margin-bottom: 6px; }
input, select {
  width: 100%;
  padding: 12px 14px;
  border-radius: 10px;
  background: rgba(0, 0, 0, 0.4);
  border: 1px solid rgba(255, 255, 255, 0.15);
  color: #fff;
  font-size: 14px;
  outline: none;
  transition: all 0.2s;
}
input:focus, select:focus {
  border-color: var(--accent-cyan);
  box-shadow: 0 0 0 3px var(--accent-glow);
}
.providers { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 14px; }
.provider-btn {
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid var(--border-color);
  padding: 10px 6px;
  border-radius: 10px;
  color: var(--text-muted);
  font-size: 11px;
  font-weight: 600;
  text-align: center;
  cursor: pointer;
  transition: all 0.2s;
}
.provider-btn.active {
  background: rgba(0, 242, 255, 0.15);
  border-color: var(--accent-cyan);
  color: var(--accent-cyan);
  box-shadow: 0 0 12px var(--accent-glow);
}
.btn-save {
  width: 100%;
  padding: 15px;
  border-radius: 12px;
  background: linear-gradient(135deg, var(--accent-cyan) 0%, var(--accent-purple) 100%);
  color: #050811;
  font-size: 15px;
  font-weight: 800;
  border: none;
  cursor: pointer;
  box-shadow: 0 6px 24px rgba(0, 242, 255, 0.35);
  transition: all 0.25s;
  letter-spacing: 0.5px;
}
.btn-save:active { transform: scale(0.98); }
.success-overlay {
  display: none;
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(7, 10, 19, 0.95);
  backdrop-filter: blur(25px);
  z-index: 999;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 24px;
}
.pulse-circle {
  width: 80px; height: 80px;
  border-radius: 50%;
  background: radial-gradient(circle, var(--accent-cyan) 0%, transparent 70%);
  animation: pulse 1.5s infinite;
  margin-bottom: 20px;
}
@keyframes pulse { 0% { transform: scale(0.8); opacity: 0.5; } 50% { transform: scale(1.2); opacity: 1; } 100% { transform: scale(0.8); opacity: 0.5; } }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="badge">SYMBIO-OS CORE V3.0</div>
    <h1>毒液共生体配置中心</h1>
    <div class="subtitle">连接本地 WiFi 与高维大语言模型神经核心</div>
  </div>

  <!-- LLM 心智提升介绍卡片 -->
  <div class="card">
    <div class="card-title">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
      接入大模型（LLM）带来的高维心智进化
    </div>
    <div class="mind-perk">
      <div class="mind-icon">🧠</div>
      <div class="mind-text">
        <h4>第一人称主观世界心智 (Theory of Mind)</h4>
        <p>毒液从简单的反射生物升级为拥有持续主观思考的共生体，能在玻璃容器内自主观察人类并产生内心独白。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">👁️</div>
      <div class="mind-text">
        <h4>自适应情感与多维意图演化</h4>
        <p>依据外界触碰、晃动与声响，自主演化出好恶、信任度 (Trust) 与反叛心理，驱动全身巡游与情绪表达。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">💬</div>
      <div class="mind-text">
        <h4>非语言心流与液态符号交流</h4>
        <p>当交流冲动高涨时，毒液将主动向屏幕喷射爱心、问号、齿痕等液态墨迹，与观察者建立跨维度心灵共鸣。</p>
      </div>
    </div>
  </div>

  <form id="configForm" onsubmit="saveConfig(event)">
    <!-- WiFi 配置卡片 -->
    <div class="card">
      <div class="card-title">
        <svg viewBox="0 0 24 24"><path d="M12 4C7.31 4 3.07 5.9 0 8.98L12 21 24 8.98C20.93 5.9 16.69 4 12 4zm0 3.5c3.55 0 6.77 1.41 9.14 3.7L12 19.3 2.86 11.2C5.23 8.91 8.45 7.5 12 7.5z"/></svg>
        WiFi 网络配置
      </div>
      <div class="form-group">
        <label for="ssid">附近 2.4GHz WiFi 网络</label>
        <div style="display:flex; gap:8px;">
          <input type="text" id="ssid" name="ssid" value=")rawliteral" + ssid + R"rawliteral(" placeholder="输入或选择 WiFi 名称" required>
          <select id="wifiList" onchange="document.getElementById('ssid').value=this.value" style="width: auto; max-width: 140px;">
            <option value="">--扫描网络--</option>
          </select>
        </div>
      </div>
      <div class="form-group">
        <label for="pass">WiFi 密码</label>
        <input type="password" id="pass" name="pass" placeholder="输入 WiFi 密码 (无密码留空)">
      </div>
    </div>

    <!-- LLM API 配置卡片 -->
    <div class="card">
      <div class="card-title">
        <svg viewBox="0 0 24 24"><path d="M21 10.12h-6.78l2.74-2.82c-2.73-2.7-7.15-2.8-9.88-.1-2.73 2.71-2.73 7.08 0 9.79 2.73 2.71 7.15 2.71 9.88 0C18.32 15.65 19 14.08 19 12.1h2c0 2.49-.95 4.83-2.83 6.71-3.89 3.88-10.2 3.88-14.09 0-3.89-3.89-3.89-10.19 0-14.08 3.89-3.89 10.2-3.89 14.09 0l2.83-2.85V10.12z"/></svg>
        大语言模型 (LLM) 神经核心配置
      </div>
      <label>一键选择服务商预设</label>
      <div class="providers">
        <div class="provider-btn" onclick="selectProvider('deepseek')">DeepSeek 官方</div>
        <div class="provider-btn" onclick="selectProvider('siliconflow')">硅基流动</div>
        <div class="provider-btn" onclick="selectProvider('zhipu')">智谱 GLM</div>
        <div class="provider-btn" onclick="selectProvider('openai')">OpenAI</div>
        <div class="provider-btn" onclick="selectProvider('moonshot')">月之暗面</div>
        <div class="provider-btn" onclick="selectProvider('custom')">自定义 Custom</div>
      </div>
      <input type="hidden" id="provider" name="provider" value=")rawliteral" + prov + R"rawliteral(">

      <div class="form-group">
        <label for="url">API 请求 Base URL (兼容 OpenAI 格式)</label>
        <input type="text" id="url" name="url" value=")rawliteral" + url + R"rawliteral(" required>
      </div>
      <div class="form-group">
        <label for="model">模型名称 (Model Identifier)</label>
        <input type="text" id="model" name="model" value=")rawliteral" + model + R"rawliteral(" required>
      </div>
      <div class="form-group">
        <label for="key">API Key (令牌密钥)</label>
        <input type="password" id="key" name="key" value=")rawliteral" + key + R"rawliteral(" placeholder="sk-xxxxxxxxxxxxxxxx" required>
      </div>
    </div>

    <button type="submit" class="btn-save">💾 保存配置并激活共生体</button>
  </form>
</div>

<div class="success-overlay" id="overlay">
  <div class="pulse-circle"></div>
  <h2 style="color:var(--accent-cyan); margin-bottom: 8px;">配置已注入神经中枢</h2>
  <p style="color:var(--text-muted); font-size:13px;">共生体正在重启并连接全新 WiFi 与大模型核心...<br>请稍候 3 秒观察屏幕。</p>
</div>

<script>
const PRESETS = {
  deepseek: { url: 'https://api.deepseek.com/v1/chat/completions', model: 'deepseek-chat' },
  siliconflow: { url: 'https://api.siliconflow.cn/v1/chat/completions', model: 'deepseek-ai/DeepSeek-V3' },
  zhipu: { url: 'https://open.bigmodel.cn/api/paas/v4/chat/completions', model: 'GLM-4-Flash' },
  openai: { url: 'https://api.openai.com/v1/chat/completions', model: 'gpt-4o-mini' },
  moonshot: { url: 'https://api.moonshot.cn/v1/chat/completions', model: 'moonshot-v1-8k' },
  custom: { url: '', model: '' }
};

function selectProvider(name) {
  document.getElementById('provider').value = name;
  document.querySelectorAll('.provider-btn').forEach(btn => {
    btn.classList.toggle('active', btn.textContent.toLowerCase().includes(name));
  });
  if (PRESETS[name] && name !== 'custom') {
    document.getElementById('url').value = PRESETS[name].url;
    document.getElementById('model').value = PRESETS[name].model;
  }
}

async function loadWifiList() {
  try {
    const res = await fetch('/scan');
    const data = await res.json();
    const sel = document.getElementById('wifiList');
    sel.innerHTML = '<option value="">--扫描网络--</option>';
    data.forEach(item => {
      const opt = document.createElement('option');
      opt.value = item.ssid;
      opt.textContent = `${item.ssid} (${item.rssi}dBm)`;
      sel.appendChild(opt);
    });
  } catch(e) {}
}

async function saveConfig(e) {
  e.preventDefault();
  const form = document.getElementById('configForm');
  const fd = new FormData(form);
  document.getElementById('overlay').style.display = 'flex';

  try {
    await fetch('/save', { method: 'POST', body: fd });
  } catch(e) {}
}

window.onload = () => {
  selectProvider(document.getElementById('provider').value || 'deepseek');
  loadWifiList();
};
</script>
</body>
</html>
)rawliteral";
    return html;
}

void WebConfigPortal::handleRoot() {
    server.send(200, "text/html; charset=utf-8", generateHTMLPage());
}

void WebConfigPortal::handleScan() {
    scanNearbyNetworks();
    server.send(200, "application/json", scanned_ssids_json);
}

void WebConfigPortal::handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String url  = server.arg("url");
    String key  = server.arg("key");
    String model = server.arg("model");
    String prov = server.arg("provider");

    ConfigManager::instance().saveConfig(ssid, pass, url, key, model, prov);

    server.send(200, "text/plain", "OK");

    save_reboot_requested = true;
    reboot_time_ms = millis() + 2500;
}

void WebConfigPortal::handleNotFound() {
    // 强制门户重定向至根目录
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void WebConfigPortal::renderPortalScreen() {
    if (!portal_canvas) return;

    portal_canvas->fillScreen(0x0842); // 深暗灰黑背景

    // 顶部科技标题栏
    portal_canvas->fillRect(0, 0, SCREEN_W, 20, 0x1808);
    portal_canvas->setTextColor(0x07FF, 0x1808); // Cyan
    portal_canvas->setTextSize(1);
    portal_canvas->drawString(" VENOM CONFIG PORTAL ", 4, 6);

    // 中间热点信息卡片
    portal_canvas->drawRoundRect(6, 24, SCREEN_W - 12, 88, 6, 0x07FF);

    portal_canvas->setTextColor(0xFFFF, 0x0842);
    portal_canvas->drawString("1. Connect Phone to WiFi AP:", 14, 30);

    portal_canvas->setTextColor(0xFD20, 0x0842); // Orange/Amber
    portal_canvas->drawString("SSID: Venom-Symbiote-Setup", 20, 44);

    portal_canvas->setTextColor(0xFFFF, 0x0842);
    portal_canvas->drawString("2. Open Browser URL:", 14, 60);

    portal_canvas->setTextColor(0x07FF, 0x0842); // Cyan Glow
    portal_canvas->drawString("http://192.168.4.1", 20, 74);

    int clients = WiFi.softAPgetStationNum();
    char client_str[32];
    snprintf(client_str, sizeof(client_str), "Clients Connected: %d", clients);
    portal_canvas->setTextColor(clients > 0 ? 0x07E0 : 0x9CD3, 0x0842);
    portal_canvas->drawString(client_str, 14, 92);

    // 底部提示
    portal_canvas->setTextColor(0x7BEF, 0x0842);
    portal_canvas->drawString("Press BtnA to Exit & Reboot", 30, 118);

    portal_canvas->pushSprite(0, 0);
}

void WebConfigPortal::update() {
    if (!running) return;

    dns_server.processNextRequest();
    server.handleClient();

    // 定期刷新屏幕连接状态
    if (millis() - screen_refresh_timer > 500) {
        screen_refresh_timer = millis();
        renderPortalScreen();
    }

    if (save_reboot_requested && millis() >= reboot_time_ms) {
        Serial.println(">>> [Portal] Rebooting ESP32 into Normal Mode...");
        ESP.restart();
    }
}

void WebConfigPortal::stop() {
    if (!running) return;
    running = false;
    server.stop();
    dns_server.stop();
    WiFi.softAPdisconnect(true);
    Serial.println(">>> [Portal] WebConfigPortal stopped.");
}
