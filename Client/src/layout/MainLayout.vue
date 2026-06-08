<template>
  <div class="main-layout" :class="{ 'sidebar-collapsed': appStore.sidebarCollapsed }">
    <!-- Sidebar -->
    <aside class="sidebar">
      <div class="sidebar-logo">
        <div class="logo-glow"></div>
        <span v-if="!appStore.sidebarCollapsed" class="logo-text">OxyTeamTask</span>
        <span v-else class="logo-icon">OT</span>
      </div>

      <el-menu
        :default-active="activeMenu"
        :collapse="appStore.sidebarCollapsed"
        :collapse-transition="false"
        background-color="var(--bg-sidebar)"
        text-color="var(--text-sidebar)"
        active-text-color="var(--text-sidebar-active)"
        router
      >
        <el-menu-item index="/dashboard">
          <el-icon><HomeFilled /></el-icon>
          <span>工作台</span>
        </el-menu-item>
        <el-menu-item index="/taskboard">
          <el-icon><Grid /></el-icon>
          <span>任务看板</span>
        </el-menu-item>
        <el-menu-item index="/todolist">
          <el-icon><List /></el-icon>
          <span>TODO List</span>
        </el-menu-item>
        <el-menu-item index="/checkin">
          <el-icon><Check /></el-icon>
          <span>每日打卡</span>
        </el-menu-item>
        <el-menu-item index="/messages">
          <el-icon><Bell /></el-icon>
          <span>消息中心</span>
        </el-menu-item>

        <template v-if="userStore.canManage">
          <el-divider style="margin: 8px 0; border-color: rgba(255,255,255,0.08)" />
          <div v-if="!appStore.sidebarCollapsed" class="menu-group-title">管理</div>
          <el-menu-item index="/manage/tasks">
            <el-icon><EditPen /></el-icon>
            <span>任务管理</span>
          </el-menu-item>
          <el-menu-item index="/manage/team">
            <el-icon><DataLine /></el-icon>
            <span>队伍信息</span>
          </el-menu-item>
        </template>

        <template v-if="userStore.isCoach">
          <el-divider style="margin: 8px 0; border-color: rgba(255,255,255,0.08)" />
          <div v-if="!appStore.sidebarCollapsed" class="menu-group-title">教练</div>
          <el-menu-item index="/manage/allteams">
            <el-icon><DataBoard /></el-icon>
            <span>全队信息</span>
          </el-menu-item>
          <el-menu-item index="/manage/users">
            <el-icon><User /></el-icon>
            <span>权限管理</span>
          </el-menu-item>
          <el-menu-item index="/manage/monitor">
            <el-icon><Monitor /></el-icon>
            <span>系统监控</span>
          </el-menu-item>
        </template>
      </el-menu>
    </aside>

    <!-- Main Content Area -->
    <div class="main-area">
      <!-- Header -->
      <header class="header">
        <div class="header-left">
          <el-button
            class="collapse-btn"
            :icon="appStore.sidebarCollapsed ? Expand : Fold"
            text
            @click="appStore.toggleSidebar()"
          />
          <el-breadcrumb separator="/">
            <el-breadcrumb-item :to="{ path: '/dashboard' }">首页</el-breadcrumb-item>
            <el-breadcrumb-item v-if="pageTitle">{{ pageTitle }}</el-breadcrumb-item>
          </el-breadcrumb>
        </div>

        <div class="header-right">
          <el-switch
            v-model="isDark"
            :active-action-icon="Moon"
            :inactive-action-icon="Sunny"
            inline-prompt
            size="small"
            @change="appStore.toggleTheme()"
          />

          <div class="msg-bell-wrapper">
            <el-badge :value="unreadCount" :max="99" :hidden="unreadCount === 0" class="notification-badge">
              <el-button icon="Bell" circle text @click="toggleMsgPanel" />
            </el-badge>
            <!-- Message dropdown panel -->
            <div v-if="msgPanelVisible" class="msg-panel" @click.stop>
              <div class="msg-panel-header">
                <span>消息通知</span>
                <el-button v-if="unreadCount > 0" link size="small" type="primary" @click="markAllRead">全部已读</el-button>
              </div>
              <div class="msg-panel-list">
                <div v-if="recentMessages.length === 0" class="msg-panel-empty">暂无消息</div>
                <div v-for="msg in recentMessages" :key="msg.id" class="msg-panel-item" :class="{ unread: msg.is_read === 0 }">
                  <div class="msg-panel-title">{{ msg.title }}</div>
                  <div class="msg-panel-time">{{ msg.created_at }}</div>
                </div>
              </div>
              <div class="msg-panel-footer" @click="goToMessages">查看全部消息</div>
            </div>
          </div>

          <el-dropdown trigger="click" @command="handleUserCommand">
            <span class="user-info">
              <el-avatar :size="32" icon="UserFilled" />
              <span class="username">{{ userStore.username || '未登录' }}</span>
              <el-icon class="arrow"><ArrowDown /></el-icon>
            </span>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item command="profile">
                  <el-icon><User /></el-icon> 个人中心
                </el-dropdown-item>
                <el-dropdown-item command="logout" divided>
                  <el-icon><SwitchButton /></el-icon> 退出登录
                </el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </div>
      </header>

      <!-- Page Content -->
      <main class="content">
        <router-view v-slot="{ Component, route }">
          <transition name="slide-fade" mode="out-in">
            <component :is="Component" :key="route.path" />
          </transition>
        </router-view>
      </main>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted, onUnmounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useUserStore } from '@/stores/user'
