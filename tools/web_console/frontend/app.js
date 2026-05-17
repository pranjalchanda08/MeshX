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
        writeCliOutput(`\nConnected to ${port} successfully. Switch to Hosted mode activated.\n`);
        
        // Query initial GPIO configurations automatically
        sendGpioCommand(12, 8, 0); // Pin 12 config
        sendGpioCommand(23, 8, 0); // Pin 23 config
        sendGpioCommand(25, 8, 0); // Pin 25 config
    };
    
    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        handleIncomingEvent(msg);
    };
    
    socket.onclose = () => {
        connectBtn.innerHTML = '<i class="fa-solid fa-link"></i> Connect';
        connectBtn.className = "btn btn-primary";
        connectBtn.disabled = false;
        socket = null;
        writeCliOutput(`\nDisconnected from port ${port}.\n`);
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
        renderGpioPipeline();
        renderLogs();
        detectNodesFromState();
    } else if (evt.type === "log") {
        state.logs.push(evt);
        if (state.logs.length > 200) state.logs.shift();
        appendLogLine(evt);
    } else if (evt.type === "text") {
        writeCliOutput(evt.data);
    } else if (evt.type === "mxsp") {
        state.packets.push(evt);
        if (state.packets.length > 30) state.packets.shift();
        renderPackets();
        detectNodeFromMxsp(evt);
    } else if (evt.type === "gpio_update") {
        state.gpio[evt.pin] = evt;
        renderGpioPipeline();
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
        div.className = "packet-item";
        
        let typeStr = `OPCODE 0x${p.msg_type.toString(16).toUpperCase()}`;
        if (p.msg_type === 0xB1) typeStr = "SYS_EVT_NOTIFY (0xB1)";
        else if (p.msg_type === 0xB2) typeStr = "DATA_EVT_NOTIFY (0xB2)";
        else if (p.msg_type === 0xC1) typeStr = "EL_CMD_SEND (0xC1)";
        else if (p.msg_type === 0xD1) typeStr = "GPIO_CMD (0xD1)";
        else if (p.msg_type === 0xD2) typeStr = "GPIO_RSP (0xD2)";
        else if (p.msg_type === 0xD3) typeStr = "GPIO_EVT (0xD3)";
        
        // Pretty print raw hex payload spaces
        const hex = p.payload.toUpperCase().match(/.{1,2}/g)?.join(" ") || p.payload;
        
        div.innerHTML = `
            <div class="pkt-meta">
                <span class="pkt-type">${typeStr}</span>
                <span class="pkt-time">${new Date().toLocaleTimeString()}</span>
            </div>
            <div class="pkt-bytes">${hex}</div>
        `;
        packetsTimeline.appendChild(div);
    });
    packetsTimeline.scrollTop = packetsTimeline.scrollHeight;
}

