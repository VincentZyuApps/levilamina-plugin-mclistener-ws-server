// levilamina-plugin-mclistener-ws-server-exec-command-relay.js — LSE JS 插件
// 连回 C++ WS 服务端，处理 execute_command → mc.runcmdEx() → 返回 command_result
//
// 部署：将此文件放到 LeviLamina 服务器 plugins/ 目录
// 需要：LegacyScriptEngine (quickjs 或 nodejs 引擎)
// 需要：C++ 插件 config.json 中 execCommandMode 设为 "js-relay"

// ── 配置（需与 C++ 插件 config.json 中的 port / wsToken 保持一致）──
const RELAY_IP = "127.0.0.1"        // 连接的 WS服务端所在的ip
const RELAY_PORT  = 60605;          // 对应 C++ 插件的 port
const RELAY_TOKEN = "test12345";    // 对应 C++ 插件的 wsToken，空字符串表示无 token

const WS_URL = RELAY_TOKEN
    ? `ws://${RELAY_IP}:${RELAY_PORT}?token=${RELAY_TOKEN}`
    : `ws://${RELAY_IP}:${RELAY_PORT}`;

const RECONNECT_DELAY_MS = 5000;

let ws = null;

function connect() {
    ws = new WSClient();

    ws.listen('onTextReceived', (msg) => {
        let data;
        try { data = JSON.parse(msg); } catch { return; }

        if (data.type !== 'external_command_to_server' || !data.command) return;

        const result = mc.runcmdEx(data.command);
        ws.send(JSON.stringify({
            type:       'command_result',
            request_id: data.request_id || '',
            command:    data.command,
            ok:         result.success,
            output:     result.output,
        }));
    });

    ws.listen('onLostConnection', () => {
        log('[exec-relay] disconnected, reconnecting...');
        setTimeout(connect, RECONNECT_DELAY_MS);
    });

    ws.listen('onError', (err) => {
        log('[exec-relay] error: ' + err);
    });

    ws.connectAsync(WS_URL, (success) => {
        if (success) {
            log('[exec-relay] connected to ' + WS_URL);
            if (RELAY_TOKEN) {
                ws.send(JSON.stringify({ type: 'auth', token: RELAY_TOKEN }));
            }
        } else {
            log('[exec-relay] connect failed, retrying...');
            setTimeout(connect, RECONNECT_DELAY_MS);
        }
    });
}

connect();
