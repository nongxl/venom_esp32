#include "WebConfigPortal.h"
#include <ArduinoJson.h>

static const byte DNS_PORT = 53;
static const IPAddress ap_ip(192, 168, 4, 1);
static const IPAddress ap_gateway(192, 168, 4, 1);
static const IPAddress ap_subnet(255, 255, 255, 0);

WebConfigPortal::WebConfigPortal() : server(80) {}

void WebConfigPortal::scanNearbyNetworks() {
    WiFi.scanDelete();
    int n = WiFi.scanNetworks(false, false);
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    if (n > 0) {
        for (int i = 0; i < n && i < 15; ++i) {
            JsonObject obj = arr.createNestedObject();
            obj["ssid"] = WiFi.SSID(i);
            obj["rssi"] = WiFi.RSSI(i);
            obj["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
    }
    WiFi.scanDelete();

    scanned_ssids_json = "";
    serializeJson(doc, scanned_ssids_json);
}

void WebConfigPortal::start(M5Canvas *canvas) {
    if (running) {
        Serial.println(">>> [Portal] Already running, ignore duplicate start.");
        return;
    }
    portal_canvas = canvas;
    running = true;
    blocking_screen = true;
    save_reboot_requested = false;
    reboot_time_ms = 0;

    Serial.println(">>> [Portal] Starting WiFi AP & Web Config Portal...");

    // 1. 优雅切断 STA 后台重连，防止通道冲突与看门狗超时/断电重启
    WiFi.disconnect();
    delay(50);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_15dBm); // 稳定安全发射功率，防止USB瞬态跌落
    delay(50);

    // 2. 启动 SoftAP (无需在启动时同步阻塞扫描 WiFi，等待网页端异步 /scan 请求)
    WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet);
    bool ap_ok = WiFi.softAP("Venom-Symbiote-Setup", "", 1); // 固定信道 1
    Serial.printf(">>> [Portal] SoftAP started: %s (IP: 192.168.4.1)\n", ap_ok ? "SUCCESS" : "FAILED");

    // 3. 启动 DNS 强制门户 (Captive Portal)
    dns_server.stop();
    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(DNS_PORT, "*", ap_ip);

    // 4. 设置 Web 路由并启动 HTTP Server
    if (!routes_configured) {
        setupRoutes();
        routes_configured = true;
    }
    server.stop();
    server.begin();

    // 5. 绘制屏幕引导界面
    renderPortalScreen();

    Serial.println(">>> [Portal] Access Point 'Venom-Symbiote-Setup' created. Open http://192.168.4.1");
}