import { useAppStore } from '@/stores/app'
import { getMessages, markRead } from '@/api/message'
import { onMessage as onWsMessage, offMessage, connectPushServer, isConnected } from '@/utils/pushClient'
import { ElNotification } from 'element-plus'
import {
  HomeFilled, Grid, List, Check, Bell, EditPen, DataLine, DataBoard,
  User, Monitor, Expand, Fold, Moon, Sunny, ArrowDown, SwitchButton,
} from '@element-plus/icons-vue'

const route = useRoute()
const router = useRouter()
const userStore = useUserStore()
const appStore = useAppStore()

// Message state
const unreadCount = ref(0)
const recentMessages = ref([])
const msgPanelVisible = ref(false)

function toggleMsgPanel() {
  msgPanelVisible.value = !msgPanelVisible.value
  if (msgPanelVisible.value) loadRecentMessages()
}

function closeMsgPanel() { msgPanelVisible.value = false }

function goToMessages() {
  msgPanelVisible.value = false
  router.push('/messages')
}

async function loadRecentMessages() {
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await getMessages({ uid, page: 1, page_size: 10 })
    if (res.error === 0) {
      recentMessages.value = res.messages || []
      unreadCount.value = res.unread_count || 0
    }
  } catch (e) { /* ignore */ }
}

async function markAllRead() {
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await markRead({ uid, ids: [] })
    if (res.error === 0) {
      recentMessages.value.forEach(m => m.is_read = 1)
      unreadCount.value = 0
    }
  } catch (e) { /* ignore */ }
}

// WebSocket message handlers
function handleNotify(data) {
  unreadCount.value++
  recentMessages.value.unshift({
    id: Date.now(), type: data.msg_type || 'notify',
    title: data.title || '新消息', is_read: 0,
    created_at: new Date().toLocaleString('zh-CN')
  })
  if (recentMessages.value.length > 10) recentMessages.value.pop()
  ElNotification({ title: data.title || '新消息', type: 'info', duration: 4000 })
}

function handleKicked(data) {
  userStore.logout()
  ElNotification({ title: '账号下线', message: '您的账号已在其他设备登录', type: 'warning', duration: 0 })
  router.push('/login')
}

function handleLoginRsp(data) {
  if (data.unread_count > 0) unreadCount.value = data.unread_count
  if (data.offline_messages && data.offline_messages.length > 0) {
    for (const msg of data.offline_messages) {
      recentMessages.value.push({
        id: Date.now() + Math.random(), type: msg.msg_type || 'notify',
        title: msg.title || '离线消息', is_read: 0,
        created_at: msg.created_at || ''
      })
    }
  }
}

// Close panel on outside click
function onDocClick(e) {
  if (msgPanelVisible.value && !e.target.closest('.msg-bell-wrapper')) {
    msgPanelVisible.value = false
  }
}

