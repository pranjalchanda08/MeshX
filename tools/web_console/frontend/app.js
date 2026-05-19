const BACKEND_URL = `${window.location.protocol}//${window.location.host}`;
const WS_URL = `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}`;

let socket = null;
let currentPort = "";
let state = {
    nodes: {},
    gpio: {},
    logs: [],
    packets: []
};

let autoScroll = true;

// UI Elements
const portSelect = document.getElementById("port-select");
const connectBtn = document.getElementById("connect-btn");
const flashBtn = document.getElementById("flash-btn");
const eraseFlashChk = document.getElementById("erase-flash-chk");
const bspSelect = document.getElementById("bsp-select");
const profileSelect = document.getElementById("profile-select");
let configMetadata = {};
const flashMxcBtn = document.getElementById("flash-mxc-btn");
const logTerminal = document.getElementById("log-terminal");
const cliOutput = document.getElementById("cli-output");
const cliInput = document.getElementById("cli-input");
const cliSendBtn = document.getElementById("cli-send-btn");
const gpioGrid = document.getElementById("gpio-grid");
const topoViewport = document.getElementById("topo-viewport");
const packetsTimeline = document.getElementById("packets-timeline");
const autoscrollBtn = document.getElementById("autoscroll-btn");
const clearLogsBtn = document.getElementById("clear-logs-btn");
const exportLogsBtn = document.getElementById("export-logs-btn");
const clearPacketsBtn = document.getElementById("clear-packets-btn");
const mxcpShiftBtn = document.getElementById("mxcp-shift-btn");

// Filter checkboxes
const filterErr = document.getElementById("filter-err");
const filterWrn = document.getElementById("filter-wrn");
const filterDbg = document.getElementById("filter-dbg");

// 1. Initial Port Discovery
async function scanPorts() {
    try {
        const res = await fetch(`${BACKEND_URL}/api/ports`);
        const ports = await res.json();
        portSelect.innerHTML = '<option value="">-- Select Port --</option>';
        ports.forEach(p => {
            const opt = document.createElement("option");
            opt.value = p.port;
            opt.textContent = `${p.port} (${p.desc || "UART Device"})`;
            portSelect.appendChild(opt);
        });
    } catch (e) {
        console.error("Port scanning failed:", e);
        writeCliOutput("Error scanning available serial ports. Ensure backend is running.\n");
    }
}

// 2. WebSocket Connection and Stream Management
function connectPort() {
    const port = portSelect.value;
    if (!port) {
        alert("Please select a valid serial port first.");
        return;
    }
    
    if (socket) {
        socket.close();
    }
    
    currentPort = port;
    connectBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Connecting...';
    connectBtn.disabled = true;
    
    socket = new WebSocket(`${WS_URL}/ws/events?port=${encodeURIComponent(port)}`);
    
    socket.onopen = () => {
        connectBtn.innerHTML = '<i class="fa-solid fa-unlink"></i> Disconnect';
        connectBtn.className = "btn btn-secondary";
        connectBtn.disabled = false;
        if (mxcpShiftBtn) mxcpShiftBtn.style.display = "inline-flex";
        writeCliOutput(`\nConnected to ${port} successfully. Switch to Hosted mode activated.\n`);
        
        // Sync dynamic ELF decoder and profile on server
        const bsp = bspSelect.value;
        const product = profileSelect.value;
        if (bsp && product) {
            fetch(`${BACKEND_URL}/api/port/set-bsp-profile?port=${encodeURIComponent(port)}&bsp=${bsp}&product=${product}`, {
                method: "POST"
            }).catch(err => console.error("Error setting port profile:", err));
        }
        
        // Query initial GPIO configurations automatically (pins 4, 5, 6 for Xiao C3 profile)
        sendGpioCommand(5, 8, 0); // Pin 5 config
        sendGpioCommand(4, 8, 0); // Pin 4 config
        sendGpioCommand(6, 8, 0); // Pin 6 config
        
        // Reset and populate local nodes
        state.nodes = {};
        
        // Render panels immediately upon connection
        renderGpioPipeline();
        renderTopography();
        renderMxcpCommands();
    };
    
    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        handleIncomingEvent(msg);
    };
    
    socket.onclose = () => {
        connectBtn.innerHTML = '<i class="fa-solid fa-link"></i> Connect';
        connectBtn.className = "btn btn-primary";
        connectBtn.disabled = false;
        if (mxcpShiftBtn) mxcpShiftBtn.style.display = "none";
        socket = null;
        writeCliOutput(`\nDisconnected from port ${port}.\n`);
        
        // Reset state and re-render empty panels
        state.nodes = {};
        state.gpio = {};
        renderTopography();
        renderMxcpCommands();
        renderGpioPipeline();
        
        // Restore flash buttons if active
        flashBtn.innerHTML = '<i class="fa-solid fa-bolt"></i> Flash Firmware';
        flashBtn.disabled = !(bspSelect.value && profileSelect.value);
        if (flashMxcBtn) {
            flashMxcBtn.innerHTML = '<i class="fa-solid fa-arrow-up-from-bracket"></i> Flash Config';
            flashMxcBtn.disabled = !(bspSelect.value && profileSelect.value);
        }
    };
    
    socket.onerror = (e) => {
        console.error("WebSocket error:", e);
        writeCliOutput(`\nSerial port gateway connection error.\n`);
    };
}

// 3. Command Senders
function sendGpioCommand(pin, cmd, value) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "gpio",
            pin: pin,
            cmd: cmd,
            value: value
        }));
    }
}

function sendCliCommand(cmd) {
    if (!cmd.trim()) return;
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "cli",
            command: cmd
        }));
        writeCliOutput(`MeshX> ${cmd}\n`);
    } else {
        writeCliOutput(`Not connected. Cannot send: ${cmd}\n`);
    }
    cliInput.value = "";
}

