import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'
import fs from 'fs'

const configPath = resolve(__dirname, 'config.json')
const serverConfig = JSON.parse(fs.readFileSync(configPath, 'utf-8'))
const gateHost = serverConfig.GateServer?.host || '127.0.0.1'
const gatePort = serverConfig.GateServer?.port || 8080

export default defineConfig({
  plugins: [vue()],
  base: './',
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src'),
    },
  },
  server: {
    port: 5173,
    open: true,
    proxy: {
      '/api': {
        target: `http://${gateHost}:${gatePort}`,
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
      },
    },
  },
  build: {
    outDir: 'dist',
  },
})
