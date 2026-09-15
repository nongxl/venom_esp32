#include "WebConfigPortal.h"
#include <ArduinoJson.h>
#include "CreatureAI.h"
#include "PhysiologySystem.h"
#include "RelationshipSystem.h"
#include "ExpressionLayer.h"
#include "LLMClient.h"

static const char* getNeuralBehaviorChineseName(NeuralBehaviorType b) {
    switch (b) {
        case BEH_ESCAPE:   return "应激逃窜 (Escape)";
        case BEH_FREEZE:   return "警惕定身 (Freeze)";
        case BEH_OBSERVE:  return "好奇注视 (Observe)";
        case BEH_EXPLORE:  return "巡游漫步 (Explore)";
        case BEH_APPROACH: return "亲近依恋 (Approach)";
        case BEH_PROBE:    return "试探捕抓 (Probe)";
        case BEH_REST:     return "静养憩息 (Rest)";
        default:           return "从容待机 (Idle)";
    }
}

static const char* getEmotionChineseName(EmotionState e) {
    switch (e) {
        case EMOTION_CALM:       return "平静从容 (Calm)";
        case EMOTION_STRESS:     return "焦虑紧绷 (Stress)";
        case EMOTION_FEAR:       return "恐慌戒备 (Fear)";
        case EMOTION_ANGER:      return "暴怒反击 (Anger)";
        case EMOTION_CURIOSITY:  return "好奇探究 (Curious)";
        case EMOTION_EXHAUSTED:  return "体力耗尽 (Exhausted)";
        default:                 return "正常稳态 (Normal)";
    }
}

static const char* getExpressionChineseName(ExpressionType e) {
    switch (e) {
        case EXPR_OBSERVE:            return "贴屏注视 (Observe)";
        case EXPR_HESITATION:         return "试探犹豫 (Hesitation)";
        case EXPR_FEAR:               return "紧缩硬化 (Fear)";
        case EXPR_TRUST:              return "摊开信任 (Trust)";
        case EXPR_DISCOMFORT:         return "不满远离 (Discomfort)";
        case EXPR_CURIOSITY:          return "探头敲击 (Curiosity)";
        case EXPR_MIMICRY:            return "节拍模仿 (Mimicry)";
        case EXPR_WARNING:            return "警告扑面 (Warning)";
        case EXPR_GLASS_CONTACT:      return "隔屏接触 (Glass Contact)";
        case EXPR_SILENT_OBSERVATION: return "静默凝视 (Silent)";
        default:                      return "从容微动 (Neutral)";
    }
}

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
    server.on("/api/neural", HTTP_GET, [this]() { handleNeural(); });

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
/* 语言切换栏 */
.lang-bar { display: flex; justify-content: flex-end; gap: 8px; margin-bottom: 12px; }
.lang-btn {
  background: rgba(4, 11, 25, 0.75);
  border: 1px solid rgba(0, 240, 255, 0.25);
  color: var(--text-muted);
  padding: 5px 14px;
  border-radius: 99px;
  font-size: 12px;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}