// 4. Handle incoming stream events
function handleIncomingEvent(evt) {
    if (evt.type === "hydration") {
        state.gpio = evt.state.gpio || {};
        state.logs = evt.state.logs || [];
        state.nodes = evt.state.nodes || {};
        renderGpioPipeline();
        renderLogs();
        detectNodesFromState();
        renderMxcpCommands();
    } else if (evt.type === "log") {
        state.logs.push(evt);
        if (state.logs.length > 200) state.logs.shift();
        appendLogLine(evt);
    } else if (evt.type === "text") {
        writeCliOutput(evt.data);
        // Self-restoring flashing UI button logic
        if (evt.data.includes("SUCCESS: Custom configuration flashed successfully!") || 
            evt.data.includes("SUCCESS: Flashing completed successfully") || 
            evt.data.includes("ERROR: Command failed") || 
            evt.data.includes("ERROR: Flashing process failed") ||
            evt.data.includes("ERROR: Configuration flashing failed") ||
            evt.data.includes("ERROR: Flashing failed")) {
            flashBtn.innerHTML = '<i class="fa-solid fa-bolt"></i> Flash Firmware';
            flashBtn.disabled = false;
            
            if (flashMxcBtn) {
                flashMxcBtn.innerHTML = '<i class="fa-solid fa-arrow-up-from-bracket"></i> Flash Config';
                flashMxcBtn.disabled = !(bspSelect.value && profileSelect.value);
            }
        }
    } else if (evt.type === "mxsp") {
        state.packets.push(evt);
        if (state.packets.length > 30) state.packets.shift();
        renderPackets();
        detectNodeFromMxsp(evt);
    } else if (evt.type === "gpio_update") {
        state.gpio[evt.pin] = evt;
        renderGpioPipeline();
    } else if (evt.type === "nodes_discovered") {
        state.nodes = evt.nodes;
        renderTopography();
        renderMxcpCommands();
    } else if (evt.type === "node_state_update") {
        if (state.nodes[evt.address]) {
            state.nodes[evt.address].value = evt.value;
            
            // Format a nice status reply string!
            let replyStr = `STATUS: ${evt.value > 0 ? "ON" : "OFF"}`;
            if (evt.element_type === 2) { // CWWW
                if (evt.func_id === 1 && evt.data_hex) {
                    replyStr = `Lightness/Temp: ${evt.data_hex}`;
                }
            } else if (evt.element_type === 4) { // RGB
                if (evt.func_id === 1 && evt.data_hex) {
                    replyStr = `HSL: ${evt.data_hex}`;
                }
            } else if (evt.element_type === 6) { // Sensor
                replyStr = `Sensor: ${evt.value} units`;
            }
            
            state.nodes[evt.address].lastReply = replyStr + ` (${new Date().toLocaleTimeString()})`;
            
            renderTopography();
            renderMxcpCommands();
        }
    }
}

// Write to raw CLI terminal log
function writeCliOutput(text) {
    cliOutput.innerHTML += text.replace(/\n/g, "<br>");
    cliOutput.scrollTop = cliOutput.scrollHeight;
}

// Append live log
function appendLogLine(log) {
    const isErr = log.message.includes("[E]") || log.level === "ERR";
    const isWrn = log.message.includes("[W]") || log.level === "WRN";
    const isDbg = log.message.includes("[D]") || log.level === "DBG";
    
    // Severity Filter logic
    if (isErr && !filterErr.checked) return;
    if (isWrn && !filterWrn.checked) return;
    if (isDbg && !filterDbg.checked) return;
    
    const div = document.createElement("div");
    div.className = "log-line";
    
    // Apply server style tags colorization
    if (isErr) {
        div.style.color = "var(--neon-crimson)";
    } else if (isWrn) {
        div.style.color = "var(--neon-amber)";
    } else if (isDbg) {
        div.style.color = "var(--neon-teal)";
    }
    
    div.textContent = log.message;
    logTerminal.appendChild(div);
    
    if (autoScroll) {
        logTerminal.scrollTop = logTerminal.scrollHeight;
    }
}

// Render entire logs list
function renderLogs() {
    logTerminal.innerHTML = "";
    state.logs.forEach(log => appendLogLine(log));
}

// Hex string helper to Uint8Array
function hexToBytes(hex) {
    if (!hex) return new Uint8Array(0);
    const bytes = new Uint8Array(hex.length / 2);
    for (let i = 0; i < hex.length; i += 2) {
        bytes[i / 2] = parseInt(hex.substr(i, 2), 16);
    }
    return bytes;
}