// Render GPIO statuses
function renderGpioPipeline() {
    gpioGrid.innerHTML = "";
    
    // We render mock system pins derived from physical config:
    // Pin 12 (INPUT Button), Pin 23 (OUTPUT Relay), Pin 25 (PWM LED Dimmer)
    const pins = [
        { pin: 12, label: "INPUT Button 1", tag: "INPUT", class: "tag-input" },
        { pin: 23, label: "OUTPUT Relay 1", tag: "OUTPUT", class: "tag-output", toggleable: true },
        { pin: 25, label: "PWM LED Dimmer", tag: "PWM", class: "tag-pwm", pwm: true }
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
                <button class="pin-toggle-btn" onclick="triggerPinToggle(${p.pin}, ${level})">
                    <i class="fa-solid fa-power-off"></i>
                </button>`;
        } else if (p.pwm) {
            const dutyPercent = Math.round((level / 255) * 100) || 0;
            levelControls = `
                <div class="pwm-meter-container" style="margin-left:15px;">
                    <div class="pwm-bar-fill" style="width: ${dutyPercent}%;"></div>
                    <span class="pwm-percent">${dutyPercent}%</span>
                </div>
                <input type="range" class="slider-control" style="width:100px; margin-left:10px;" min="0" max="255" value="${level}" 
                    oninput="triggerPwmDuty(${p.pin}, this.value)">`;
        } else {
            // Standard input
            levelControls = `<span class="status-indicator-light ${lightClass}"></span>`;
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
    // Initialize nodes
    state.nodes = {};
    topoViewport.innerHTML = "";
    
    // Automatically discover common test nodes
    addMeshNode("0x0001", "RGB Smart Lamp", "HSL", 255);
    addMeshNode("0x0002", "4-Relay Control Panel", "Relay", 0);
}

function detectNodeFromMxsp(pkt) {
    // If telemetry notify packet comes, hydrate dynamic nodes
    if (pkt.msg_type === 0xB1 || pkt.msg_type === 0xB2) {
        // Automatically extract address from frame and build card
        const hexAddr = "0x" + pkt.payload.slice(0, 4).toUpperCase();
        if (!state.nodes[hexAddr]) {
            addMeshNode(hexAddr, `MeshX Node ${hexAddr}`, "Relay", 0);
        }
    }
}

function addMeshNode(address, name, type, value) {
    state.nodes[address] = { name, type, value };
    renderTopography();
}

function renderTopography() {
    const nodeKeys = Object.keys(state.nodes);
    if (nodeKeys.length === 0) {
        topoViewport.innerHTML = `
            <div class="empty-state">
                <i class="fa-solid fa-satellite-dish"></i>
                <p>No active BLE Mesh nodes detected.<br>Connect to start auto-discovery.</p>
            </div>`;
        return;
    }
    
    topoViewport.innerHTML = "";
    nodeKeys.forEach(addr => {
        const node = state.nodes[addr];
        const card = document.createElement("div");
        card.className = "mesh-node";
        
        let controlWidget = "";
        if (node.type === "HSL") {
            controlWidget = `
                <div class="control-group">
                    <label><i class="fa-solid fa-sun"></i> Brightness Level:</label>
                    <input type="range" class="slider-control" min="0" max="255" value="${node.value}" 
                        onchange="sendNodeElementCmd('${addr}', 0, this.value)">
                </div>`;
        } else {
            controlWidget = `
                <div class="control-group">
                    <label><i class="fa-solid fa-power-off"></i> Relay Switch State:</label>
                    <div style="display:flex; gap:10px; margin-top:5px;">
                        <button class="btn btn-secondary ${node.value > 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', 1, 1)">ON</button>
                        <button class="btn btn-secondary ${node.value === 0 ? "active" : ""}" style="flex:1; padding:4px;" 
                            onclick="sendNodeElementCmd('${addr}', 1, 0)">OFF</button>
                    </div>
                </div>`;
        }
        
        card.innerHTML = `
            <div class="node-header">
                <span class="node-addr">${addr}</span>
                <span class="node-type-badge">${node.type} Node</span>
            </div>
            <div class="node-controls">
                <p style="font-size:0.85rem; font-weight:600; color:var(--text-primary);">${node.name}</p>
                ${controlWidget}
            </div>
        `;
        topoViewport.appendChild(card);
    });
}

window.sendNodeElementCmd = (addr, elementIdx, val) => {
    // Update local state cache instantly for premium responsive feel
    if (state.nodes[addr]) {
        state.nodes[addr].value = parseInt(val);
        renderTopography();
    }
    
    // Broadcast node control CLI or dynamic MXSP frames
    const addressInt = parseInt(addr, 16);
    // Send ut command via CLI console to drive active bound control widgets
    // ut 8 1 1 <0|1>
    if (elementIdx === 1) {
        sendCliCommand(`ut 8 1 1 ${val}`);
    } else {
        // Drive PWM led
        sendGpioCommand(25, 4, parseInt(val));
    }
};

// 6. Hook Event Listeners
connectBtn.addEventListener("click", () => {
    if (socket) {
        socket.close();
    } else {
        connectPort();
    }
});

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

// Load Ports on page load
scanPorts();
detectNodesFromState();
renderGpioPipeline();