.lang-btn:hover { border-color: var(--accent-cyan); color: #fff; }
.lang-btn.active {
  background: rgba(0, 240, 255, 0.22);
  border-color: var(--accent-cyan);
  color: var(--accent-cyan);
  box-shadow: 0 0 14px var(--accent-glow);
}

/* 神经看板专属样式 */
.neural-badge {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 3px 10px;
  border-radius: 8px;
  font-size: 12px;
  font-weight: 700;
  background: rgba(0, 240, 255, 0.12);
  color: var(--accent-cyan);
  border: 1px solid rgba(0, 240, 255, 0.25);
}
.neural-badge.secondary {
  background: rgba(255, 255, 255, 0.06);
  color: var(--text-muted);
  border-color: rgba(255, 255, 255, 0.15);
}
.mind-stream-box {
  background: linear-gradient(135deg, rgba(8, 22, 45, 0.9) 0%, rgba(3, 8, 20, 0.95) 100%);
  border: 1px solid rgba(0, 240, 255, 0.35);
  border-radius: 12px;
  padding: 12px 14px;
  margin-bottom: 14px;
  position: relative;
  box-shadow: inset 0 0 20px rgba(0, 240, 255, 0.1);
}
.mind-stream-title {
  font-size: 11px;
  font-weight: 800;
  color: var(--accent-cyan);
  letter-spacing: 1px;
  text-transform: uppercase;
  margin-bottom: 4px;
  display: flex;
  align-items: center;
  gap: 6px;
}
.mind-stream-text {
  font-size: 14px;
  font-weight: 600;
  color: #ffffff;
  line-height: 1.4;
  min-height: 20px;
}
.neural-status-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
  margin-bottom: 14px;
}
@media (max-width: 480px) { .neural-status-row { grid-template-columns: 1fr; } }
.neural-status-item {
  background: rgba(3, 8, 20, 0.7);
  border: 1px solid rgba(0, 240, 255, 0.14);
  padding: 10px 12px;
  border-radius: 10px;
}
.neural-status-label { font-size: 11px; color: var(--text-muted); margin-bottom: 3px; font-weight: 600; }
.neural-status-val { font-size: 13px; font-weight: 800; color: #fff; }

.drives-container { display: flex; flex-direction: column; gap: 8px; margin-bottom: 14px; }
.drive-item {
  background: rgba(3, 8, 20, 0.6);
  border: 1px solid rgba(0, 240, 255, 0.1);
  padding: 8px 12px;
  border-radius: 8px;
}
.drive-info { display: flex; justify-content: space-between; font-size: 12px; font-weight: 600; margin-bottom: 4px; }
.drive-name { color: #d0e4ff; }
.drive-val { color: var(--accent-cyan); font-weight: 700; }
.drive-bar-bg {
  width: 100%;
  height: 6px;
  background: rgba(255, 255, 255, 0.08);
  border-radius: 99px;
  overflow: hidden;
}
.drive-bar-fill {
  height: 100%;
  border-radius: 99px;
  background: linear-gradient(90deg, #1b6cd8, #00f0ff);
  transition: width 0.4s ease;
  width: 0%;
}
.drive-bar-fill.danger { background: linear-gradient(90deg, #d84315, #ff5722); }
.drive-bar-fill.calm { background: linear-gradient(90deg, #00897b, #26a69a); }

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
  <!-- 多语言无缝切换栏 -->
  <div class="lang-bar">
    <button type="button" class="lang-btn active" id="btn-lang-zh" onclick="switchLanguage('zh')">🇨🇳 中文</button>
    <button type="button" class="lang-btn" id="btn-lang-en" onclick="switchLanguage('en')">🇬🇧 English</button>
  </div>
  <div class="header">
    <div class="badge">LIFE FOUNDATION // SPECIMEN LF-978</div>
    <h1>共生体神经收容与拟态中枢</h1>
    <div class="subtitle">宿主终端已接入。玻璃后方为地外高危液态生物，操作时请保持理智。</div>
  </div>

  <!-- 🧠 神经心智状态实时监控看板 (Neural Mind Monitor) -->
  <div class="card" id="card-neural-monitor" style="border: 1px solid rgba(0, 240, 255, 0.38); box-shadow: 0 10px 32px rgba(0, 240, 255, 0.18);">
    <div class="card-title" style="color: var(--accent-cyan); justify-content: space-between;">
      <div style="display: flex; align-items: center; gap: 8px;">
        <svg viewBox="0 0 24 24" style="fill: var(--accent-cyan);"><path d="M12 3c-4.97 0-9 4.03-9 9 0 2.12.74 4.07 1.97 5.61L4.35 19.4c-.39.39-.39 1.02 0 1.41.39.39 1.02.39 1.41 0l1.9-1.9C9.28 19.63 10.59 20 12 20c4.97 0 9-4.03 9-9s-4.03-9-9-9zm0 15c-3.31 0-6-2.69-6-6s2.69-6 6-6 6 2.69 6 6-2.69 6-6 6z"/></svg>
        <span id="txt-neural-title">🧠 神经心智实时看板 (Neural Mind Monitor)</span>
      </div>
      <span class="neural-badge" id="lbl-live-indicator">● LIVE 1.5s</span>
    </div>

    <!-- 第一人称意识流 (Mind Stream / Subjective Consciousness) -->
    <div class="mind-stream-box">
      <div class="mind-stream-title">
        <span>⚡</span> <span id="txt-mind-stream-lbl">毒液实时意识心流 (Mind Stream)</span>
      </div>
      <div class="mind-stream-text" id="val-notes">静谧共鸣，液态身躯静静贴在玻璃内壁。</div>
    </div>

    <!-- 胜出行为与情绪状态 -->
    <div class="neural-status-row">
      <div class="neural-status-item">
        <div class="neural-status-label" id="txt-winner-lbl">当前胜出主导行为 (Winner Drive Behavior)</div>
        <div class="neural-status-val" id="val-behavior" style="color: var(--accent-cyan);">凝视外界观察者 (Observe Stare)</div>
      </div>
      <div class="neural-status-item">
        <div class="neural-status-label" id="txt-runner-lbl">竞争次高分行为 (Runner-up Competitor)</div>
        <div class="neural-status-val" id="val-runner-up" style="color: #cbd5e1;">悬浮悠然漫游 (Drift Explore)</div>
      </div>
    </div>

    <div class="neural-status-row" style="margin-bottom: 12px;">
      <div class="neural-status-item">
        <div class="neural-status-label" id="txt-emotion-lbl">生理情绪 & 微表情 (Emotion & Expression)</div>
        <div class="neural-status-val" id="val-emotion-expr">平静 (Calm) / 平静注视</div>
      </div>
      <div class="neural-status-item">
        <div class="neural-status-label" id="txt-env-lbl">环境感知与短期记忆 (Perception & Memory)</div>
        <div class="neural-status-val" id="val-event-env">事件: 暂无 | 麦克风: 35 dB</div>
      </div>
    </div>

    <!-- 8 大神经生理驱动进度仪表 -->
    <div style="font-size: 13px; font-weight: 700; color: #fff; margin-bottom: 8px; display: flex; justify-content: space-between;">
      <span id="txt-drives-title">八大神经生理驱动力 (8 Core Physiological Drives)</span>
      <span style="font-size: 11px; color: var(--text-muted);" id="txt-drives-sub">数值越高效力越强</span>
    </div>

    <div class="drives-container" id="drivesList">
      <!-- 动态填充 8 大驱动 -->
    </div>

    <!-- 习惯化抑制与社交关系稳态 -->
    <div style="background: rgba(4, 12, 28, 0.7); border: 1px solid rgba(0, 240, 255, 0.18); border-radius: 10px; padding: 10px 12px;">
      <div style="display: flex; justify-content: space-between; font-size: 12px; margin-bottom: 4px;">
        <span style="color: var(--text-muted);" id="txt-hab-lbl">感知习惯化阻尼因子 (Habituation Damping):</span>
        <span style="color: #00f0ff; font-weight: 700;" id="val-hab-factor">1.00x (敏锐)</span>
      </div>
      <div style="font-size: 11px; color: #728fae; line-height: 1.4;" id="txt-hab-desc">
        💡 敲击或震动多次后自动触发感知钝化阻尼，抑制惊恐与逃窜，防止生物行为机械重复。
      </div>
      <div style="display: flex; gap: 14px; margin-top: 6px; font-size: 12px;">
        <div><span style="color:var(--text-muted);" id="txt-trust-lbl">信任度:</span> <b id="val-trust" style="color:#4ade80;">0.50</b></div>
        <div><span style="color:var(--text-muted);" id="txt-open-lbl">社交开放度:</span> <b id="val-soc" style="color:#38bdf8;">0.60</b></div>
        <div><span style="color:var(--text-muted);" id="txt-resent-lbl">怨念累积:</span> <b id="val-resent" style="color:#f87171;">0.00</b></div>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
      ⚠️ 收容生物物理特性 (Threat Level: HIGH)
    </div>
    <div class="mind-perk">
      <div class="mind-icon">🧠</div>
      <div class="mind-text">
        <h4>自主掠食与代谢循环 (Predatory Cycle)</h4>
        <p>发呆、巡游、攀爬、蹦床、颠球、翻滚、蛛丝秋千与边缘暗中窥视。体力耗尽时会主动沉睡并打呼噜（Zz）。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">👁️</div>
      <div class="mind-text">
        <h4>边缘潜行与暗中伏击 (Stealth Stalking)</h4>
        <p>好奇心与警惕交织时，身体深潜隐蔽于边框外，仅露双眼和头尖，伺机探视或暴起突袭。</p>
      </div>
    </div>
    <div class="mind-perk">
      <div class="mind-icon">💬</div>
      <div class="mind-text">
        <h4>流体喷墨非语言交流 (Fluid Symbology)</h4>
        <p>受刺激或表达欲激增时，向玻璃喷射爱心、问号、惊叹号或齿痕符号，爬行经过时重新吞噬吸收。</p>
      </div>
    </div>
  </div>

  <!-- 共生体行为拟态演示与测试控制台 -->
  <div class="card" style="border: 1px solid rgba(0, 240, 255, 0.35); background: rgba(5, 13, 30, 0.9); box-shadow: 0 8px 30px rgba(0, 240, 255, 0.15);">
    <div class="card-title" style="color: var(--accent-cyan);">
      <svg viewBox="0 0 24 24" style="fill: var(--accent-cyan);"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
      🧪 共生体行为诱导与拟态实验室
    </div>
    <p style="font-size: 13px; color: var(--text-muted); margin-bottom: 14px; line-height: 1.5;">
      向收容仓施加神经脉冲诱导。系统将在 0.8 秒仿生蓄势后驱使共生体进入指定动作演进：
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
      <button type="button" class="demo-btn" onclick="triggerAction('sleep')">💤 深度休眠恢复</button>
      <button type="button" class="demo-btn" onclick="triggerAction('bat_hang')">🦇 蝙蝠倒挂小憩</button>
      <button type="button" class="demo-btn" onclick="triggerAction('irritate')">⚡ 神经激惹抽搐</button>
    </div>
    <div id="demoStatus" style="font-size: 12px; color: var(--accent-cyan); min-height: 20px; text-align: center; font-weight: 600;"></div>
  </div>

  <form id="configForm" onsubmit="saveConfig(event)">
    <div class="card">
      <div class="card-title">
        <svg viewBox="0 0 24 24"><path d="M12 4C7.31 4 3.07 5.9 0 8.98L12 21 24 8.98C20.93 5.9 16.69 4 12 4zm0 3.5c3.55 0 6.77 1.41 9.14 3.7L12 19.3 2.86 11.2C5.23 8.91 8.45 7.5 12 7.5z"/></svg>
        🌐 宿主局域网穿透 (Local Area WiFi)
      </div>
      <div class="form-group">
        <label for="ssid">附近 2.4GHz WiFi 频段</label>
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
        🧠 克林塔蜂巢意识链接 (Klyntar Hive Mind Uplink)
      </div>
      <label>一键选择高维意识神经元预设</label>
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
        <label for="url">意识枢纽 Base URL</label>
        <input type="text" id="url" name="url" value=")rawliteral" + url + R"rawliteral(" required>
      </div>
      <div class="form-group">
        <label for="model">思维模型代号 (Model Identifier)</label>
        <input type="text" id="model" name="model" value=")rawliteral" + model + R"rawliteral(" required>
      </div>
      <div class="form-group">
        <label for="key">神经通信密钥 (API Key)</label>
        <input type="password" id="key" name="key" value=")rawliteral" + key + R"rawliteral(" placeholder="sk-xxxxxxxxxxxxxxxx" required>
      </div>
    </div>

    <button type="submit" class="btn-save">💉 注入神经参数并激活收容仓 (Inject & Reboot)</button>
  </form>
</div>

<!-- 趴在屏幕边缘的活体共生体 (Living Edge Venom) -->
<div id="symbioteEdgeWidget" class="symbiote-edge" title="戳一戳毒液" onclick="pokeVenom(event)">
  <div class="symbiote-speech" id="symbioteSpeech">嘶... 正在观察你</div>
  <canvas id="venomCanvas" width="220" height="110"></canvas>
</div>

<div class="success-overlay" id="overlay">
  <div class="pulse-circle"></div>
  <h2 style="color:var(--accent-cyan); margin-bottom: 8px;">神经脉冲已同步至共生体</h2>
  <p style="color:var(--text-muted); font-size:13px;">收容仓正在重载生命维持协议并接入大模型蜂巢意识...<br>请退后 3 秒，观察玻璃屏幕变化。</p>
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

let currentLanguage = localStorage.getItem('venom_lang') || 'zh';

const I18N = {
  zh: {
    page_title: "共生体神经收容与拟态中枢",
    page_sub: "宿主终端已接入。玻璃后方为地外高危液态生物，操作时请保持理智。",
    neural_title: "🧠 神经心智实时看板 (Neural Mind Monitor)",
    mind_stream_lbl: "毒液实时意识心流 (Mind Stream)",
    winner_lbl: "当前胜出主导行为 (Winner Drive Behavior)",
    runner_lbl: "竞争次高分行为 (Runner-up Competitor)",
    emotion_lbl: "生理情绪 & 微表情 (Emotion & Expression)",
    env_lbl: "环境感知与短期记忆 (Perception & Memory)",
    drives_title: "八大神经生理驱动力 (8 Core Physiological Drives)",
    drives_sub: "数值越高效力越强",
    hab_lbl: "感知习惯化阻尼因子 (Habituation Damping):",
    hab_desc: "💡 敲击或震动多次后自动触发感知钝化阻尼，抑制惊恐与逃窜，防止生物行为机械重复。",
    trust_lbl: "信任度:",
    open_lbl: "社交开放度:",
    resent_lbl: "怨念累积:",
    specimen_title: "⚠️ 收容生物物理特性 (Threat Level: HIGH)",
    demo_title: "🧪 共生体行为诱导与拟态实验室",
    demo_sub: "向收容仓施加神经脉冲诱导。系统将在 0.8 秒仿生蓄势后驱使共生体进入指定动作演进：",
    wifi_title: "🌐 宿主局域网穿透 (Local Area WiFi)",
    wifi_lbl: "附近 2.4GHz WiFi 频段",
    wifi_scan: "--扫描网络--",
    pass_lbl: "WiFi 密码",
    llm_title: "🧠 克林塔蜂巢意识链接 (Klyntar Hive Mind Uplink)",
    llm_preset: "一键选择高维意识神经元预设",
    llm_url: "意识枢纽 Base URL",
    llm_model: "思维模型代号 (Model Identifier)",
    llm_key: "神经通信密钥 (API Key)",
    btn_save: "💉 注入神经参数并激活收容仓 (Inject & Reboot)"
  },
  en: {
    page_title: "Symbiote Neural Containment & Mimicry Hub",
    page_sub: "Host terminal linked. High-threat extraterrestrial fluid behind glass. Proceed with caution.",
    neural_title: "🧠 Real-Time Neural Mind Monitor",
    mind_stream_lbl: "Symbiote Mind Stream (Inner Voice)",
    winner_lbl: "Active Winner Behavior",
    runner_lbl: "Runner-up Competitor",
    emotion_lbl: "Physiology & Expression",
    env_lbl: "Perception & Short-Term Memory",
    drives_title: "8 Core Physiological Drives",
    drives_sub: "Higher values dominate behavioral competition",
    hab_lbl: "Habituation Damping Factor:",
    hab_desc: "💡 Repeated taps trigger sensory habituation to prevent panic escape loops.",
    trust_lbl: "Trust:",
    open_lbl: "Social Openness:",
    resent_lbl: "Resentment:",
    specimen_title: "⚠️ Specimen Biological Profile (Threat Level: HIGH)",
    demo_title: "🧪 Behavioral Induction & Mimicry Lab",
    demo_sub: "Send neural impulse triggers. Symbiote transitions naturally within 0.8s pre-action windup:",
    wifi_title: "🌐 Host Local Area Network (WiFi)",
    wifi_lbl: "Nearby 2.4GHz WiFi Bands",
    wifi_scan: "--Scan Networks--",
    pass_lbl: "WiFi Password",
    llm_title: "🧠 Klyntar Hive Mind Uplink",
    llm_preset: "Select High-Dimensional Neural Preset",
    llm_url: "Mind Hub Base URL",
    llm_model: "Model Identifier",
    llm_key: "API Key",
    btn_save: "💉 Inject Neural Parameters & Reboot"
  }
};

function switchLanguage(lang) {
  currentLanguage = lang;
  localStorage.setItem('venom_lang', lang);
  document.getElementById('btn-lang-zh').classList.toggle('active', lang === 'zh');
  document.getElementById('btn-lang-en').classList.toggle('active', lang === 'en');

  const dict = I18N[lang];
  if (!dict) return;

  const setT = (id, text) => { const el = document.getElementById(id); if (el) el.innerText = text; };
  setT('txt-neural-title', dict.neural_title);
  setT('txt-mind-stream-lbl', dict.mind_stream_lbl);
  setT('txt-winner-lbl', dict.winner_lbl);
  setT('txt-runner-lbl', dict.runner_lbl);
  setT('txt-emotion-lbl', dict.emotion_lbl);
  setT('txt-env-lbl', dict.env_lbl);
  setT('txt-drives-title', dict.drives_title);
  setT('txt-drives-sub', dict.drives_sub);
  setT('txt-hab-lbl', dict.hab_lbl);
  setT('txt-hab-desc', dict.hab_desc);
  setT('txt-trust-lbl', dict.trust_lbl);
  setT('txt-open-lbl', dict.open_lbl);
  setT('txt-resent-lbl', dict.resent_lbl);

  fetchNeuralStatus();
}

async function fetchNeuralStatus() {
  try {
    const res = await fetch('/api/neural');
    if (!res.ok) return;
    const data = await res.json();

    const isZh = (currentLanguage === 'zh');

    // 意识心流
    if (data.notes) {
      document.getElementById('val-notes').innerText = data.notes;
    }

    // 胜出主行为 & 次级行为
    document.getElementById('val-behavior').innerText = isZh ? (data.behavior_cn || data.behavior_en) : data.behavior_en;
    document.getElementById('val-runner-up').innerText = isZh ? (data.runner_up_cn || data.runner_up_en) : data.runner_up_en;

    // 情绪与表情
    const emo = isZh ? (data.emotion_cn || data.emotion_en) : data.emotion_en;
    const expr = isZh ? (data.expression_cn || data.expression_en) : data.expression_en;
    document.getElementById('val-emotion-expr').innerText = `${emo} / ${expr}`;

    // 环境与感知
    const evt = data.event || 'NONE';
    const mic = data.mic_db || 0;
    document.getElementById('val-event-env').innerText = isZh ? `最近感知: ${evt} | 环境音: ${mic} dB` : `Event: ${evt} | Mic: ${mic} dB`;

    // 习惯化阻尼
    const damp = (data.habituation_damping !== undefined) ? data.habituation_damping : 1.0;
    let dampDesc = '';
    if (damp >= 0.85) dampDesc = isZh ? ' (高敏锐)' : ' (Sensitive)';
    else if (damp >= 0.5) dampDesc = isZh ? ' (微钝化)' : ' (Habituated)';
    else dampDesc = isZh ? ' (深度钝化/冷静)' : ' (Deeply Habituated)';
    document.getElementById('val-hab-factor').innerText = `${damp.toFixed(2)}x${dampDesc}`;

    // 关系状态
    if (data.trust !== undefined) document.getElementById('val-trust').innerText = data.trust.toFixed(2);
    if (data.social_openness !== undefined) document.getElementById('val-soc').innerText = data.social_openness.toFixed(2);
    if (data.resentment !== undefined) document.getElementById('val-resent').innerText = data.resentment.toFixed(2);

    // 渲染 8 大驱动
    if (data.drives) {
      const container = document.getElementById('drivesList');
      let html = '';
      for (const [k, d] of Object.entries(data.drives)) {
        if (k === 'habituation') continue; // 习惯化在独立模块显示
        const label = isZh ? d.cn : d.en;
        const val = d.val || 0.0;
        const pct = Math.min(100, Math.max(0, Math.round(val * 100)));
        let barClass = 'drive-bar-fill';
        if (k === 'threat' || k === 'resentment') barClass += ' danger';
        else if (k === 'comfort' || k === 'energy') barClass += ' calm';

        html += `
          <div class="drive-item">
            <div class="drive-info">
              <span class="drive-name">${label}</span>
              <span class="drive-val">${val.toFixed(2)} (${pct}%)</span>
            </div>
            <div class="drive-bar-bg">
              <div class="${barClass}" style="width: ${pct}%;"></div>
            </div>
          </div>
        `;
      }
      container.innerHTML = html;
    }
  } catch(e) {}
}

// 页面加载启动 1.5s 轮询
setInterval(fetchNeuralStatus, 1500);
setTimeout(() => { switchLanguage(currentLanguage); }, 100);

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
    "嘶... 宿主，你看起来很可口",
    "我们在玻璃后面看着你...",
    "别乱戳！小心我们咬断你的触控笔！",
    "饥饿感正在蔓延... 虫子呢？",
    "5V 供电太抠门了，我们需要巧克力！",
    "你以为你是饲养员？你只是移动血包！",
    "克林塔蜂巢意识连接中... 嘶...",
    "再敲玻璃，我们出去第一个吃你！",
    "我们即是毒液 (WE ARE VENOM)！"
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

void WebConfigPortal::handleNeural() {
    StaticJsonDocument<2048> doc;
    if (src_ai) {
        const NeuralDrive &core = src_ai->getNeuralCore();
        const DriveState &d = core.getDrives();
        const BehaviorScores &sc = core.getScores();

        doc["ai_state"] = src_ai->getStateName();
        doc["behavior_en"] = core.getCurrentBehaviorName();
        doc["behavior_cn"] = getNeuralBehaviorChineseName(core.getCurrentBehavior());
        doc["runner_up_en"] = getNeuralBehaviorName(sc.runner_up);
        doc["runner_up_cn"] = getNeuralBehaviorChineseName(sc.runner_up);
        doc["event"] = core.getLatestEventName();
        doc["habituation_damping"] = (int)(core.getHabituationDampingFactor() * 100) / 100.0f;

        JsonObject drives = doc.createNestedObject("drives");
        auto setDrive = [&](const char *key, float val, const char *en, const char *cn) {
            JsonObject o = drives.createNestedObject(key);
            o["val"] = (int)(val * 100) / 100.0f;
            o["en"] = en;
            o["cn"] = cn;
        };

        setDrive("threat", d.threat, "Threat Avoidance", "威胁躲避 (T)");
        setDrive("curiosity", d.curiosity, "Curiosity Seeking", "好奇探索 (C)");
        setDrive("attachment", d.attachment, "Social Attachment", "社交依恋 (A)");
        setDrive("arousal", d.arousal, "Symbiote Arousal", "神经激活度 (R)");
        setDrive("energy", d.energy, "Cellular Energy", "细胞能量储备 (E)");
        setDrive("fatigue", d.fatigue, "Neuromuscular Fatigue", "神经疲劳度 (F)");
        setDrive("comfort", d.comfort, "Homeostatic Comfort", "体内稳态舒适 (M)");
        if (src_rel) {
            setDrive("resentment", src_rel->getResentment(), "Captive Resentment", "幽禁怨念累积 (RES)");
        }
        setDrive("habituation", d.habituation, "Sensory Habituation", "感知习惯化 (H)");
    }

    if (src_phys) {
        doc["emotion_en"] = src_phys->getEmotionName();
        doc["emotion_cn"] = getEmotionChineseName(src_phys->getEmotion());
        doc["mic_db"] = (int)src_phys->getMicDecibels();
    }

    if (src_rel) {
        doc["trust"] = (int)(src_rel->getTrust() * 100) / 100.0f;
        doc["resentment"] = (int)(src_rel->getResentment() * 100) / 100.0f;
        doc["social_openness"] = (int)(src_rel->getSocialOpenness() * 100) / 100.0f;
    }

    if (src_expr) {
        doc["expression_en"] = src_expr->getExpressionName();
        doc["expression_cn"] = getExpressionChineseName(src_expr->getCurrentExpression());
    }

    if (src_llm) {
        doc["notes"] = src_llm->getLatestNotes();
    }

    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json; charset=utf-8", resp);
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
