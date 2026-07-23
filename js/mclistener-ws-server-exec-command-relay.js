// mclistener-ws-server-exec-command-relay.js — LSE JS 插件
// 连回 C++ WS 服务端，处理 execute_command → mc.runcmdEx() → 返回 command_result
//
// 部署：将此文件放到 LeviLamina 服务器 plugins/ 目录
// 需要：LegacyScriptEngine (quickjs 或 nodejs 引擎)
// 需要：C++ 插件 config.json 中 execCommandMode 设为 "js-relay"

// ── 元数据（与 tooth.json 一致）───────────────────────
const PLUGIN_NAME    = "mclistener-ws-server";
const PLUGIN_VERSION = "0.6.5-alpha.29";
const PLUGIN_REPO    = "github.com/VincentZyuApps/levilamina-plugin-mclistener-ws-server";

// ── 配置（需与 C++ 插件 config.json 中的 port / wsToken 保持一致）──
const RELAY_IP    = "127.0.0.1";
const RELAY_PORT  = 60605;
const RELAY_TOKEN = "test12345";

const WS_URL = RELAY_TOKEN
    ? `ws://${RELAY_IP}:${RELAY_PORT}?token=${RELAY_TOKEN}`
    : `ws://${RELAY_IP}:${RELAY_PORT}`;

const RECONNECT_DELAY_MS = 5000;
const MAX_RECONNECT_COUNT = 0; // 0 = 无限制

// ── 安全 log ──────────────────────────────────────────
function safeLog(msg) {
    try { log(`[${PLUGIN_NAME}-relay] ${msg}`); } catch (e) { /* QuickJS 可能无全局 log */ }
}

let ws             = null;
let reconnectCount = 0;
let reconnectTimer = null;

function connect() {
    if (MAX_RECONNECT_COUNT > 0 && reconnectCount >= MAX_RECONNECT_COUNT) {
        safeLog(`已达最大重连次数 (${MAX_RECONNECT_COUNT})，停止重连。`);
        return;
    }

    ws = new WSClient();

    ws.listen('onTextReceived', (msg) => {
        let data;
        try { data = JSON.parse(msg); } catch { return; }

        if (data.type !== 'external_command_to_server' || !data.command) return;

        const result = mc.runcmdEx(data.command);
        safeLog(`exec '${data.command}' → ${result.success ? 'ok' : 'fail'} (${result.output.length} chars)`);

        ws.send(JSON.stringify({
            type:       'command_result',
            request_id: data.request_id || '',
            command:    data.command,
            ok:         result.success,
            result:     result.output,
        }));
    });

    ws.listen('onLostConnection', (code) => {
        safeLog(`disconnected (code=${code || '?'})`);
        scheduleReconnect();
    });

    ws.listen('onError', (err) => {
        safeLog(`error: ${err || 'unknown'}`);
        scheduleReconnect();
    });

    ws.connectAsync(WS_URL, (success) => {
        if (success) {
            reconnectCount = 0;
            safeLog(`connected to ${WS_URL}`);
            if (RELAY_TOKEN) {
                ws.send(JSON.stringify({ type: 'auth', token: RELAY_TOKEN }));
            }
        } else {
            reconnectCount++;
            safeLog(`connect failed (attempt ${reconnectCount}), retrying in ${RECONNECT_DELAY_MS}ms...`);
            scheduleReconnect();
        }
    });
}

function scheduleReconnect() {
    if (reconnectTimer) return;          // 防抖：已有重连任务排队
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connect();
    }, RECONNECT_DELAY_MS);
}

// ── 启动 ──────────────────────────────────────────────
mc.listen("onServerStarted", () => {
    safeLog(`v${PLUGIN_VERSION} starting, connect to ${WS_URL} ...`);
    connect();
});