onMounted(() => {
  document.addEventListener('click', onDocClick)
  onWsMessage('notify', handleNotify)
  onWsMessage('kicked', handleKicked)
  onWsMessage('login_rsp', handleLoginRsp)
  if (userStore.uid) {
    loadRecentMessages()
    // 刷新页面后重连 WebSocket（login() 只在首次登录时调用）
    console.log('[MainLayout] onMounted: uid=', userStore.uid, 'host=', userStore.pushServerHost, 'port=', userStore.pushServerPort, 'connected=', isConnected())
    if (!isConnected() && userStore.pushServerHost && userStore.pushServerPort) {
      console.log('[MainLayout] calling connectPushServer')
      connectPushServer(userStore.pushServerHost, userStore.pushServerPort, userStore.uid, userStore.token)
    } else {
      console.log('[MainLayout] skipping reconnect: connected=', isConnected(), 'host=', userStore.pushServerHost, 'port=', userStore.pushServerPort)
    }
  }
})

onUnmounted(() => {
  document.removeEventListener('click', onDocClick)
  offMessage('notify', handleNotify)
  offMessage('kicked', handleKicked)
  offMessage('login_rsp', handleLoginRsp)
})

const activeMenu = computed(() => route.path)
const pageTitle = computed(() => route.meta?.title || '')
const isDark = computed({
  get: () => appStore.theme === 'dark',
  set: () => {},
})

// Update page title on route change
watch(pageTitle, (title) => {
  appStore.setPageTitle(title)
}, { immediate: true })

function handleUserCommand(cmd) {
  if (cmd === 'profile') {
    router.push('/profile')
  } else if (cmd === 'logout') {
    userStore.logout()
  }
}
</script>

<style scoped>
.main-layout {
  display: flex;
  height: 100vh;
  overflow: hidden;
}

/* ===== Sidebar ===== */
.sidebar {
  width: var(--sidebar-width);
  min-width: var(--sidebar-width);
  background: var(--bg-sidebar);
  display: flex;
  flex-direction: column;
  transition: width var(--transition-normal), min-width var(--transition-normal);
  overflow: hidden;
  position: relative;
}

.sidebar::before {
  content: '';
  position: absolute;
  top: 0;
  right: 0;
  width: 1px;
  height: 100%;
  background: linear-gradient(to bottom, rgba(79, 110, 247, 0.25), rgba(124, 92, 252, 0.08), transparent);
  z-index: 1;
}

.sidebar::after {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 120px;
  background: linear-gradient(to top, rgba(79, 110, 247, 0.04), transparent);
  pointer-events: none;
}

.sidebar-collapsed .sidebar {
  width: var(--sidebar-collapsed-width);
  min-width: var(--sidebar-collapsed-width);
}

.sidebar-logo {
  height: var(--header-height);
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
  overflow: hidden;
  border-bottom: 1px solid rgba(255, 255, 255, 0.04);
}

.logo-glow {
  position: absolute;
  top: -30px;
  left: 50%;
  transform: translateX(-50%);
  width: 160px;
  height: 80px;
  background: radial-gradient(ellipse, rgba(79, 110, 247, 0.15), transparent 70%);
  pointer-events: none;
  animation: logoPulse 4s ease-in-out infinite;
}

@keyframes logoPulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