// Complete MXSP Packet Decoder
function decodeMxspPacket(msg_type, payloadHex) {
    try {
        const bytes = hexToBytes(payloadHex);
        if (bytes.length === 0) return null;
        
        let html = '<div class="pkt-decoded">';
        
        if (msg_type === 0xB1 || msg_type === 0xC2) { // SYS_EVT_NOTIFY / SYS_CMD_SEND
            const evt_id = bytes[0] | (bytes[1] << 8);
            const eventNames = {
                1: "NODE_RESET",
                2: "PROV_COMPLETED",
                3: "PROV_FAILED",
                4: "PROV_STARTED",
                5: "IDENTIFY_STARTED",
                6: "IDENTIFY_STOPPED",
                7: "GET_COMPOSITION",
                8: "SET_CONSOLE_ROUTING"
            };
            const evtName = eventNames[evt_id] || `UNKNOWN_SYS_EVT (${evt_id})`;
            const title = msg_type === 0xB1 ? "System Control Notification" : "System Control Command";
            html += `<div class="pkt-decoded-header">${title}</div>`;
            html += `<div class="pkt-decoded-item">Event / Action: <strong>${evtName}</strong></div>`;
            
            if (evt_id === 0x07 && msg_type === 0xB1 && bytes.length >= 5) {
                const num_elements = bytes[4];
                html += `<div class="pkt-decoded-item">Elements Discovered: <strong>${num_elements}</strong></div>`;
                html += `<table class="pkt-composition-table">`;
                html += `<thead><tr><th>Idx</th><th>Variant</th><th>Type</th><th>Name</th></tr></thead><tbody>`;
                let offset = 5;
                for (let i = 0; i < num_elements; i++) {
                    if (offset + 6 > bytes.length) break;
                    const idx = bytes[offset] | (bytes[offset + 1] << 8);
                    const variant = bytes[offset + 2] | (bytes[offset + 3] << 8);
                    const type = bytes[offset + 4] | (bytes[offset + 5] << 8);
                    offset += 6;
                    let name = "";
                    while (offset < bytes.length && bytes[offset] !== 0) {
                        name += String.fromCharCode(bytes[offset]);
                        offset++;
                    }
                    offset++; // skip null
                    const variants = {
                        0: "Relay Server",
                        1: "Relay Client",
                        2: "CWWW Server",
                        3: "CWWW Client",
                        4: "RGB Server",
                        5: "RGB Client",
                        6: "Sensor Server",
                        7: "Sensor Client"
                    };
                    const variantStr = variants[variant] || `Unknown (${variant})`;
                    const typeStr = type === 0 ? "Server" : "Client";
                    html += `<tr><td>${idx}</td><td>${variantStr}</td><td>${typeStr}</td><td>${name}</td></tr>`;
                }
                html += `</tbody></table>`;
            } else if (evt_id === 0x02 && bytes.length >= 24) { // PROV_COMPLETED
                const net_idx = bytes[4] | (bytes[5] << 8);
                const addr = bytes[6] | (bytes[7] << 8);
                const uuidBytes = bytes.slice(8, 24);
                const uuidHex = Array.from(uuidBytes).map(b => b.toString(16).padStart(2, '0')).join('');
                const uuidFormatted = `${uuidHex.substr(0,8)}-${uuidHex.substr(8,4)}-${uuidHex.substr(12,4)}-${uuidHex.substr(16,4)}-${uuidHex.substr(20)}`;
                html += `<div class="pkt-decoded-item">Assigned Address: <strong>0x${addr.toString(16).toUpperCase().padStart(4, '0')}</strong></div>`;
                html += `<div class="pkt-decoded-item">NetIdx: <strong>${net_idx}</strong></div>`;
                html += `<div class="pkt-decoded-item">UUID: <code style="font-size:0.7rem;">${uuidFormatted}</code></div>`;
            } else if (evt_id === 0x03 && bytes.length >= 5) { // PROV_FAILED
                const reason = bytes[4];
                html += `<div class="pkt-decoded-item" style="color: var(--neon-crimson);">Failed Reason: <strong>0x${reason.toString(16).toUpperCase().padStart(2, '0')}</strong></div>`;
            } else if (evt_id === 0x08 && bytes.length >= 5) { // SET_CONSOLE_ROUTING
                const enable = bytes[4] !== 0;
                html += `<div class="pkt-decoded-item">Console Routing: <strong>${enable ? "ENABLED" : "DISABLED"}</strong></div>`;
            }
        } 
        else if (msg_type === 0xB2 || msg_type === 0xC1) { // DATA_EVT_NOTIFY / EL_CMD_SEND
            const el_id = bytes[0] | (bytes[1] << 8);
            const el_type = bytes[2] | (bytes[3] << 8);
            const func_id = bytes[4] | (bytes[5] << 8);
            const msg_len = bytes[6] | (bytes[7] << 8);
            
            const elementTypes = {
                0: "RELAY_SERVER",
                1: "RELAY_CLIENT",
                2: "LIGHT_CWWW_SERVER",
                3: "LIGHT_CWWW_CLIENT",
                4: "LIGHT_HSL_SERVER",
                5: "LIGHT_HSL_CLIENT",
                6: "SENSOR_SERVER",
                7: "SENSOR_CLIENT"
            };
            const elTypeName = elementTypes[el_type] || `Unknown Type (${el_type})`;
            const title = msg_type === 0xB2 ? "Telemetry Data Event" : "Element Control Command";
            
            html += `<div class="pkt-decoded-header">${title}</div>`;
            html += `<div class="pkt-decoded-item">Element #${el_id} (<strong>${elTypeName}</strong>) | Func ID: <strong>0x${func_id.toString(16).toUpperCase().padStart(2, '0')}</strong></div>`;
            
            if (bytes.length >= 8 + msg_len) {
                const dataBytes = bytes.slice(8, 8 + msg_len);
                // Decode specific telemetry states based on element type
                if (el_type === 0) { // Relay Server
                    if (func_id === 0x00 && dataBytes.length >= 1) {
                        const stateStr = dataBytes[0] === 1 ? "ON" : "OFF";
                        html += `<div class="pkt-decoded-item">Telemetry: State is <strong style="color: ${dataBytes[0] === 1 ? 'var(--neon-emerald)' : 'var(--text-secondary)'};">${stateStr}</strong></div>`;
                    }
                } else if (el_type === 1) { // Relay Client
                    if (func_id === 0x00 && dataBytes.length >= 2) {
                        const err = dataBytes[0];
                        const stateStr = dataBytes[1] === 1 ? "ON" : "OFF";
                        html += `<div class="pkt-decoded-item">Telemetry: State is <strong>${stateStr}</strong> (Error: ${err})</div>`;
                    }
                } else if (el_type === 2) { // CWWW Server
                    if (func_id === 0x00 && dataBytes.length >= 1) {
                        const stateStr = dataBytes[0] === 1 ? "ON" : "OFF";
                        html += `<div class="pkt-decoded-item">Telemetry: State is <strong>${stateStr}</strong></div>`;
                    } else if (func_id === 0x01 && dataBytes.length >= 6) {
                        const lightness = dataBytes[0] | (dataBytes[1] << 8);
                        const temperature = dataBytes[2] | (dataBytes[3] << 8);
                        const delta_uv = dataBytes[4] | (dataBytes[5] << 8);
                        html += `<div class="pkt-decoded-item">Lightness: <strong>${lightness}</strong> | Temp: <strong>${temperature} K</strong> | Delta UV: <strong>${delta_uv}</strong></div>`;
                    }
                } else if (el_type === 3) { // CWWW Client
                    if (func_id === 0x00 && dataBytes.length >= 2) {
                        const err = dataBytes[0];
                        const stateStr = dataBytes[1] === 1 ? "ON" : "OFF";
                        html += `<div class="pkt-decoded-item">Telemetry: State is <strong>${stateStr}</strong> (Error: ${err})</div>`;
                    } else if (func_id === 0x01 && dataBytes.length >= 7) {
                        const err = dataBytes[0];
                        const lightness = dataBytes[1] | (dataBytes[2] << 8);
                        const temperature = dataBytes[3] | (dataBytes[4] << 8);
                        const delta_uv = dataBytes[5] | (dataBytes[6] << 8);
                        html += `<div class="pkt-decoded-item">Lightness: <strong>${lightness}</strong> | Temp: <strong>${temperature} K</strong> (Error: ${err})</div>`;
                    }
                } else if (el_type === 4) { // RGB Server
                    if (func_id === 0x00 && dataBytes.length >= 1) {
                        const stateStr = dataBytes[0] === 1 ? "ON" : "OFF";
                        html += `<div class="pkt-decoded-item">Telemetry: State is <strong>${stateStr}</strong></div>`;
                    } else if (func_id === 0x01 && dataBytes.length >= 6) {
                        const h = dataBytes[0] | (dataBytes[1] << 8);
                        const s = dataBytes[2] | (dataBytes[3] << 8);
                        const l = dataBytes[4] | (dataBytes[5] << 8);
                        html += `<div class="pkt-decoded-item">HSL: <strong>H:${h} S:${s}% L:${l}%</strong></div>`;
                    }
                } else if (el_type === 6) { // Sensor Server
                    if (func_id === 0x00 && dataBytes.length >= 2) {
                        const val = dataBytes[0] | (dataBytes[1] << 8);
                        html += `<div class="pkt-decoded-item">Sensor Value: <strong>${val} units</strong></div>`;
                    }
                }
            }
        }
        else if (msg_type === 0xD1) { // GPIO_CMD
            const cmd = bytes[0];
            const logical_pin = bytes[1];
            const reserved = bytes[2];
            const payload_len = bytes[3];
            
            const cmdNames = {
                1: "SET_LEVEL",
                2: "GET_LEVEL",
                3: "TOGGLE",
                4: "SET_PWM_DUTY",
                5: "SET_PWM_FREQ",
                6: "INTR_ENABLE",
                7: "INTR_DISABLE",
                8: "GET_CONFIG",
                9: "GET_STATE"
            };
            const cmdName = cmdNames[cmd] || `UNKNOWN_CMD (${cmd})`;
            html += `<div class="pkt-decoded-header">GPIO Control Command</div>`;
            html += `<div class="pkt-decoded-item">Logical Pin: <strong>${logical_pin}</strong> | Action: <strong>${cmdName}</strong></div>`;
            if (payload_len > 0 && bytes.length >= 5) {
                const val = bytes[4];
                if (cmd === 1) {
                    html += `<div class="pkt-decoded-item">Target Level: <strong style="color: ${val === 1 ? 'var(--neon-emerald)' : 'var(--text-secondary)'};">${val === 1 ? "HIGH (1)" : "LOW (0)"}</strong></div>`;
                } else if (cmd === 4) {
                    html += `<div class="pkt-decoded-item">PWM Duty Cycle: <strong>${val}%</strong></div>`;
                } else {
                    html += `<div class="pkt-decoded-item">Value: <strong>${val}</strong></div>`;
                }
            }
        }
        else if (msg_type === 0xD2) { // GPIO_RSP
            const cmd = bytes[0];
            const logical_pin = bytes[1];
            const status = bytes[2];
            const response_len = bytes[3];
            
            const cmdNames = {
                1: "SET_LEVEL",
                2: "GET_LEVEL",
                3: "TOGGLE",
                4: "SET_PWM_DUTY",
                5: "SET_PWM_FREQ",
                6: "INTR_ENABLE",
                7: "INTR_DISABLE",
                8: "GET_CONFIG",
                9: "GET_STATE"
            };
            const cmdName = cmdNames[cmd] || `UNKNOWN_CMD (${cmd})`;
            const statusStr = status === 0 ? "SUCCESS" : `ERROR (${status})`;
            html += `<div class="pkt-decoded-header">GPIO Response</div>`;
            html += `<div class="pkt-decoded-item">Pin: <strong>${logical_pin}</strong> | Action: <strong>${cmdName}</strong></div>`;
            html += `<div class="pkt-decoded-item">Status: <strong style="color: ${status === 0 ? 'var(--neon-emerald)' : 'var(--neon-crimson)'};">${statusStr}</strong></div>`;
            
            if (response_len > 0 && bytes.length >= 5) {
                const respVal = bytes[4];
                if (cmd === 8 && bytes.length >= 6) { // GET_CONFIG
                    const mode = bytes[4];
                    const pull = bytes[5];
                    const modeStr = mode === 1 ? "INPUT" : mode === 2 ? "OUTPUT" : "PWM";
                    html += `<div class="pkt-decoded-item">Config: Mode=<strong>${modeStr}</strong>, Pull=<strong>${pull}</strong></div>`;
                } else {
                    html += `<div class="pkt-decoded-item">Response Data: <strong>${respVal}</strong></div>`;
                }
            }
        }
        else if (msg_type === 0xD3) { // GPIO_EVT
            const event_type = bytes[0];
            const logical_pin = bytes[1];
            const value = bytes[2];
            const timestamp = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
            
            const eventNames = {
                1: "LEVEL_CHANGE",
                2: "INTERRUPT",
                3: "MODE_CHANGE",
                255: "ERROR"
            };
            const eventName = eventNames[event_type] || `UNKNOWN (${event_type})`;
            html += `<div class="pkt-decoded-header">GPIO Async Event</div>`;
            html += `<div class="pkt-decoded-item">Pin: <strong>${logical_pin}</strong> | Event: <strong>${eventName}</strong></div>`;
            html += `<div class="pkt-decoded-item">Level Value: <strong style="color: ${value === 1 ? 'var(--neon-emerald)' : 'var(--text-secondary)'};">${value === 1 ? "HIGH (1)" : "LOW (0)"}</strong></div>`;
            html += `<div class="pkt-decoded-item">Timestamp: <strong>${timestamp} ms</strong></div>`;
        }
        else if (msg_type === 0x03) { // HOSTED_MODE
            const enable = bytes[0] !== 0;
            html += `<div class="pkt-decoded-header">Hosted Mode Switch</div>`;
            html += `<div class="pkt-decoded-item">State: <strong style="color: var(--neon-purple);">${enable ? "ENABLED" : "DISABLED"}</strong></div>`;
        }
        else {
            html += `<div class="pkt-decoded-header">OPCODE 0x${msg_type.toString(16).toUpperCase()}</div>`;
            html += `<div class="pkt-decoded-item">Binary payload decodes to custom vendors frame.</div>`;
        }
        
        html += '</div>';
        return html;
    } catch (e) {
        console.error("Decoding error: ", e);
        return null;
    }
}

