#ifndef SIMPLE_WEBSOCKET_H
#define SIMPLE_WEBSOCKET_H

#include <Arduino.h>
#include <WiFiS3.h>

#define MAX_WS_CLIENTS 4

namespace SimpleWS {

// --- Minimal SHA1 & Base64 for Handshake ---
class Crypto {
public:
    static String createAcceptKey(String clientKey) {
        String guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        String input = clientKey + guid;
        uint8_t hash[20];
        sha1(input, hash);
        return base64(hash, 20);
    }

private:
    static void sha1(String input, uint8_t* result) {
        uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
        uint32_t count[2] = {0, 0};
        uint8_t buffer[64];
        
        size_t len = input.length();
        const uint8_t* data = (const uint8_t*)input.c_str();
        
        for (size_t i = 0; i < len; ++i) {
            size_t j = (count[0] >> 3) & 63;
            if ((count[0] += 8) < 8) count[1]++;
            buffer[j] = data[i];
            if (j + 1 == 64) {
                transform(state, buffer);
            }
        }
        
        size_t j = (count[0] >> 3) & 63;
        buffer[j++] = 0x80;
        if (j > 56) {
            while (j < 64) buffer[j++] = 0;
            transform(state, buffer);
            j = 0;
        }
        while (j < 56) buffer[j++] = 0;
        
        uint64_t bits = (uint64_t)count[0] + ((uint64_t)count[1] << 32);
        for (int i = 7; i >= 0; i--) buffer[56 + i] = bits >> ((7 - i) * 8);
        transform(state, buffer);
        
        for (int i = 0; i < 20; i++) result[i] = (state[i >> 2] >> ((3 - (i & 3)) * 8));
    }

    static void transform(uint32_t state[5], const uint8_t buffer[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], x[80];
        for (int i = 0; i < 16; i++) x[i] = (buffer[i*4] << 24) | (buffer[i*4+1] << 16) | (buffer[i*4+2] << 8) | buffer[i*4+3];
        for (int i = 16; i < 80; i++) x[i] = (x[i-3] ^ x[i-8] ^ x[i-14] ^ x[i-16]) << 1 | (x[i-3] ^ x[i-8] ^ x[i-14] ^ x[i-16]) >> 31;
        
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t temp = (a << 5 | a >> 27) + f + e + k + x[i];
            e = d; d = c; c = (b << 30 | b >> 2); b = a; a = temp;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
    }

    static String base64(uint8_t* data, size_t len) {
        const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        String res = "";
        int val = 0, valb = -6;
        for (size_t i = 0; i < len; i++) {
            val = (val << 8) + data[i];
            valb += 8;
            while (valb >= 0) { res += t[(val >> valb) & 0x3F]; valb -= 6; }
        }
        if (valb > -6) res += t[((val << 8) >> (valb + 8)) & 0x3F];
        while (res.length() % 4) res += '=';
        return res;
    }
};

// --- WebSocket Server ---
typedef void (*MsgCallback)(String msg);

class WebSocketServer {
public:
    WebSocketServer(int port) : server(port) {}

    void begin() { server.begin(); }

    void onMessage(MsgCallback cb) { callback = cb; }

    void broadcast(const char* txt) {
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (clients[i]) {
                if (clients[i].connected()) {
                    if (!sendFrame(clients[i], txt)) {
                        clients[i].stop();
                        clients[i] = WiFiClient();
                    }
                } else {
                    clients[i].stop();
                    clients[i] = WiFiClient();
                }
            }
        }
    }

    void broadcast(const String& msg) {
        broadcast(msg.c_str());
    }

    void poll() {
        cleanupDisconnectedClients();

        // Handle new clients
        WiFiClient newClient = server.available();
        if (newClient) {
            int freeIdx = -1;
            for (int i = 0; i < MAX_WS_CLIENTS; i++) {
                if (!clients[i] || !clients[i].connected()) {
                    freeIdx = i;
                    break;
                }
            }

            if (freeIdx != -1) {
                if (clients[freeIdx]) clients[freeIdx].stop();
                clients[freeIdx] = newClient;
                if (!doHandshake(clients[freeIdx])) {
                    clients[freeIdx].stop();
                    clients[freeIdx] = WiFiClient();
                }
            } else {
                newClient.stop(); // Server full
            }
        }

        // Handle data
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (clients[i] && clients[i].connected() && clients[i].available()) {
                readFrame(clients[i]);
            }
        }
    }

