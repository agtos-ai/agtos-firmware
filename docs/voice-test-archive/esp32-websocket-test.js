#!/usr/bin/env node
/*
 * Provenance: extracted from the agtOS repository (agtos-ai/agtos) at commit
 * c063025e on 2026-08-18, where it lived at `test/esp32-websocket-test.js`.
 * agtOS removed its dead `test/` scratch directory (agtos-ai/agtos issue 1444);
 * this is an ESP32 bring-up harness, so it is archived beside the firmware.
 * It targets the 2025-era agtOS test stack and is NOT maintained.
 */


/**
 * ESP32-S3 Sense WebSocket Test Server
 * 
 * Minimal server to verify ESP32 hardware functionality:
 * - WebSocket connection
 * - Audio streaming
 * - Command protocol
 * - Audio quality assessment
 */

const WebSocket = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');

// Configuration
const PORT = 8765;  // Using 8765 to avoid common dev server ports (300x range)
const AUDIO_DIR = path.join(__dirname, 'audio-samples');

// Ensure audio directory exists
if (!fs.existsSync(AUDIO_DIR)) {
  fs.mkdirSync(AUDIO_DIR, { recursive: true });
}

// Create HTTP server
const server = http.createServer((req, res) => {
  // Handle different routes
  if (req.url === '/events' && req.method === 'GET') {
    // Server-sent events endpoint
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      'Connection': 'keep-alive'
    });
    
    events.push(res);
    
    req.on('close', () => {
      const index = events.indexOf(res);
      if (index !== -1) {
        events.splice(index, 1);
      }
    });
  }
  else if (req.url === '/command' && req.method === 'POST') {
    // Command endpoint
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const { command } = JSON.parse(body);
        
        // Send command to all connected devices
        devices.forEach((device) => {
          device.ws.send(JSON.stringify({ command }));
        });
        
        console.log(`📤 Sent command to ${devices.size} device(s):`, command);
        res.writeHead(200);
        res.end('OK');
      } catch (e) {
        res.writeHead(400);
        res.end('Invalid JSON');
      }
    });
  }
  else if (req.url === '/' || req.url === '/index.html') {
    // Main web UI
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32 Voice Test Server</title>
      <style>
        body { font-family: monospace; padding: 20px; background: #1a1a1a; color: #0f0; }
        .log { background: #000; padding: 10px; height: 400px; overflow-y: auto; margin: 20px 0; }
        button { padding: 10px 20px; margin: 5px; background: #0f0; color: #000; border: none; cursor: pointer; }
        button:hover { background: #0c0; }
        .stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin: 20px 0; }
        .stat { background: #000; padding: 10px; border: 1px solid #0f0; }
      </style>
    </head>
    <body>
      <h1>🎤 ESP32-S3 Sense Voice Test Server</h1>
      
      <div class="stats">
        <div class="stat">
          <strong>Status:</strong> <span id="status">Waiting for ESP32...</span>
        </div>
        <div class="stat">
          <strong>Audio Bytes:</strong> <span id="audioBytes">0</span>
        </div>
        <div class="stat">
          <strong>Messages:</strong> <span id="messageCount">0</span>
        </div>
      </div>
      
      <div>
        <button onclick="sendCommand('start_recording')">Start Recording</button>
        <button onclick="sendCommand('stop_recording')">Stop Recording</button>
        <button onclick="sendCommand('status')">Get Status</button>
        <button onclick="sendCommand('ping')">Ping</button>
        <button onclick="clearLog()">Clear Log</button>
      </div>
      
      <div class="log" id="log"></div>
      
      <script>
        const log = document.getElementById('log');
        const status = document.getElementById('status');
        const audioBytes = document.getElementById('audioBytes');
        const messageCount = document.getElementById('messageCount');
        
        let totalAudioBytes = 0;
        let totalMessages = 0;
        
        function addLog(message, type = 'info') {
          const timestamp = new Date().toLocaleTimeString();
          const color = type === 'error' ? '#f00' : type === 'success' ? '#0f0' : '#ff0';
          log.innerHTML += \`<div style="color: \${color}">[\${timestamp}] \${message}</div>\`;
          log.scrollTop = log.scrollHeight;
        }
        
        function sendCommand(command) {
          fetch('/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command })
          });
          addLog(\`Sent command: \${command}\`, 'info');
        }
        
        function clearLog() {
          log.innerHTML = '';
          addLog('Log cleared', 'info');
        }
        
        // Server-sent events for real-time updates
        const eventSource = new EventSource('/events');
        
        eventSource.onmessage = (event) => {
          const data = JSON.parse(event.data);
          
          if (data.type === 'log') {
            addLog(data.message, data.level);
          }
          
          if (data.type === 'status') {
            status.textContent = data.status;
          }
          
          if (data.type === 'audio') {
            totalAudioBytes += data.bytes;
            audioBytes.textContent = totalAudioBytes.toLocaleString();
          }
          
          if (data.type === 'message') {
            totalMessages++;
            messageCount.textContent = totalMessages;
          }
        };
        
        eventSource.onerror = () => {
          addLog('Lost connection to server', 'error');
          status.textContent = 'Disconnected';
        };
        
        addLog('Web interface connected', 'success');
      </script>
    </body>
    </html>
  `);
  }
  else {
    // 404 for other routes
    res.writeHead(404);
    res.end('Not Found');
  }
});

// WebSocket server
const wss = new WebSocket.Server({ noServer: true });

// Connected ESP32 devices
const devices = new Map();

// Event emitter for web interface
const events = [];
function broadcast(event) {
  events.forEach(res => {
    res.write(`data: ${JSON.stringify(event)}\n\n`);
  });
}

// Handle WebSocket connections
wss.on('connection', (ws, request) => {
  const clientIp = request.connection.remoteAddress || request.headers['x-forwarded-for'];
  const realIP = clientIp.replace('::ffff:', '').replace('::1', 'localhost');
  const userAgent = request.headers['user-agent'] || 'none';
  
  console.log(`\n✅ Client connected:`, {
    ip: realIP,
    userAgent: userAgent,
    origin: request.headers['origin'] || 'none',
    key: request.headers['sec-websocket-key']
  });
  
  // Only broadcast ESP32 connections (no user-agent)
  if (userAgent === 'none' || userAgent.includes('ESP')) {
    broadcast({ type: 'log', message: `ESP32 connected from ${realIP}`, level: 'success' });
    broadcast({ type: 'status', status: 'Waiting for device info...' });
  }
  
  let deviceInfo = null;
  let audioFile = null;
  let audioStream = null;
  let audioChunks = [];
  let totalAudioBytes = 0;
  
  ws.on('message', (data) => {
    // Debug: Check what we received
    if (Buffer.isBuffer(data)) {
      // Try to decode as text first
      const asText = data.toString('utf8');
      if (asText.startsWith('{') || asText.startsWith('[')) {
        // It's JSON sent as binary - convert to string
        console.log('📨 Binary JSON detected, converting to text');
        data = asText;
      } else {
        console.log('📨 Binary audio data:', data.length, 'bytes');
      }
    }
    
    console.log('📨 Raw message:', {
      type: typeof data,
      length: data.length,
      preview: typeof data === 'string' ? data.substring(0, 100) : 'binary'
    });
    
    // Handle text messages (JSON commands/status)
    if (typeof data === 'string') {
      try {
        const message = JSON.parse(data);
        console.log('📥 Parsed JSON:', message);
        broadcast({ type: 'log', message: `Message: ${JSON.stringify(message)}`, level: 'info' });
        broadcast({ type: 'message' });
        
        // Handle device info
        if (message.type === 'device_info') {
          deviceInfo = message;
          devices.set(message.deviceId, { ws, info: deviceInfo, ip: realIP });
          console.log('📱 Device registered:', message.deviceId);
          console.log('   Type:', message.deviceType);
          console.log('   IP:', realIP);
          console.log('   Capabilities:', message.capabilities);
          console.log('   Audio config:', message.audio);
          broadcast({ type: 'log', message: `Device: ${message.deviceId} (${message.deviceType})`, level: 'success' });
          broadcast({ type: 'status', status: 'Connected' });
          broadcast({ type: 'devices', count: devices.size });
        }
        
        // Handle recording events
        if (message.type === 'recording_started') {
          const timestamp = Date.now();
          audioFile = path.join(AUDIO_DIR, `recording_${timestamp}.pcm`);
          audioStream = fs.createWriteStream(audioFile);
          audioChunks = [];
          totalAudioBytes = 0;
          console.log('🎤 Recording started, saving to:', audioFile);
          broadcast({ type: 'log', message: 'Recording started', level: 'success' });
        }
        
        if (message.type === 'recording_stopped') {
          if (audioStream) {
            audioStream.end();
            console.log('🛑 Recording stopped');
            console.log(`   Total bytes: ${totalAudioBytes}`);
            console.log(`   Duration: ~${(totalAudioBytes / (16000 * 2)).toFixed(1)}s`);
            console.log(`   Saved to: ${audioFile}`);
            
            // Save metadata
            const metaFile = audioFile.replace('.pcm', '.json');
            fs.writeFileSync(metaFile, JSON.stringify({
              device: deviceInfo,
              totalBytes: totalAudioBytes,
              chunks: audioChunks.length,
              duration: totalAudioBytes / (16000 * 2),
              timestamp: new Date().toISOString()
            }, null, 2));
            
            broadcast({ type: 'log', message: `Recording saved: ${path.basename(audioFile)}`, level: 'success' });
            audioStream = null;
          }
        }
        
      } catch (e) {
        console.error('❌ Invalid JSON:', e.message);
        broadcast({ type: 'log', message: `Invalid JSON: ${e.message}`, level: 'error' });
      }
    }
    
    // Handle binary messages (audio data)
    else if (Buffer.isBuffer(data)) {
      const bytes = data.length;
      totalAudioBytes += bytes;
      audioChunks.push(data);
      
      // Save to file if recording
      if (audioStream) {
        audioStream.write(data);
      }
      
      // Log audio stats periodically
      if (audioChunks.length % 10 === 0) {
        console.log(`🎵 Audio: ${totalAudioBytes} bytes, ${audioChunks.length} chunks`);
        broadcast({ type: 'audio', bytes });
      }
    }
  });
  
  ws.on('close', (code, reason) => {
    const closeReason = reason ? reason.toString('utf8') : 'No reason';
    console.log('❌ Client disconnected:', { 
      ip: realIP, 
      code, 
      reason: closeReason,
      hadDevice: !!deviceInfo 
    });
    
    // Only broadcast if we had device info (real ESP32)
    if (deviceInfo) {
      broadcast({ type: 'log', message: `ESP32 ${deviceInfo.deviceId} disconnected`, level: 'error' });
      broadcast({ type: 'status', status: 'Disconnected' });
      broadcast({ type: 'devices', count: devices.size - 1 });
    }
    
    if (deviceInfo && devices.has(deviceInfo.deviceId)) {
      devices.delete(deviceInfo.deviceId);
    }
    
    // Close any open streams
    if (audioStream) {
      audioStream.end();
    }
  });
  
  ws.on('error', (error) => {
    console.error('❌ WebSocket error:', error.message);
    broadcast({ type: 'log', message: `WebSocket error: ${error.message}`, level: 'error' });
  });
});

// Handle upgrade for WebSocket on /ws/audio path
server.on('upgrade', (request, socket, head) => {
  if (request.url === '/ws/audio') {
    wss.handleUpgrade(request, socket, head, (ws) => {
      wss.emit('connection', ws, request);
    });
  } else {
    socket.destroy();
  }
});

// Start server
server.listen(PORT, '0.0.0.0', () => {
  console.log(`
╔════════════════════════════════════════════════════════╗
║       ESP32-S3 Sense WebSocket Test Server            ║
╠════════════════════════════════════════════════════════╣
║                                                        ║
║  🌐 WebSocket:  ws://0.0.0.0:${PORT}/ws/audio             ║
║  🖥️  Web UI:     http://localhost:${PORT}                 ║
║  📁 Audio Dir:  ${AUDIO_DIR}        ║
║                                                        ║
║  Waiting for ESP32 to connect...                      ║
║                                                        ║
║  Features:                                            ║
║  • Real-time WebSocket communication                  ║
║  • Audio streaming capture                            ║
║  • Command protocol testing                           ║
║  • PCM audio file saving                              ║
║  • Web-based monitoring UI                            ║
║                                                        ║
╚════════════════════════════════════════════════════════╝
  `);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n\n👋 Shutting down test server...');
  
  devices.forEach((device) => {
    device.ws.close();
  });
  
  server.close(() => {
    console.log('✅ Server closed');
    process.exit(0);
  });
});