// Render Packets timeline
function renderPackets() {
    if (state.packets.length === 0) {
        packetsTimeline.innerHTML = `
            <div class="empty-state">
                <i class="fa-solid fa-receipt"></i>
                <p>Listening for active binary MXSP stream packets...</p>
            </div>`;
        return;
    }
    
    packetsTimeline.innerHTML = "";
    state.packets.forEach(p => {
        const div = document.createElement("div");
        const isIncoming = (p.msg_type === 0xB1 || p.msg_type === 0xB2 || p.msg_type === 0xD2 || p.msg_type === 0xD3);
        div.className = `packet-item ${isIncoming ? "incoming" : "outgoing"}`;
        
        let typeStr = `OPCODE 0x${p.msg_type.toString(16).toUpperCase()}`;
        if (p.msg_type === 0xB1) typeStr = "SYS_EVT_NOTIFY (0xB1)";
        else if (p.msg_type === 0xB2) typeStr = "DATA_EVT_NOTIFY (0xB2)";
        else if (p.msg_type === 0xC1) typeStr = "EL_CMD_SEND (0xC1)";
        else if (p.msg_type === 0xC2) typeStr = "SYS_CMD_SEND (0xC2)";
        else if (p.msg_type === 0xD1) typeStr = "GPIO_CMD (0xD1)";
        else if (p.msg_type === 0xD2) typeStr = "GPIO_RSP (0xD2)";
        else if (p.msg_type === 0xD3) typeStr = "GPIO_EVT (0xD3)";
        else if (p.msg_type === 0x03) typeStr = "HOSTED_MODE (0x03)";
        
        const directionLabel = isIncoming 
            ? `<span style="color: var(--neon-emerald); font-size: 0.7rem; font-weight: 800; text-transform: uppercase;"><i class="fa-solid fa-cloud-arrow-down"></i> Incoming Telemetry</span>`
            : `<span style="color: var(--neon-purple); font-size: 0.7rem; font-weight: 800; text-transform: uppercase;"><i class="fa-solid fa-paper-plane"></i> Outgoing Command</span>`;
        
        // Pretty print raw hex payload spaces
        const hex = p.payload.toUpperCase().match(/.{1,2}/g)?.join(" ") || p.payload;
        
        // Decode telemetry to human understandable form
        const decodedHtml = decodeMxspPacket(p.msg_type, p.payload) || "";
        
        div.innerHTML = `
            <div class="pkt-meta" style="margin-bottom: 2px;">
                ${directionLabel}
                <span class="pkt-time">${new Date().toLocaleTimeString()}</span>
            </div>
            <div class="pkt-meta">
                <span class="pkt-type">${typeStr}</span>
                <span style="font-size:0.72rem; color:var(--text-secondary);">Length: ${p.payload.length / 2} bytes</span>
            </div>
            <div class="pkt-bytes">${hex}</div>
            ${decodedHtml}
        `;
        packetsTimeline.appendChild(div);
    });
    packetsTimeline.scrollTop = packetsTimeline.scrollHeight;
}

