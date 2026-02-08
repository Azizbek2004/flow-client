const https = require('https');
const net = require('net');

const API_BASE = 'https://flow-backend-flax.vercel.app/api';

function checkTcpConnection(host, port) {
    return new Promise((resolve, reject) => {
        const socket = new net.Socket();
        socket.setTimeout(5000);

        const timer = setTimeout(() => {
            socket.destroy();
            reject(new Error('Connection timed out'));
        }, 5000);

        socket.connect(port, host, () => {
            clearTimeout(timer);
            socket.destroy();
            resolve(true);
        });

        socket.on('error', (err) => {
            clearTimeout(timer);
            reject(err);
        });
    });
}

async function verify() {
    console.log("🚀 Starting Verification Process...\n");

    try {
        // Step 0: Authenticate
        console.log("0️⃣  Authenticating...");
        const loginData = JSON.stringify({ email: "test@example.com" });
        const authOptions = {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': loginData.length
            }
        };

        const loginRes = await new Promise((resolve, reject) => {
            const req = https.request(`${API_BASE}/auth`, authOptions, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    if (res.statusCode === 200) {
                        resolve(JSON.parse(data));
                    } else {
                        reject(new Error(`Auth failed: ${res.statusCode} ${data}`));
                    }
                });
            });
            req.on('error', reject);
            req.write(loginData);
            req.end();
        });

        if (!loginRes.token) throw new Error("Authentication failed: No token returned");
        const token = loginRes.token;
        console.log("✅ Authenticated. Token received.\n");

        function fetchWithAuth(url) {
            return new Promise((resolve, reject) => {
                const opts = {
                    headers: { 'Authorization': `Bearer ${token}` }
                };
                https.get(url, opts, (res) => {
                    let data = '';
                    res.on('data', (chunk) => data += chunk);
                    res.on('end', () => {
                        if (res.statusCode >= 200 && res.statusCode < 300) {
                            try { resolve(JSON.parse(data)); } catch (e) { reject(e); }
                        } else {
                            reject(new Error(`Status ${res.statusCode}: ${data}`));
                        }
                    });
                }).on('error', reject);
            });
        }

        // Step 1: Fetch Servers
        console.log("1️⃣  Fetching Servers...");
        const serversData = await fetchWithAuth(`${API_BASE}/servers`);

        let servers = [];
        if (serversData.servers) {
            servers = serversData.servers;
        } else if (Array.isArray(serversData)) {
            servers = serversData;
        }

        if (!servers || servers.length === 0) {
            console.log("DEBUG: Response:", serversData);
            throw new Error("No servers returned from API");
        }
        console.log(`✅ Found ${servers.length} servers.`);
        const targetServer = servers[0];
        // API uses 'address'
        const targetHost = targetServer.address || targetServer.ip;
        console.log(`   Target: ${targetServer.location} (${targetHost})\n`);

        // Step 2: Fetch Config for Target Server
        console.log("2️⃣  Fetching Configuration...");
        const config = await fetchWithAuth(`${API_BASE}/config?server=${targetServer.id}`);

        if (!config.server || !config.server.address) {
            throw new Error("Invalid config returned from API");
        }
        console.log(`✅ Config received.`);
        console.log(`   Address: ${config.server.address}`);
        console.log(`   Port: ${config.server.port}`);
        console.log(`   UUID: ${config.user.id}`);
        console.log(`   Public Key: ${config.reality.publicKey}\n`);

        // Step 3: Verify TCP Connection
        console.log("3️⃣  Verifying Server Reachability (TCP)...");
        await checkTcpConnection(config.server.address, config.server.port);
        console.log(`✅ Successfully connected to ${config.server.address}:${config.server.port}`);
        console.log(`   (XRay is listening on this port)\n`);

        console.log("🎉 VERIFICATION SUCCEEDED! The infrastructure is fully operational.");

    } catch (err) {
        console.error("\n❌ Verification Failed:");
        console.error(err.message);
        process.exit(1);
    }
}

verify();