private:
    WiFiServer server;
    WiFiClient clients[MAX_WS_CLIENTS];
    MsgCallback callback = nullptr;

    void cleanupDisconnectedClients() {
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (clients[i] && !clients[i].connected()) {
                clients[i].stop();
                clients[i] = WiFiClient();
            }
        }
    }

    // Helper to read N bytes with timeout
    bool readBytes(WiFiClient& client, uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; i++) {
            unsigned long start = millis();
            while (client.available() == 0) {
                if (millis() - start > 400) return false; // 400ms timeout per byte
                if (!client.connected()) return false;
                delay(1);
            }
            int c = client.read();
            if (c < 0) return false;
            buf[i] = (uint8_t)c;
        }
        return true;
    }

    bool doHandshake(WiFiClient& client) {
        String key = "";
        unsigned long start = millis();
        client.setTimeout(200);
        while (client.connected() && millis() - start < 1500) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                line.trim();
                if (line.startsWith("Sec-WebSocket-Key:")) {
                    key = line.substring(18);
                    key.trim();
                }
                if (line.length() == 0 && key.length() > 0) break;
            } else {
                delay(10);
            }
        }
        if (key.length() > 0) {
            client.print("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");
            client.println(Crypto::createAcceptKey(key));
            client.println();
            return true;
        }
        return false;
    }

    bool sendPong(WiFiClient& client, const uint8_t* payload, size_t len) {
        if (!client.connected()) return false;
        uint8_t header[2];
        header[0] = 0x8A; // Pong frame (Fin=1, Opcode=0xA)
        header[1] = (uint8_t)(len & 0x7F); // Pings are control frames, len <= 125
        if (client.write(header, 2) != 2) return false;
        if (len > 0 && payload != nullptr) {
            if (client.write(payload, len) != len) return false;
        }
        return true;
    }

    bool sendFrame(WiFiClient& client, const char* txt) {
        if (!client.connected()) return false;
        size_t len = strlen(txt);
        uint8_t header[4];
        size_t headerLen = 0;
        header[0] = 0x81; // Text frame, fin = 1
        if (len <= 125) {
            header[1] = (uint8_t)len;
            headerLen = 2;
        } else {
            header[1] = 126;
            header[2] = (uint8_t)(len >> 8);
            header[3] = (uint8_t)(len & 0xFF);
            headerLen = 4;
        }
        if (client.write(header, headerLen) != headerLen) return false;
        if (len > 0) {
            if (client.write((const uint8_t*)txt, len) != len) return false;
        }
        return true;
    }

    void readFrame(WiFiClient& client) {
        uint8_t header[2];
        if (!readBytes(client, header, 2)) { client.stop(); return; }

        uint8_t b1 = header[0];
        uint8_t b2 = header[1];
        
        uint8_t opcode = b1 & 0x0F;
        if (opcode == 0x8) { client.stop(); return; } // Close frame

        bool masked = b2 & 0x80;
        uint64_t len = b2 & 0x7F;
        if (len == 126) {
            uint8_t lenBytes[2];
            if (!readBytes(client, lenBytes, 2)) { client.stop(); return; }
            len = (lenBytes[0] << 8) | lenBytes[1];
        }
        
        if (len > 2048) { client.stop(); return; }

        uint8_t mask[4] = {0};
        if (masked) {
            if (!readBytes(client, mask, 4)) { client.stop(); return; }
        }

        // Handle Ping (Opcode 0x9)
        if (opcode == 0x9) {
            uint8_t pingPayload[128];
            size_t pingLen = (len < 128) ? (size_t)len : 125;
            if (pingLen > 0) {
                if (!readBytes(client, pingPayload, pingLen)) { client.stop(); return; }
                if (masked) {
                    for (size_t i = 0; i < pingLen; i++) pingPayload[i] ^= mask[i % 4];
                }
            }
            sendPong(client, pingPayload, pingLen);
            return;
        } else if (opcode == 0xA) {
            // Pong received: discard payload
            for (size_t i = 0; i < len; i++) {
                if (client.read() < 0) break;
            }
            return;
        }

        String payload = "";
        payload.reserve(len);
        
        const size_t CHUNK_SIZE = 64;
        uint8_t buf[CHUNK_SIZE];
        size_t remaining = (size_t)len;
        size_t totalRead = 0;

        while (remaining > 0) {
            size_t toRead = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
            if (!readBytes(client, buf, toRead)) { client.stop(); return; }
            
            for (size_t i = 0; i < toRead; i++) {
                if (masked) buf[i] ^= mask[(totalRead + i) % 4];
                payload += (char)buf[i];
            }
            remaining -= toRead;
            totalRead += toRead;
        }

        if (opcode == 0x1 && callback) callback(payload);
    }
};

} // namespace
#endif