// Render GPIO statuses
function renderGpioPipeline() {
    if (!gpioGrid) return;
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        gpioGrid.innerHTML = `
            <div class="empty-state">
                <i class="fa-solid fa-microchip"></i>
                <p>Serial port offline.<br>Connect to start monitoring GPIOs.</p>
            </div>`;
        return;
    }
    
    gpioGrid.innerHTML = "";
    
    // Render actual active board pins from Xiao ESP32-C3 product profile (BUTTON_1=5, RELAY_1=4, LED_PWM=6)
    const pins = [
        { pin: 5, label: "BUTTON_1 (Input Switch)", tag: "INPUT", class: "tag-input" },
        { pin: 4, label: "RELAY_1 (Output Relay)", tag: "OUTPUT", class: "tag-output", toggleable: true },
        { pin: 6, label: "LED_PWM (Dimmer)", tag: "PWM", class: "tag-pwm", pwm: true }
    ];
    
    pins.forEach(p => {
        const stateData = state.gpio[p.pin] || { level: 0 };
        const level = stateData.level || 0;
        
        const div = document.createElement("div");
        div.className = `gpio-item ${p.tag.toLowerCase()}-pin`;
        
        let lightClass = level > 0 ? "light-on animate-pulse-glow" : "light-off";
        let levelControls = "";
        
        if (p.toggleable) {
            levelControls = `
                <div class="gpio-control-wrap">
                    <button class="pin-toggle-btn" onclick="triggerPinToggle(${p.pin}, ${level})">
                        <i class="fa-solid fa-power-off"></i>
                    </button>
                </div>`;
        } else if (p.pwm) {
            const dutyPercent = Math.round((level / 255) * 100) || 0;
            levelControls = `
                <div class="gpio-control-wrap pwm-wrap">
                    <div class="pwm-meter-container">
                        <div class="pwm-bar-fill" style="width: ${dutyPercent}%;"></div>
                        <span class="pwm-percent">${dutyPercent}%</span>
                    </div>
                    <input type="range" class="slider-control" min="0" max="255" value="${level}" 
                        oninput="triggerPwmDuty(${p.pin}, this.value)">
                </div>`;
        } else {
            // Standard input
            levelControls = `
                <div class="gpio-control-wrap">
                    <span class="status-indicator-light ${lightClass}"></span>
                </div>`;
        }
        
        div.innerHTML = `
            <span class="pin-num">Pin ${p.pin}</span>
            <span class="pin-tag ${p.class}">${p.tag}</span>
            <span class="pin-label">${p.label}</span>
            ${levelControls}
        `;
        gpioGrid.appendChild(div);
    });
}

window.triggerPinToggle = (pin, currentLevel) => {
    const nextLevel = currentLevel > 0 ? 0 : 1;
    // Set active pin level via MXSP command D1 (cmd=1, pin, val)
    sendGpioCommand(pin, 1, nextLevel);
};

window.triggerPwmDuty = (pin, val) => {
    // Set PWM Duty cycle via MXSP command D1 (cmd=4, pin, val)
    sendGpioCommand(pin, 4, parseInt(val));
};

// 5. Dynamic Nodes Discovery Topography Rendering
function detectNodesFromState() {
    // Composition updates state.nodes dynamically. Keep online status visual only.
    renderTopography();
}

function detectNodeFromMxsp(pkt) {
    // B1/B2 notifications are parsed via custom dynamic event handlers
}

function addMeshNode(address, name, type, value) {
    state.nodes[address] = { name, type, value };
    renderTopography();
}

function renderTopography() {
    const nodeKeys = Object.keys(state.nodes).filter(addr => {
        const node = state.nodes[addr];
        return node.element_idx !== 0 && addr !== "0x0000";
    });
    
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        topoViewport.innerHTML = `
            <div class="empty-state">
                <i class="fa-solid fa-satellite-dish"></i>
                <p>Serial port offline.<br>Connect to start BLE Mesh auto-discovery.</p>
            </div>`;
        return;
    }
    
    if (nodeKeys.length === 0) {
        topoViewport.innerHTML = `
            <div class="empty-state">
                <i class="fa-solid fa-satellite-dish"></i>
                <p>No active BLE Mesh elements composition retrieved.<br>Retrying or waiting for composition scan...</p>
            </div>`;
        return;
    }
    
    topoViewport.innerHTML = "";
    nodeKeys.forEach(addr => {
        const node = state.nodes[addr];
        const card = document.createElement("div");
        card.className = "mesh-node";
        
        let controlWidget = "";
        let badgeColor = "var(--neon-emerald)";
        let badgeBg = "rgba(0, 230, 118, 0.15)";
        
        if (node.type.includes("Relay")) {
            badgeColor = "var(--neon-teal)";
            badgeBg = "rgba(0, 229, 255, 0.15)";
            controlWidget = `
                <div class="control-group">
                    <label><i class="fa-solid fa-power-off"></i> Element Command:</label>
                    <div style="display:flex; gap:10px; margin-top:5px;">
                        <button class="btn btn-secondary ${node.value > 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 1)">ON</button>
                        <button class="btn btn-secondary ${node.value === 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 0)">OFF</button>
                    </div>
                </div>`;
        } else if (node.type.includes("Light") || node.type.includes("RGB")) {
            badgeColor = "var(--neon-purple)";
            badgeBg = "rgba(138, 43, 226, 0.15)";
            controlWidget = `
                <div class="control-group">
                    <label><i class="fa-solid fa-lightbulb"></i> Element Command:</label>
                    <div style="display:flex; gap:10px; margin-top:5px;">
                        <button class="btn btn-secondary ${node.value > 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 1)">ON</button>
                        <button class="btn btn-secondary ${node.value === 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 0)">OFF</button>
                    </div>
                </div>`;
        } else {
            badgeColor = "var(--neon-amber)";
            badgeBg = "rgba(255, 179, 0, 0.15)";
            controlWidget = `
                <div class="control-group" style="text-align: center; padding: 10px 0;">
                    <span style="font-size:0.85rem; color:var(--text-secondary); font-weight:600;"><i class="fa-solid fa-circle-check animate-pulse-glow"></i> Element Active</span>
                </div>`;
        }
        
        card.innerHTML = `
            <div class="node-header">
                <span class="node-addr">${addr}</span>
                <span class="node-type-badge" style="color: ${badgeColor}; border-color: ${badgeColor}; background: ${badgeBg};">${node.type}</span>
            </div>
            <div class="node-controls">
                <p style="font-size:0.85rem; font-weight:600; color:var(--text-primary);"><i class="fa-solid fa-microchip" style="color: ${badgeColor}; margin-right: 6px;"></i>${node.name}</p>
                ${controlWidget}
            </div>
        `;
        topoViewport.appendChild(card);
    });
}

