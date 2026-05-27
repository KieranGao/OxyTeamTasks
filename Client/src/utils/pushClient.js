/**
 * PushServer WebSocket client.
 *
 * Connects to PushServer after login, handles auto-reconnect,
 * and dispatches incoming messages to the rest of the app via callbacks.
 *
 * Usage (in store or component):
 *   import { connectPushServer, onMessage, disconnect } from '@/utils/pushClient'
 *   connectPushServer('127.0.0.1', 8890, uid, token)
 *   onMessage('notify', (data) => { console.log(data) })
 */

import { ElMessage } from 'element-plus'

let ws = null
let wsId = 0 // incrementing counter to detect stale closures
let reconnectTimer = null
let reconnectDelay = 0
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
  // If we're already connected with the same uid+token, skip — prevents
  // double-connect that would tear down an authenticated session.
  if (ws && ws.readyState === WebSocket.OPEN && uid === userUid && token === userToken) {
    console.log('[PushClient] connectPushServer skipped — already connected with same uid+token')
    return
  }
  // If a connection is in progress with the same uid+token, skip — let it complete.
  if (ws && ws.readyState === WebSocket.CONNECTING && uid === userUid && token === userToken) {
    console.log('[PushClient] connectPushServer skipped — connection in progress with same uid+token')
    return
  }

  host = serverHost
  port = serverPort
  uid = userUid
  token = userToken
  shouldReconnect = true
  reconnectDelay = 0
  // Kill any pending reconnect from a previous session
  clearTimeout(reconnectTimer)
  reconnectTimer = null

  console.log('[PushClient] connectPushServer uid=', userUid, 'host=', host, 'port=', port)
  doConnect()
}

function doConnect() {
  // Clear any lingering reconnect timer
  clearTimeout(reconnectTimer)
  reconnectTimer = null

  // If shouldReconnect was flipped off (e.g. disconnect() called while a timer
  // was pending), don't connect — let connectPushServer be the only on-ramp.
  if (!shouldReconnect) {
    console.log('[PushClient] doConnect skipped — shouldReconnect is false')
    return
  }

  if (uid <= 0 || !token) {
    console.error('[PushClient] doConnect skipped — invalid uid or token')
    return
  }

  // Close old socket cleanly — null its handlers first so stale onclose can't
  // null out a newer ws reference or trigger a duplicate reconnect.
  if (ws) {
    ws.onclose = null
    ws.onerror = null
    ws.onmessage = null
    ws.onopen = null
    ws.close()
    ws = null
  }

  const currentWsId = ++wsId
  const url = `ws://${host}:${port}`
  console.log('[PushClient] connecting to', url, 'wsId=', currentWsId)
  ws = new WebSocket(url)

  ws.onopen = () => {
    console.log('[PushClient] connected wsId=', currentWsId, 'sending login uid=', uid)
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
      if (handlers.has('*')) {
        handlers.get('*').forEach((fn) => fn(data))
      }
    } catch (e) {
      console.error('[PushClient] parse error:', e)
    }
  }

  ws.onclose = (event) => {
    // Stale closure — a newer WebSocket has replaced this one
    if (wsId !== currentWsId) {
      console.log('[PushClient] stale onclose ignored wsId=', currentWsId, 'current=', wsId)
      return
    }
    console.log('[PushClient] disconnected wsId=', currentWsId, 'code=', event.code, 'reason=', event.reason, 'shouldReconnect=', shouldReconnect)
    ws = null
    if (shouldReconnect) {
      reconnectDelay = reconnectDelay > 0 ? Math.min(reconnectDelay * 2, 30000) : 2000
      console.log('[PushClient] reconnecting in', reconnectDelay, 'ms')
      reconnectTimer = setTimeout(() => {
        // Double-check shouldReconnect before actually reconnecting — disconnect()
        // may have been called between timer set and timer fire.
        if (shouldReconnect) {
          doConnect()
        } else {
          console.log('[PushClient] reconnect cancelled — shouldReconnect is false')
        }
      }, reconnectDelay)
    }
  }

  ws.onerror = (err) => {
    console.error('[PushClient] error wsId=', currentWsId, err)
  }
}

/**
 * Disconnect and stop auto-reconnect.
 */
export function disconnect() {
  shouldReconnect = false
  clearTimeout(reconnectTimer)
  reconnectTimer = null
  token = ''
  uid = 0
  if (ws) {
    // Null handlers so stale close/error/message can't fire callbacks
    ws.onclose = null
    ws.onerror = null
    ws.onmessage = null
    const socket = ws
    ws = null
    socket.close()
    return new Promise((resolve) => {
      const done = () => resolve()
      socket.addEventListener('close', done, { once: true })
      setTimeout(done, 500)
    })
  }
  return Promise.resolve()
}

// Built-in handler for login_rsp — close connection on auth failure.
// Only clears auth state if the rejection is for the current session uid,
// preventing a stale login_rsp from destroying an active session.
onMessage('login_rsp', (data) => {
  if (data.error !== 0 && data.error !== undefined) {
    console.error('[PushClient] login rejected for uid=', data.uid, 'error=', data.error, '(current session uid=', uid, ')')
    // Only act if the rejection uid matches the current session, or if no uid in response
    if (data.uid === undefined || data.uid === uid) {
      disconnect()
      const keys = ['token', 'uid', 'username', 'email', 'role', 'belong_captain_id', 'belong_team_id']
      keys.forEach((k) => localStorage.removeItem(k))
      ElMessage.error('登录已过期，请重新登录')
      window.location.hash = '#/login'
    } else {
      console.warn('[PushClient] login_rsp uid mismatch — ignoring stale rejection')
    }
  }
})

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

// Before unload — close cleanly so server sees TCP close, not RST
window.addEventListener('beforeunload', () => {
  if (ws) {
    shouldReconnect = false
    ws.onclose = null
    ws.close()
    ws = null
  }
})
