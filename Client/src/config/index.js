import serverConfig from '../../config.json'

const SERVER_HOST = serverConfig.GateServer?.host || '127.0.0.1'
const SERVER_PORT = serverConfig.GateServer?.port || 8080

// During development, all requests go through the Vite proxy at /api
// to avoid CORS issues (GateServer doesn't set CORS headers).
// In production (Electron), use the direct server URL.
const isDev = import.meta.env.DEV
export const API_BASE_URL = isDev ? '/api' : `http://${SERVER_HOST}:${SERVER_PORT}`

export default {
  SERVER_HOST,
  SERVER_PORT,
  API_BASE_URL,
}