window.sendNodeElementCmd = (addr, elementIdx, elementType, val) => {
    // Update local state cache instantly for premium responsive feel
    if (state.nodes[addr]) {
        state.nodes[addr].value = parseInt(val);
        renderTopography();
        renderMxcpCommands();
    }
    
    // Broadcast dynamic MXSP binary element control frame
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "el_cmd",
            element_idx: parseInt(elementIdx),
            element_type: parseInt(elementType),
            func_id: 0,
            value: parseInt(val)
        }));
        writeCliOutput(`[System] Sent dynamic element control command (el_idx: ${elementIdx}, type: ${elementType}, val: ${val})\n`);
    }
};

const mxcpCommandsTbody = document.getElementById("mxcp-commands-tbody");

function renderMxcpCommands() {
    if (!mxcpCommandsTbody) return;
    
    // Filter out root element (index 0 / 0x0000)
    const validAddresses = Object.keys(state.nodes).filter(addr => {
        const node = state.nodes[addr];
        return node.element_idx !== 0 && addr !== "0x0000";
    });
    
    if (!socket || socket.readyState !== WebSocket.OPEN || validAddresses.length === 0) {
        mxcpCommandsTbody.innerHTML = `
            <tr class="empty-row">
                <td colspan="5" style="text-align: center; color: var(--text-secondary); padding: 20px;">
                    No customer/OEM elements discovered yet. Click "Enable MXCP" and "Refresh Topography" in the header to discover.
                </td>
            </tr>`;
        return;
    }
    
    mxcpCommandsTbody.innerHTML = "";
    
    validAddresses.forEach(addr => {
        const node = state.nodes[addr];
        const lastReply = node.lastReply || "STATUS: IDLE";
        
        // Let's create rows for the commands!
        if (node.element_type === 0) { // Relay Server
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-input" style="background: rgba(0, 229, 255, 0.15); color: var(--neon-teal); border: 1px solid var(--neon-teal);">Relay Server</span></td>
                <td>On/Off Toggle (Func ID: 0)</td>
                <td>
                    <div style="display: flex; gap: 8px;">
                        <button class="btn btn-secondary btn-table-control ${node.value > 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 1)">ON</button>
                        <button class="btn btn-secondary btn-table-control ${node.value === 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 0)">OFF</button>
                    </div>
                </td>
                <td><span class="status-reply-cell">${node.value > 0 ? "STATUS: ON" : "STATUS: OFF"}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr);
            
        } else if (node.element_type === 2) { // CWWW Light
            // Row 1: On/Off Toggle
            const tr1 = document.createElement("tr");
            tr1.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-pwm" style="background: rgba(255, 179, 0, 0.15); color: var(--neon-amber); border: 1px solid var(--neon-amber);">CWWW Light</span></td>
                <td>On/Off Toggle (Func ID: 0)</td>
                <td>
                    <div style="display: flex; gap: 8px;">
                        <button class="btn btn-secondary btn-table-control ${node.value > 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 1)">ON</button>
                        <button class="btn btn-secondary btn-table-control ${node.value === 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 0)">OFF</button>
                    </div>
                </td>
                <td><span class="status-reply-cell">${node.value > 0 ? "STATUS: ON" : "STATUS: OFF"}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr1);
            
            // Row 2: CTL Lightness & Temp
            const tr2 = document.createElement("tr");
            const lightValId = `cwww-light-${node.element_idx}`;
            const tempValId = `cwww-temp-${node.element_idx}`;
            tr2.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-pwm" style="background: rgba(255, 179, 0, 0.15); color: var(--neon-amber); border: 1px solid var(--neon-amber);">CWWW Light</span></td>
                <td>CTL Level (Func ID: 1)</td>
                <td>
                    <div style="display: flex; flex-direction: column; gap: 6px; min-width: 180px;">
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 0.75rem; width: 60px; color: var(--text-secondary);">Lightness:</span>
                            <input type="range" id="${lightValId}" class="slider-control" min="0" max="65535" value="32768" style="flex: 1;">
                        </div>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 0.75rem; width: 60px; color: var(--text-secondary);">Temp:</span>
                            <input type="range" id="${tempValId}" class="slider-control" min="0" max="65535" value="32768" style="flex: 1;">
                        </div>
                        <button class="btn btn-primary btn-table-control" style="align-self: flex-start; padding: 2px 10px; margin-top: 4px;"
                            onclick="sendCwwwCtlCmd('${addr}', ${node.element_idx}, document.getElementById('${lightValId}').value, document.getElementById('${tempValId}').value)">
                            Set CTL
                        </button>
                    </div>
                </td>
                <td><span class="status-reply-cell" id="status-reply-cwww-${node.element_idx}">${lastReply}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr2);
            
        } else if (node.element_type === 4) { // RGB / HSL Light
            // Row 1: On/Off Toggle
            const tr1 = document.createElement("tr");
            tr1.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-output" style="background: rgba(138, 43, 226, 0.15); color: var(--neon-purple); border: 1px solid var(--neon-purple);">RGB Light</span></td>
                <td>On/Off Toggle (Func ID: 0)</td>
                <td>
                    <div style="display: flex; gap: 8px;">
                        <button class="btn btn-secondary btn-table-control ${node.value > 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 1)">ON</button>
                        <button class="btn btn-secondary btn-table-control ${node.value === 0 ? "active" : ""}" 
                            onclick="sendNodeElementCmd('${addr}', ${node.element_idx}, ${node.element_type}, 0)">OFF</button>
                    </div>
                </td>
                <td><span class="status-reply-cell">${node.value > 0 ? "STATUS: ON" : "STATUS: OFF"}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr1);
            
            // Row 2: HSL Color
            const tr2 = document.createElement("tr");
            const hId = `rgb-h-${node.element_idx}`;
            const sId = `rgb-s-${node.element_idx}`;
            const lId = `rgb-l-${node.element_idx}`;
            tr2.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-output" style="background: rgba(138, 43, 226, 0.15); color: var(--neon-purple); border: 1px solid var(--neon-purple);">RGB Light</span></td>
                <td>HSL Color (Func ID: 1)</td>
                <td>
                    <div style="display: flex; flex-direction: column; gap: 6px; min-width: 180px;">
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 0.75rem; width: 60px; color: var(--text-secondary);">Hue:</span>
                            <input type="range" id="${hId}" class="slider-control" min="0" max="65535" value="32768" style="flex: 1;">
                        </div>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 0.75rem; width: 60px; color: var(--text-secondary);">Sat:</span>
                            <input type="range" id="${sId}" class="slider-control" min="0" max="65535" value="65535" style="flex: 1;">
                        </div>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 0.75rem; width: 60px; color: var(--text-secondary);">Light:</span>
                            <input type="range" id="${lId}" class="slider-control" min="0" max="65535" value="32768" style="flex: 1;">
                        </div>
                        <button class="btn btn-primary btn-table-control" style="align-self: flex-start; padding: 2px 10px; margin-top: 4px;"
                            onclick="sendHslColorCmd('${addr}', ${node.element_idx}, document.getElementById('${hId}').value, document.getElementById('${sId}').value, document.getElementById('${lId}').value)">
                            Set HSL
                        </button>
                    </div>
                </td>
                <td><span class="status-reply-cell" id="status-reply-rgb-${node.element_idx}">${lastReply}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr2);
            
        } else if (node.element_type === 6) { // Sensor Server
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><strong>Element ${node.element_idx}</strong></td>
                <td><span class="pin-tag tag-input" style="background: rgba(0, 230, 118, 0.15); color: var(--neon-emerald); border: 1px solid var(--neon-emerald);">Sensor Server</span></td>
                <td>Read Value (Func ID: 0)</td>
                <td>
                    <button class="btn btn-primary btn-table-control" 
                        onclick="sendSensorReadCmd('${addr}', ${node.element_idx})">
                        <i class="fa-solid fa-gauge"></i> Read Sensor
                    </button>
                </td>
                <td><span class="status-reply-cell" id="status-reply-sensor-${node.element_idx}">${lastReply}</span></td>
            `;
            mxcpCommandsTbody.appendChild(tr);
        }
    });
}

window.sendCwwwCtlCmd = (addr, elementIdx, lightness, temp) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "el_cmd_cwww",
            element_idx: parseInt(elementIdx),
            element_type: 2,
            func_id: 1,
            lightness: parseInt(lightness),
            temperature: parseInt(temp)
        }));
        const statusCell = document.getElementById(`status-reply-cwww-${elementIdx}`);
        if (statusCell) statusCell.innerText = "STATUS: SENT";
        writeCliOutput(`[System] Sent CWWW CTL command (el_idx: ${elementIdx}, lightness: ${lightness}, temp: ${temp})\n`);
    }
};

window.sendHslColorCmd = (addr, elementIdx, hue, sat, light) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "el_cmd_hsl",
            element_idx: parseInt(elementIdx),
            element_type: 4,
            func_id: 1,
            hue: parseInt(hue),
            saturation: parseInt(sat),
            lightness: parseInt(light)
        }));
        const statusCell = document.getElementById(`status-reply-rgb-${elementIdx}`);
        if (statusCell) statusCell.innerText = "STATUS: SENT";
        writeCliOutput(`[System] Sent HSL color command (el_idx: ${elementIdx}, H: ${hue}, S: ${sat}, L: ${light})\n`);
    }
};

window.sendSensorReadCmd = (addr, elementIdx) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            type: "el_cmd_sensor",
            element_idx: parseInt(elementIdx),
            element_type: 6,
            func_id: 0
        }));
        const statusCell = document.getElementById(`status-reply-sensor-${elementIdx}`);
        if (statusCell) statusCell.innerText = "STATUS: READING...";
        writeCliOutput(`[System] Sent Sensor Read request (el_idx: ${elementIdx})\n`);
    }
};

// 6. Async Device Flashing Trigger Handler
async function triggerDeviceFlash() {
    const port = portSelect.value;
    if (!port) {
        alert("Please select a valid serial port to flash first.");
        return;
    }
    
    const bsp = bspSelect.value;
    const product = profileSelect.value;
    if (!bsp || !product) {
        alert("Please select a BSP and Product profile first.");
        return;
    }
    
    const erase = eraseFlashChk.checked;
    
    flashBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Flashing...';
    flashBtn.disabled = true;
    if (flashMxcBtn) flashMxcBtn.disabled = true;
    
    try {
        const res = await fetch(`${BACKEND_URL}/api/flash?port=${encodeURIComponent(port)}&bsp=${bsp}&product=${product}&erase=${erase}`, {
            method: "POST"
        });
        const data = await res.json();
        if (data.status === "success") {
            writeCliOutput(`\n[System] Flash pipeline triggered successfully. Streaming compile & write logs below:\n`);
        } else {
            writeCliOutput(`\n[System] Failed to trigger flash: ${data.detail || data.message}\n`);
            flashBtn.innerHTML = '<i class="fa-solid fa-bolt"></i> Flash Firmware';
            flashBtn.disabled = false;
            if (flashMxcBtn) flashMxcBtn.disabled = !(bspSelect.value && profileSelect.value);
        }
    } catch (e) {
        console.error("Flash failed:", e);
        writeCliOutput(`\n[System] Server flash endpoint connection error.\n`);
        flashBtn.innerHTML = '<i class="fa-solid fa-bolt"></i> Flash Firmware';
        flashBtn.disabled = false;
        if (flashMxcBtn) flashMxcBtn.disabled = !(bspSelect.value && profileSelect.value);
    }
}

// 7. Hook Event Listeners
connectBtn.addEventListener("click", () => {
    if (socket) {
        socket.close();
    } else {
        connectPort();
    }
});

flashBtn.addEventListener("click", triggerDeviceFlash);

cliInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
        sendCliCommand(cliInput.value);
    }
});

cliSendBtn.addEventListener("click", () => {
    sendCliCommand(cliInput.value);
});

autoscrollBtn.addEventListener("click", () => {
    autoScroll = !autoScroll;
    autoscrollBtn.className = `btn btn-secondary ${autoScroll ? "active" : ""}`;
});

clearLogsBtn.addEventListener("click", () => {
    state.logs = [];
    renderLogs();
});

clearPacketsBtn.addEventListener("click", () => {
    state.packets = [];
    renderPackets();
});

exportLogsBtn.addEventListener("click", () => {
    if (state.logs.length === 0) {
        alert("Log buffer is empty. Nothing to export.");
        return;
    }
    const blob = new Blob([JSON.stringify(state.logs, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `meshx_console_logs_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
});