.logo-text {
  font-size: 17px;
  font-weight: 700;
  color: #fff;
  letter-spacing: 0.5px;
  position: relative;
  z-index: 1;
  background: linear-gradient(135deg, #fff 0%, rgba(255, 255, 255, 0.8) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.logo-icon {
  font-size: 20px;
  font-weight: 800;
  background: var(--gradient-primary);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.menu-group-title {
  padding: 16px 20px 6px;
  font-size: 10px;
  color: rgba(255, 255, 255, 0.2);
  text-transform: uppercase;
  letter-spacing: 2px;
  font-weight: 700;
}

.sidebar :deep(.el-menu) {
  border-right: none;
  flex: 1;
  overflow-y: auto;
  overflow-x: hidden;
  padding: 6px 0;
}

.sidebar :deep(.el-menu-item) {
  height: 44px;
  line-height: 44px;
  margin: 2px 12px;
  border-radius: var(--radius-sm);
  font-size: 13px;
  font-weight: 500;
  letter-spacing: 0.2px;
  transition: all var(--transition-fast);
}

.sidebar :deep(.el-menu-item:hover) {
  background: var(--bg-sidebar-hover) !important;
}

.sidebar :deep(.el-menu-item.is-active) {
  background: var(--bg-sidebar-active) !important;
  box-shadow: 0 2px 12px rgba(79, 110, 247, 0.25);
  font-weight: 600;
}

.sidebar :deep(.el-divider--horizontal) {
  margin: 8px 16px;
  border-color: rgba(255, 255, 255, 0.04);
}

/* ===== Main Area ===== */
.main-area {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
}

/* ===== Header ===== */
.header {
  height: var(--header-height);
  min-height: var(--header-height);
  background: var(--glass-bg);
  backdrop-filter: var(--glass-blur);
  -webkit-backdrop-filter: var(--glass-blur);
  border-bottom: 1px solid var(--border-light);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 28px;
  position: sticky;
  top: 0;
  z-index: 10;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 14px;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 14px;
}

.collapse-btn {
  font-size: 18px;
  color: var(--text-secondary);
  transition: color var(--transition-fast);
}
.collapse-btn:hover { color: var(--color-primary); }

.notification-badge { margin-right: 4px; }

/* Message panel */
.msg-bell-wrapper { position: relative; }
.msg-panel {
  position: absolute;
  top: calc(100% + 10px);
  right: 0;
  width: 360px;
  background: var(--bg-secondary);
  border: 1px solid var(--border-light);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-xl);
  z-index: 1000;
  overflow: hidden;
  animation: panelSlide 0.2s cubic-bezier(0.4, 0, 0.2, 1);
}

@keyframes panelSlide {
  from { opacity: 0; transform: translateY(-8px); }
  to { opacity: 1; transform: translateY(0); }
}

.msg-panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 14px 20px;
  border-bottom: 1px solid var(--border-light);
  font-weight: 600;
  font-size: 14px;
  background: var(--gradient-primary-soft);
}
.msg-panel-list { max-height: 380px; overflow-y: auto; }
.msg-panel-empty {
  padding: 40px 0;
  text-align: center;
  color: var(--text-secondary);
  font-size: 13px;
}
.msg-panel-item {
  padding: 12px 20px;
  border-bottom: 1px solid var(--border-light);
  cursor: pointer;
  transition: all var(--transition-fast);
  position: relative;
}
.msg-panel-item:hover { background: var(--color-primary-bg); }
.msg-panel-item:last-child { border-bottom: none; }
.msg-panel-item.unread { background: var(--color-primary-bg); }
.msg-panel-item.unread::before {
  content: '';
  position: absolute;
  left: 8px;
  top: 50%;
  transform: translateY(-50%);
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--color-primary);
}
.msg-panel-title {
  font-size: 13px;
  color: var(--text-primary);
  margin-bottom: 4px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  padding-left: 8px;
}
.msg-panel-item.unread .msg-panel-title { font-weight: 600; }
.msg-panel-time { font-size: 11px; color: var(--text-secondary); padding-left: 8px; }
.msg-panel-footer {
  padding: 12px 0;
  text-align: center;
  font-size: 13px;
  font-weight: 500;
  color: var(--color-primary);
  cursor: pointer;
  border-top: 1px solid var(--border-light);
  transition: all var(--transition-fast);
}
.msg-panel-footer:hover { background: var(--color-primary-bg); }

.user-info {
  display: flex;
  align-items: center;
  gap: 10px;
  cursor: pointer;
  padding: 6px 12px;
  border-radius: var(--radius-md);
  transition: all var(--transition-fast);
}
.user-info:hover { background: var(--color-primary-bg); }

.username {
  font-size: 13px;
  color: var(--text-primary);
  max-width: 110px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-weight: 500;
}

.arrow {
  font-size: 12px;
  color: var(--text-secondary);
  transition: transform var(--transition-fast);
}

/* ===== Content ===== */
.content {
  flex: 1;
  overflow-y: auto;
  background: var(--bg-primary);
  background-image: var(--gradient-mesh);
  background-attachment: fixed;
}
</style>
