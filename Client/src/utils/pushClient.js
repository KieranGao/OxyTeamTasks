/**
 * PushServer WebSocket client.
 *
 * Connects to PushServer after login, handles auto-reconnect,
 * and dispatches incoming messages to the rest of the app via callbacks.
 *
 * Usage (in store or component):
 *   import { connectPushServer, onMessage, disconnect } from '@/utils/pushClient'
 *   connectPushServer('127.0.0.1', 8890)
 *   onMessage('notify', (data) => { console.log(data) })
 */

let ws = null
let reconnectTimer = null
let host = ''
let port = ''
let uid = 0
let token = ''
let shouldReconnect = false

const handlers = new Map() // type → [callback, ...]

/**
 * Connect to PushServer via WebSocket and perform TCP login.
 */
export function connectPushServer(serverHost, serverPort, userUid, userToken) {
  host = serverHost
  port = serverPort
  uid = userUid
  token = userToken
  shouldReconnect = true

  doConnect()
}

function doConnect() {
  if (ws) {
    ws.close()
    ws = null
  }

  const url = `ws://${host}:${port}`
  console.log('[PushClient] connecting to', url)
  ws = new WebSocket(url)

  ws.onopen = () => {
    console.log('[PushClient] connected, sending login')
    // TCP login handshake
    sendMessage({
      type: 'login',
      uid,
      token,
    })
  }

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data)
      const type = data.type
      console.log('[PushClient] received:', type, data)

      if (handlers.has(type)) {
        handlers.get(type).forEach((fn) => fn(data))
      }
      // Also fire wildcard handlers
      if (handlers.has('*')) {
        handlers.get('*').forEach((fn) => fn(data))
      }
    } catch (e) {
      console.error('[PushClient] parse error:', e)
    }
  }

  ws.onclose = (event) => {
    console.log('[PushClient] disconnected:', event.code, event.reason)
    ws = null
    if (shouldReconnect) {
      const delay = Math.min((reconnectTimer || 0) + 2000, 30000)
      console.log('[PushClient] reconnecting in', delay, 'ms')
      reconnectTimer = setTimeout(doConnect, delay)
    }
  }

  ws.onerror = (err) => {
    console.error('[PushClient] error:', err)
  }
}

/**
 * Disconnect and stop auto-reconnect.
 */
export function disconnect() {
  shouldReconnect = false
  clearTimeout(reconnectTimer)
  reconnectTimer = null
  if (ws) {
    ws.close()
    ws = null
  }
}

/**
 * Send a JSON message through the WebSocket.
 */
export function sendMessage(data) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(data))
  } else {
    console.warn('[PushClient] not connected, cannot send')
  }
}

/**
 * Register a handler for a specific message type (or '*' for all).
 */
export function onMessage(type, callback) {
  if (!handlers.has(type)) {
    handlers.set(type, [])
  }
  handlers.get(type).push(callback)
}

/**
 * Remove a handler.
 */
export function offMessage(type, callback) {
  if (handlers.has(type)) {
    const list = handlers.get(type).filter((fn) => fn !== callback)
    handlers.set(type, list)
  }
}

/**
 * Check if WebSocket is currently connected.
 */
export function isConnected() {
  return ws !== null && ws.readyState === WebSocket.OPEN
}