// Real-time filter updates
[filterErr, filterWrn, filterDbg].forEach(el => {
    el.addEventListener("change", renderLogs);
});

// Hook Refresh Composition Button
const refreshCompositionBtn = document.getElementById("refresh-composition-btn");
if (refreshCompositionBtn) {
    refreshCompositionBtn.addEventListener("click", () => {
        if (socket && socket.readyState === WebSocket.OPEN) {
            fetch(`${BACKEND_URL}/api/composition/request?port=${encodeURIComponent(currentPort)}`, { method: "POST" })
                .then(res => res.json())
                .then(data => {
                    writeCliOutput("[System] Requested elements composition query from target node over serial line...\n");
                })
                .catch(err => {
                    console.error("Composition request failed:", err);
                    writeCliOutput("[System] Error: Failed to trigger elements composition query.\n");
                });
        } else {
            alert("Please select a serial port and click Connect first.");
        }
    });
}

// Fetch available target configuration metadata and populate selectors
async function loadConfigMetadata() {
    try {
        const res = await fetch(`${BACKEND_URL}/api/config-metadata`);
        const data = await res.json();
        configMetadata = data.bsps || {};
        
        // Populate BSP Selector
        bspSelect.innerHTML = '<option value="">Select BSP...</option>';
        Object.keys(configMetadata).forEach(bsp => {
            const opt = document.createElement("option");
            opt.value = bsp;
            opt.textContent = bsp;
            bspSelect.appendChild(opt);
        });
    } catch (e) {
        console.error("Failed to load target configuration metadata:", e);
        writeCliOutput("[System] Error: Failed to fetch build configuration metadata from backend.\n");
    }
}

