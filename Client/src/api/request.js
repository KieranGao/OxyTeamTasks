import axios from 'axios'
import { API_BASE_URL } from '@/config'
import { ElMessage } from 'element-plus'

const request = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
})

// Request interceptor — attach auth token
request.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('token')
    const uid = localStorage.getItem('uid')
    if (token) {
      config.headers.Authorization = `Bearer ${token}`
    }
    if (uid) {
      config.headers['X-User-Id'] = uid
    }
    return config
  },
  (error) => Promise.reject(error),
)

// Response interceptor — handle 401 and common errors
request.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.response) {
      const { status } = error.response
      if (status === 401) {
        localStorage.removeItem('token')
        localStorage.removeItem('uid')
        localStorage.removeItem('username')
        localStorage.removeItem('email')
        localStorage.removeItem('role')
        window.location.hash = '#/login'
        ElMessage.error('登录已过期，请重新登录')
      } else {
        ElMessage.error(`请求失败 (${status})`)
      }
    } else if (error.code === 'ECONNABORTED') {
      ElMessage.error('请求超时，请检查网络连接')
    } else {
      ElMessage.error('网络连接失败，请检查服务器状态')
    }
    return Promise.reject(error)
  },
)

export default request