void WebConfigPortal::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/scan", HTTP_GET, [this]() { handleScan(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/api/demo", HTTP_GET, [this]() { handleDemo(); });
    server.on("/api/demo", HTTP_POST, [this]() { handleDemo(); });

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
<title>VENOM SYMBIOTE | 神经心智控制台</title>
<style>
:root {
  --bg-deep: #020714;
  --bg-sapphire-top: #184c8a;
  --bg-sapphire-mid: #0b254e;
  --bg-sapphire-deep: #051329;
  --card-bg: rgba(6, 15, 34, 0.85);
  --venom-black: #05060a;
  --accent-cyan: #00f0ff;
  --accent-blue: #1b6cd8;
  --accent-glow: rgba(0, 240, 255, 0.35);
  --text-main: #f0f6ff;
  --text-muted: #8ba5c7;
  --border-color: rgba(0, 240, 255, 0.22);
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif; }
body {
  background: radial-gradient(circle at 50% -5%, var(--bg-sapphire-top) 0%, var(--bg-sapphire-mid) 35%, var(--bg-sapphire-deep) 72%, var(--bg-deep) 100%);
  color: var(--text-main);
  min-height: 100vh;
  padding: 24px 16px 130px;
  position: relative;
  overflow-x: hidden;
  line-height: 1.5;
}
/* 粘稠黑色流体顶部滴漏装饰条 */
.slime-banner {
  position: absolute;
  top: 0; left: 0; width: 100%; height: 8px;
  background: linear-gradient(90deg, #000, #040812 50%, #000);
  box-shadow: 0 3px 18px rgba(0, 240, 255, 0.3);
}
.container { max-width: 550px; margin: 0 auto; position: relative; z-index: 10; }
.header { text-align: center; margin-bottom: 24px; position: relative; }
.badge {
  display: inline-block;
  padding: 4px 14px;
  border-radius: 99px;
  background: rgba(0, 240, 255, 0.12);
  color: var(--accent-cyan);
  font-size: 11px;
  font-weight: 800;
  letter-spacing: 2px;
  border: 1px solid rgba(0, 240, 255, 0.35);
  margin-bottom: 10px;
  text-transform: uppercase;
  box-shadow: 0 0 16px rgba(0, 240, 255, 0.2);
}
h1 {
  font-size: 26px;
  font-weight: 900;
  letter-spacing: -0.5px;
  background: linear-gradient(135deg, #ffffff 20%, var(--accent-cyan) 75%, var(--accent-blue) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin-bottom: 6px;
  text-shadow: 0 0 25px rgba(0, 240, 255, 0.25);
}
.subtitle { color: var(--text-muted); font-size: 13px; }
.card {
  background: var(--card-bg);
  backdrop-filter: blur(22px);
  -webkit-backdrop-filter: blur(22px);
  border: 1px solid var(--border-color);
  border-radius: 18px;
  padding: 22px;
  margin-bottom: 20px;
  box-shadow: 0 12px 38px rgba(2, 6, 18, 0.75), inset 0 1px 0 rgba(255, 255, 255, 0.1);
  position: relative;
  overflow: hidden;
}
.card::before {
  content: "";
  position: absolute;
  top: 0; left: 0; right: 0; height: 3px;
  background: linear-gradient(90deg, transparent, var(--accent-cyan), transparent);
  opacity: 0.6;
}
.card-title {
  font-size: 15px;
  font-weight: 800;
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 14px;
  color: #fff;
  border-bottom: 1px solid rgba(0, 240, 255, 0.12);
  padding-bottom: 10px;
  letter-spacing: 0.3px;
}
.card-title svg { fill: var(--accent-cyan); width: 18px; height: 18px; filter: drop-shadow(0 0 6px var(--accent-cyan)); }
.mind-perk {
  display: flex;
  gap: 12px;
  margin-bottom: 10px;
  background: rgba(3, 8, 20, 0.65);
  padding: 12px;
  border-radius: 12px;
  border-left: 3px solid var(--accent-cyan);
  border-top: 1px solid rgba(0, 240, 255, 0.1);
}
.mind-perk:nth-child(2) { border-left-color: #3b82f6; }
.mind-perk:nth-child(3) { border-left-color: #06b6d4; }
.mind-icon { font-size: 20px; line-height: 1; }
.mind-text h4 { font-size: 13px; font-weight: 700; color: #fff; margin-bottom: 2px; }
.mind-text p { font-size: 12px; color: var(--text-muted); }

/* 动作测试按钮样式 (黑色粘稠流体科技质感) */
.demo-btn {
  background: linear-gradient(145deg, rgba(14, 28, 54, 0.95), rgba(4, 9, 20, 0.98));
  border: 1px solid rgba(0, 240, 255, 0.32);
  color: #e2eeff;
  padding: 11px 8px;
  border-radius: 11px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.22s cubic-bezier(0.16, 1, 0.3, 1);
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 5px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4), inset 0 1px 0 rgba(255, 255, 255, 0.08);
}
.demo-btn:hover {
  background: linear-gradient(145deg, rgba(0, 240, 255, 0.22), rgba(8, 22, 45, 0.95));
  border-color: var(--accent-cyan);
  color: #ffffff;
  transform: translateY(-2px);
  box-shadow: 0 6px 18px rgba(0, 240, 255, 0.35);
}
.demo-btn:active {
  transform: translateY(1px) scale(0.98);
}

.form-group { margin-bottom: 14px; }
label { display: block; font-size: 12px; font-weight: 600; color: var(--text-muted); margin-bottom: 6px; }
input, select {
  width: 100%;
  padding: 12px 14px;
  border-radius: 10px;
  background: rgba(2, 6, 16, 0.7);
  border: 1px solid rgba(0, 240, 255, 0.18);
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
  background: rgba(4, 11, 25, 0.7);
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
  background: rgba(0, 240, 255, 0.18);
  border-color: var(--accent-cyan);
  color: var(--accent-cyan);
  box-shadow: 0 0 14px var(--accent-glow);
}
.btn-save {
  width: 100%;
  padding: 15px;
  border-radius: 12px;
  background: linear-gradient(135deg, var(--accent-cyan) 0%, var(--accent-blue) 100%);
  color: #030814;
  font-size: 15px;
  font-weight: 800;
  border: none;
  cursor: pointer;
  box-shadow: 0 6px 24px rgba(0, 240, 255, 0.4);
  transition: all 0.25s;
  letter-spacing: 0.5px;
}
.btn-save:active { transform: scale(0.98); }
.success-overlay {
  display: none;
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(3, 8, 20, 0.96);
  backdrop-filter: blur(25px);
  z-index: 999;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}
.pulse-circle {
  width: 80px; height: 80px;
  border-radius: 50%;
  background: radial-gradient(circle, var(--accent-cyan) 0%, transparent 70%);
  animation: pulse 1.5s infinite;
  margin-bottom: 20px;
}
@keyframes pulse { 0% { transform: scale(0.8); opacity: 0.5; } 50% { transform: scale(1.2); opacity: 1; } 100% { transform: scale(0.8); opacity: 0.5; } }

/* ── 趴在屏幕边缘的活体共生体 (Living Edge Venom) ── */
.symbiote-edge {
  position: fixed;
  bottom: 0;
  right: 24px;
  width: 220px;
  height: 110px;
  z-index: 9999;
  cursor: pointer;
  filter: drop-shadow(0 -4px 18px rgba(0, 240, 255, 0.3));
  transition: transform 0.25s cubic-bezier(0.175, 0.885, 0.32, 1.275);
  user-select: none;
  -webkit-user-select: none;
  touch-action: manipulation;
}
.symbiote-edge:hover {
  transform: translateY(-6px) scale(1.03);
}
.symbiote-edge:active {
  transform: translateY(3px) scale(0.97);
}
.symbiote-speech {
  position: absolute;
  top: -30px;
  left: 50%;
  transform: translateX(-50%) translateY(4px);
  background: rgba(3, 9, 22, 0.94);
  border: 1px solid var(--accent-cyan);
  color: var(--accent-cyan);
  font-size: 11.5px;
  font-weight: 700;
  padding: 4px 12px;
  border-radius: 99px;
  white-space: nowrap;
  pointer-events: none;
  opacity: 0;
  transition: all 0.25s ease;
  box-shadow: 0 0 16px var(--accent-glow);
}
.symbiote-speech.active {
  opacity: 1;
  transform: translateX(-50%) translateY(0);
}
@media (max-width: 600px) {
  .symbiote-edge {
    right: 12px;
    width: 170px;
    height: 85px;
  }
  #venomCanvas {
    width: 170px;
    height: 85px;
  }
}
</style>
</head>
<body>
<div class="slime-banner"></div>
<div class="container">
  <div class="header">
    <div class="badge">VENOM SYMBIOTE OS</div>
    <h1>神经心智与动作演进中枢</h1>
    <div class="subtitle">连接蓝色高维神经脉冲与多维动作拟态实验室</div>
  </div>

  <div class="card">
    <div class="card-title">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
      当前共生体生物心智特性
    </div>
    <div class="mind-perk">
      <div class="mind-icon">🧠</div>
      <div class="mind-text">
        <h4>自主演化感知状态机</h4>
        <p>支持发呆、巡游、爬行、蹦床、颠球、翻滚、荡秋千与边缘暗中窥视，随生理代谢自主起居。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">👁️</div>
      <div class="mind-text">
        <h4>边缘暗中伏击与张望</h4>
        <p>好奇心高且充满警惕时，贴在屏幕边缘潜行巡游，只露双眼，时不时探头张望又机警缩回。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">💬</div>
      <div class="mind-text">
        <h4>非语言心流与液态符号交流</h4>
        <p>交流冲动高涨或受互动刺激时，破空喷出水墨爱心、问号、叹号与旋律音符。</p>
      </div>
    </div>
  </div>

  <!-- 共生体行为拟态演示与测试控制台 -->
  <div class="card" style="border: 1px solid rgba(0, 240, 255, 0.35); background: rgba(5, 13, 30, 0.9); box-shadow: 0 8px 30px rgba(0, 240, 255, 0.15);">
    <div class="card-title" style="color: var(--accent-cyan);">
      <svg viewBox="0 0 24 24" style="fill: var(--accent-cyan);"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
      🧪 共生体行为拟态演示与测试控制台
    </div>
    <p style="font-size: 13px; color: var(--text-muted); margin-bottom: 14px; line-height: 1.5;">
      点选目标动作，系统将自动调谐毒液的生理心理数值至临界状态，并在 0.8 秒仿生准备后，自然过渡到演示动作（设备屏幕将实时呈现拟态演进）：
    </p>
    <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(135px, 1fr)); gap: 9px; margin-bottom: 12px;">
      <button type="button" class="demo-btn" onclick="triggerAction('peek')">👁️ 边缘暗中观察</button>
      <button type="button" class="demo-btn" onclick="triggerAction('bounce')">🚀 史莱姆蹦蹦床</button>
      <button type="button" class="demo-btn" onclick="triggerAction('ball_play')">⚽ 自体分裂颠球</button>
      <button type="button" class="demo-btn" onclick="triggerAction('swing')">🕸️ 吸顶蛛丝秋千</button>
      <button type="button" class="demo-btn" onclick="triggerAction('roll')">🌀 索尼克软体翻滚</button>
      <button type="button" class="demo-btn" onclick="triggerAction('catch_dust')">🐾 猎捕抓微粒</button>
      <button type="button" class="demo-btn" onclick="triggerAction('crawl')">🧗 触手大步攀爬</button>
      <button type="button" class="demo-btn" onclick="triggerAction('creep')">🐛 足丝地表巡游</button>
      <button type="button" class="demo-btn" onclick="triggerAction('sleep')">💤 深度安稳睡眠</button>
      <button type="button" class="demo-btn" onclick="triggerAction('bat_hang')">🦇 天花板倒挂小憩</button>
      <button type="button" class="demo-btn" onclick="triggerAction('irritate')">⚡ 激惹青色脉冲</button>
    </div>
    <div id="demoStatus" style="font-size: 12px; color: var(--accent-cyan); min-height: 20px; text-align: center; font-weight: 600;"></div>
  </div>

  <form id="configForm" onsubmit="saveConfig(event)">
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
        <label for="url">API 请求 Base URL</label>
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

<!-- 趴在屏幕边缘的活体共生体 (Living Edge Venom) -->
<div id="symbioteEdgeWidget" class="symbiote-edge" title="戳一戳毒液" onclick="pokeVenom(event)">
  <div class="symbiote-speech" id="symbioteSpeech">嘶... 正在观察你</div>
  <canvas id="venomCanvas" width="220" height="110"></canvas>
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

async function triggerAction(actionName) {
  const statusEl = document.getElementById('demoStatus');
  statusEl.innerText = '⚡ 正在注入心智参数并自然过渡至: ' + actionName + '...';
  if (window.triggerSymbioteExcitement) window.triggerSymbioteExcitement(actionName);
  try {
    const res = await fetch('/api/demo?action=' + actionName, { method: 'POST' });
    const data = await res.json();
    statusEl.innerText = '✅ 演示已激活: ' + actionName + ' (请直观欣赏设备屏幕演进)';
    setTimeout(() => { if (statusEl.innerText.includes(actionName)) statusEl.innerText = ''; }, 4000);
  } catch(e) {
    statusEl.innerText = '❌ 触发网络异常: ' + e;
  }
}

/* ── 活体边缘毒液渲染引擎 (Living Edge Venom Canvas Engine) ── */
(function initLivingVenom() {
  const canvas = document.getElementById('venomCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;

  let mouseX = window.innerWidth / 2;
  let mouseY = window.innerHeight / 2;
  window.addEventListener('mousemove', (e) => { mouseX = e.clientX; mouseY = e.clientY; });
  window.addEventListener('touchmove', (e) => {
    if (e.touches.length > 0) {
      mouseX = e.touches[0].clientX;
      mouseY = e.touches[0].clientY;
    }
  }, { passive: true });

  let time = 0;
  let blinkPhase = 0;
  let nextBlink = 2.5;
  let bobOffset = 0;
  let targetBob = 0;
  let bobTimer = 0;
  let squashX = 1.0, squashY = 1.0;

  const quotes = [
    "嘶... 观察者",
    "我们在注视着你",
    "心智同步率 100%",
    "感知到触碰信号",
    "随时准备行动！",
    "饥饿感正在蔓延...",
    "共生神经连接稳定"
  ];

  window.pokeVenom = function(e) {
    if (e) e.stopPropagation();
    squashX = 1.25; squashY = 0.72;
    blinkPhase = 1.0;
    const speech = document.getElementById('symbioteSpeech');
    if (speech) {
      speech.innerText = quotes[Math.floor(Math.random() * quotes.length)];
      speech.classList.add('active');
      clearTimeout(window.speechTimeout);
      window.speechTimeout = setTimeout(() => { speech.classList.remove('active'); }, 2600);
    }
  };

  window.triggerSymbioteExcitement = function(act) {
    squashX = 1.15; squashY = 0.85;
    targetBob = -16;
    const speech = document.getElementById('symbioteSpeech');
    if (speech) {
      speech.innerText = '⚡ 演示: ' + act;
      speech.classList.add('active');
      setTimeout(() => { speech.classList.remove('active'); }, 3000);
    }
  };

  function render() {
    time += 0.035;
    ctx.clearRect(0, 0, W, H);

    // 呼吸与弹跳阻尼
    const breath = Math.sin(time * 2.2) * 2.5;
    squashX += (1.0 - squashX) * 0.12;
    squashY += (1.0 - squashY) * 0.12;

    // 偶发探头打量 (Peek-a-boo bobbing)
    bobTimer += 0.016;
    if (bobTimer > 4.5) {
      bobTimer = 0;
      targetBob = (Math.random() < 0.6) ? -14 : 0;
    }
    bobOffset += (targetBob - bobOffset) * 0.08;

    // 头部中心基准
    const cx = W * 0.5;
    const cy = H - 28 + bobOffset + breath;

    // ── 1. 绘制粘稠黑色流体肉身 (Viscous Black Symbiote Blob) ──
    ctx.save();
    ctx.translate(cx, H);
    ctx.scale(squashX, squashY);
    ctx.translate(-cx, -H);

    // 外轮廓：液态胶质大肉团贴附屏幕底边
    ctx.beginPath();
    ctx.moveTo(8, H);
    ctx.bezierCurveTo(30, cy + 20, cx - 78, cy - 22, cx, cy - 25);
    ctx.bezierCurveTo(cx + 78, cy - 22, W - 30, cy + 20, W - 8, H);
    ctx.closePath();

    const bodyGrad = ctx.createRadialGradient(cx, cy - 8, 8, cx, cy + 20, 95);
    bodyGrad.addColorStop(0, '#101726');
    bodyGrad.addColorStop(0.35, '#070a12');
    bodyGrad.addColorStop(1, '#020306');
    ctx.fillStyle = bodyGrad;
    ctx.fill();

    // 宝蓝环境流体高光边缘 (Sapphire Specular Rim Light)
    ctx.lineWidth = 2.5;
    const rimGrad = ctx.createLinearGradient(cx - 70, cy - 25, cx + 70, cy - 25);
    rimGrad.addColorStop(0, 'rgba(0, 240, 255, 0.15)');
    rimGrad.addColorStop(0.5, 'rgba(0, 240, 255, 0.75)');
    rimGrad.addColorStop(1, 'rgba(27, 108, 216, 0.4)');
    ctx.strokeStyle = rimGrad;
    ctx.stroke();

    // 粘液表面水墨反光斑点
    ctx.beginPath();
    ctx.ellipse(cx - 24, cy - 14, 18, 5, -0.2, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(255, 255, 255, 0.12)';
    ctx.fill();

    // ── 2. 眼睛瞳孔视线追踪计算 ──
    const rect = canvas.getBoundingClientRect();
    const eyeScreenX = rect.left + cx;
    const eyeScreenY = rect.top + cy;
    const angleToMouse = Math.atan2(mouseY - eyeScreenY, mouseX - eyeScreenX);
    const distToMouse = Math.hypot(mouseX - eyeScreenX, mouseY - eyeScreenY);
    const maxLook = 4.5;
    const lookDist = Math.min(distToMouse * 0.02, maxLook);
    const pupilLookX = Math.cos(angleToMouse) * lookDist;
    const pupilLookY = Math.sin(angleToMouse) * lookDist;

    // 眨眼动画控制
    if (time > nextBlink) {
      blinkPhase += 0.25;
      if (blinkPhase > 2.0) {
        blinkPhase = 0;
        nextBlink = time + 2.5 + Math.random() * 3.5;
      }
    }
    const eyelidClose = (blinkPhase > 0 && blinkPhase < 2.0)
      ? (blinkPhase < 1.0 ? blinkPhase : 2.0 - blinkPhase)
      : 0.0;

    // ── 3. 绘制经典毒液上挑尖锐白眼 (Venom Predatory Eyes) ──
    function drawVenomEye(eyeCenterX, eyeCenterY, isLeft) {
      ctx.save();
      ctx.translate(eyeCenterX, eyeCenterY);
      if (!isLeft) ctx.scale(-1, 1);

      // 眼眶轮廓：斜挑犀利曲线
      ctx.beginPath();
      ctx.moveTo(-16, 6);
      ctx.bezierCurveTo(-10, -14, 8, -18, 20, -10);
      ctx.bezierCurveTo(12, 4, 0, 8, -16, 6);
      ctx.closePath();

      // 白色巩膜充盈水润流体
      const eyeGrad = ctx.createRadialGradient(2, -4, 2, 0, 0, 16);
      eyeGrad.addColorStop(0, '#ffffff');
      eyeGrad.addColorStop(0.7, '#edf4fc');
      eyeGrad.addColorStop(1, '#c0d6ef');
      ctx.fillStyle = eyeGrad;
      ctx.shadowColor = 'rgba(0, 240, 255, 0.7)';
      ctx.shadowBlur = 10;
      ctx.fill();
      ctx.shadowBlur = 0;

      // 狭长深邃瞳孔 (追踪光标)
      if (eyelidClose < 0.8) {
        ctx.beginPath();
        const px = pupilLookX * (isLeft ? 1 : -1);
        const py = pupilLookY;
        ctx.ellipse(2 + px, -3 + py, 3.2, 7.5, 0.15, 0, Math.PI * 2);
        ctx.fillStyle = '#060810';
        ctx.fill();

        // 瞳孔反光小白点
        ctx.beginPath();
        ctx.arc(3 + px, -6 + py, 1.4, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();
      }

      // 眼睑闭合遮罩
      if (eyelidClose > 0.05) {
        ctx.beginPath();
        ctx.rect(-20, -22, 44, 30 * eyelidClose);
        ctx.fillStyle = '#05070c';
        ctx.fill();
      }

      ctx.restore();
    }

    drawVenomEye(cx - 32, cy - 4, true);
    drawVenomEye(cx + 32, cy - 4, false);

    ctx.restore();
    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
})();

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

void WebConfigPortal::handleDemo() {
    String action = server.arg("action");
    if (action.length() == 0) {
        action = server.arg("plain");
    }
    Serial.printf(">>> [Portal API] Action requested: %s\n", action.c_str());
    blocking_screen = false; // 解除全屏阻断，恢复毒液屏幕实时演示
    if (on_demo_action) {
        on_demo_action(action);
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"" + action + "\"}");
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
    portal_canvas->setTextColor(0x07FF, 0x0842);
    portal_canvas->drawString("BtnB: Live Venom Screen | BtnA: Exit", 12, 118);

    portal_canvas->pushSprite(0, 0);
}

void WebConfigPortal::update() {
    if (!running) return;

    dns_server.processNextRequest();
    server.handleClient();

    // 仅在全屏配网覆盖模式下刷新屏幕，一旦进入测试演示模式则交由主渲染管线绘制毒液
    if (blocking_screen && millis() - screen_refresh_timer > 500) {
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
    blocking_screen = false;
    server.stop();
    dns_server.stop();
    WiFi.softAPdisconnect(true);
    Serial.println(">>> [Portal] WebConfigPortal stopped.");
}