// Hook BSP selector change event listener
if (bspSelect) {
    bspSelect.addEventListener("change", () => {
        const bsp = bspSelect.value;
        profileSelect.innerHTML = '<option value="">Select Product...</option>';
        
        if (bsp && configMetadata[bsp]) {
            profileSelect.disabled = false;
            configMetadata[bsp].forEach(product => {
                const opt = document.createElement("option");
                opt.value = product;
                opt.textContent = product;
                profileSelect.appendChild(opt);
            });
        } else {
            profileSelect.disabled = true;
        }
        
        // Update flash buttons state
        updateFlashButtonsState();
    });
}

// Hook Profile selector change event listener
if (profileSelect) {
    profileSelect.addEventListener("change", () => {
        updateFlashButtonsState();
        
        // If port is connected, dynamically notify the backend so the serial log decoder loads the correct ELF compiler artifact
        const port = portSelect.value;
        const bsp = bspSelect.value;
        const product = profileSelect.value;
        if (port && socket && bsp && product) {
            fetch(`${BACKEND_URL}/api/port/set-bsp-profile?port=${encodeURIComponent(port)}&bsp=${bsp}&product=${product}`, {
                method: "POST"
            }).catch(err => console.error("Error setting port profile:", err));
        }
    });
}

function updateFlashButtonsState() {
    const active = bspSelect.value && profileSelect.value;
    flashBtn.disabled = !active;
    if (flashMxcBtn) {
        flashMxcBtn.disabled = !active;
    }
}

// Hook Flash Config click event listener
if (flashMxcBtn) {
    flashMxcBtn.addEventListener("click", async () => {
        const port = portSelect.value;
        if (!port) {
            alert("Please select a valid serial port to flash first.");
            return;
        }
        
        const bsp = bspSelect.value;
        const product = profileSelect.value;
        if (!bsp || !product) {
            alert("Please select a BSP and Product profile first.");
            return;
        }
        
        flashMxcBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Flashing...';
        flashMxcBtn.disabled = true;
        flashBtn.disabled = true;
        
        try {
            const res = await fetch(`${BACKEND_URL}/api/flash/config?port=${encodeURIComponent(port)}&bsp=${bsp}&product=${product}`, {
                method: "POST"
            });
            const data = await res.json();
            if (data.status === "success") {
                writeCliOutput(`\n[System] Custom configuration flash pipeline triggered successfully. Streaming write logs below:\n`);
            } else {
                writeCliOutput(`\n[System] Failed to trigger custom flash: ${data.detail || data.message}\n`);
                flashMxcBtn.innerHTML = '<i class="fa-solid fa-arrow-up-from-bracket"></i> Flash Config';
                flashMxcBtn.disabled = false;
                flashBtn.disabled = false;
            }
        } catch (e) {
            console.error("Flash config failed:", e);
            writeCliOutput(`\n[System] Server custom flash config endpoint connection error.\n`);
            flashMxcBtn.innerHTML = '<i class="fa-solid fa-arrow-up-from-bracket"></i> Flash Config';
            flashMxcBtn.disabled = false;
            flashBtn.disabled = false;
        }
    });
}

if (mxcpShiftBtn) {
    mxcpShiftBtn.addEventListener("click", async () => {
        const port = portSelect.value;
        if (!port) {
            alert("Please select a valid serial port to enable MXCP first.");
            return;
        }
        
        mxcpShiftBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Enabling...';
        mxcpShiftBtn.disabled = true;
        
        try {
            const res = await fetch(`${BACKEND_URL}/api/mxcp/enable?port=${encodeURIComponent(port)}`, {
                method: "POST"
            });
            const data = await res.json();
            if (data.status === "success") {
                writeCliOutput(`\n[System] MXCP channel shifting sent ('ut 8 1 1 1') and element composition query triggered.\n`);
            } else {
                writeCliOutput(`\n[System] Failed to enable MXCP: ${data.detail || data.message}\n`);
            }
        } catch (e) {
            console.error("MXCP shift failed:", e);
            writeCliOutput(`\n[System] Server MXCP enable endpoint connection error.\n`);
        } finally {
            mxcpShiftBtn.innerHTML = '<i class="fa-solid fa-shuffle"></i> Enable MXCP';
            mxcpShiftBtn.disabled = false;
        }
    });
}

// Load Ports, metadata and Initialise Offline States on page load
scanPorts();
loadConfigMetadata();
detectNodesFromState();
renderGpioPipeline();
renderMxcpCommands();
