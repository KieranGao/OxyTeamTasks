import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { loginUser, registerUser as apiRegister, resetPassword as apiResetPassword, updateTeamInfo as apiUpdateTeam } from '@/api/user'

// Lazy-load pushClient to avoid blocking app init
let pushClient = null
async function getPushClient() {
  if (!pushClient) {
    pushClient = await import('@/utils/pushClient')
  }
  return pushClient
}

// role: 0 = 队员, 1 = 队长, 2 = 教练 (matches database)
const ROLE_LABELS = { 0: '队员', 1: '队长', 2: '教练' }
const ROLE_KEYS = { 0: 'member', 1: 'captain', 2: 'coach' }

export const useUserStore = defineStore('user', () => {
  // --- State ---
  const token = ref(localStorage.getItem('token') || '')
  const uid = ref(Number(localStorage.getItem('uid')) || 0)
  const username = ref(localStorage.getItem('username') || '')
  const email = ref(localStorage.getItem('email') || '')
  const role = ref(Number(localStorage.getItem('role')) || 0)
  const belongCaptainId = ref(Number(localStorage.getItem('belong_captain_id')) || 0)
  const belongTeamId = ref(Number(localStorage.getItem('belong_team_id')) || 0)
  const pushServerHost = ref(localStorage.getItem('pushServerHost') || '')
  const pushServerPort = ref(localStorage.getItem('pushServerPort') || '')

  // --- Getters ---
  const isLoggedIn = computed(() => !!token.value)
  const roleLabel = computed(() => ROLE_LABELS[role.value] || '队员')
  const roleKey = computed(() => ROLE_KEYS[role.value] || 'member')
  const isCaptain = computed(() => role.value === 1)
  const isCoach = computed(() => role.value === 2)
  const canManage = computed(() => isCaptain.value || isCoach.value)
  const hasTeam = computed(() => belongTeamId.value > 0)

  // --- Helpers ---
  function persist(key, value) {
    if (value || value === 0) {
      localStorage.setItem(key, String(value))
    } else {
      localStorage.removeItem(key)
    }
  }

  function clearAuth() {
    token.value = ''
    uid.value = 0
    username.value = ''
    email.value = ''
    role.value = 0
    belongCaptainId.value = 0
    belongTeamId.value = 0
    pushServerHost.value = ''
    pushServerPort.value = ''
    localStorage.removeItem('token')
    localStorage.removeItem('uid')
    localStorage.removeItem('username')
    localStorage.removeItem('email')
    localStorage.removeItem('role')
    localStorage.removeItem('belong_captain_id')
    localStorage.removeItem('belong_team_id')
    localStorage.removeItem('pushServerHost')
    localStorage.removeItem('pushServerPort')
  }

  // --- Actions ---
  async function login({ email: loginEmail, password }) {
    const res = await loginUser({ email: loginEmail, password })
    if (res.error !== 0) {
      throw new Error(res.error)
    }
    token.value = res.token
    uid.value = res.uid
    username.value = res.username
    email.value = res.email
    role.value = res.role ?? 0
    belongCaptainId.value = res.belong_captain_id ?? 0
    belongTeamId.value = res.belong_team_id ?? 0
    pushServerHost.value = res.host || ''
    pushServerPort.value = res.port || ''

    persist('token', res.token)
    persist('uid', res.uid)
    persist('username', res.username)
    persist('email', res.email)
    persist('role', res.role ?? 0)
    persist('belong_captain_id', res.belong_captain_id ?? 0)
    persist('belong_team_id', res.belong_team_id ?? 0)
    persist('pushServerHost', res.host || '')
    persist('pushServerPort', res.port || '')

    // Auto-connect WebSocket to PushServer after login
    if (pushServerHost.value && pushServerPort.value) {
      try {
        const pc = await getPushClient()
        pc.connectPushServer(pushServerHost.value, pushServerPort.value, uid.value, token.value)
      } catch (e) {
        console.error('[UserStore] Failed to connect PushServer:', e)
      }
    }

    return res
  }

  async function register(data) {
    const res = await apiRegister(data)
    if (res.error !== 0) {
      throw new Error(res.error)
    }
    return res
  }

  async function resetPassword(data) {
    const res = await apiResetPassword(data)
    if (res.error !== 0) {
      throw new Error(res.error)
    }
    return res
  }

  async function updateTeam(targetUid, teamId) {
    const res = await apiUpdateTeam({ uid: targetUid, belong_team_id: teamId })
    if (res.error !== 0) {
      throw new Error(res.error)
    }
    if (targetUid === uid.value) {
      belongTeamId.value = teamId
      persist('belong_team_id', teamId)
    }
    return res
  }

  async function logout() {
    if (pushClient) {
      await pushClient.disconnect()
    }
    clearAuth()
    window.location.hash = '#/login'
  }

  return {
    token,
    uid,
    username,
    email,
    role,
    belongCaptainId,
    belongTeamId,
    pushServerHost,
    pushServerPort,
    isLoggedIn,
    roleLabel,
    roleKey,
    isCaptain,
    isCoach,
    canManage,
    hasTeam,
    login,
    register,
    resetPassword,
    updateTeam,
    logout,
    clearAuth,
  }
})
