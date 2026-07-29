<template>
  <div class="main-layout" :class="{ 'sidebar-collapsed': appStore.sidebarCollapsed }">
    <!-- Sidebar -->
    <aside class="sidebar">
      <div class="sidebar-logo">
        <span v-if="!appStore.sidebarCollapsed" class="logo-text">OxyTeamTask</span>
        <span v-else class="logo-icon">&gt;_</span>
      </div>

      <el-menu
        :default-active="activeMenu"
        :collapse="appStore.sidebarCollapsed"
        :collapse-transition="false"
        background-color="transparent"
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
          <div class="menu-divider"></div>
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
          <div class="menu-divider"></div>
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
              <el-avatar :size="28" icon="UserFilled" />
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
    if (!isConnected() && userStore.pushServerHost && userStore.pushServerPort) {
      connectPushServer(userStore.pushServerHost, userStore.pushServerPort, userStore.uid, userStore.token)
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
  border-right: 1px solid var(--border-default);
  display: flex;
  flex-direction: column;
  transition: width var(--duration-normal) var(--ease-default),
              min-width var(--duration-normal) var(--ease-default);
  overflow: hidden;
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
  border-bottom: 1px solid var(--border-default);
}

.logo-text {
  font-family: var(--font-mono);
  font-size: var(--text-md);
  font-weight: 700;
  color: var(--text-sidebar-active);
  letter-spacing: var(--tracking-tight);
}

.logo-icon {
  font-family: var(--font-mono);
  font-size: var(--text-lg);
  font-weight: 700;
  color: var(--color-primary);
}

.menu-divider {
  height: 1px;
  background: var(--border-default);
  margin: var(--space-2) var(--space-4);
}

.menu-group-title {
  padding: var(--space-3) var(--space-5) var(--space-1);
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  color: var(--text-tertiary);
  text-transform: uppercase;
  letter-spacing: var(--tracking-wider);
  font-weight: 600;
}

.sidebar :deep(.el-menu) {
  border-right: none;
  flex: 1;
  overflow-y: auto;
  overflow-x: hidden;
  padding: var(--space-2) 0;
}

.sidebar :deep(.el-menu-item) {
  height: 36px;
  line-height: 36px;
  margin: 1px var(--space-2);
  border-radius: var(--radius-sm);
  font-size: var(--text-base);
  font-weight: 500;
  transition: all var(--duration-fast) var(--ease-default);
}

.sidebar :deep(.el-menu-item:hover) {
  background: var(--bg-sidebar-hover) !important;
}

.sidebar :deep(.el-menu-item.is-active) {
  background: var(--bg-sidebar-active) !important;
  color: var(--text-sidebar-active) !important;
  font-weight: 600;
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
  background: var(--bg-surface);
  border-bottom: 1px solid var(--border-default);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 var(--space-5);
  position: sticky;
  top: 0;
  z-index: var(--z-sticky);
}

.header-left {
  display: flex;
  align-items: center;
  gap: var(--space-3);
}

.header-right {
  display: flex;
  align-items: center;
  gap: var(--space-3);
}

.collapse-btn {
  font-size: var(--text-lg);
  color: var(--text-secondary);
  transition: color var(--duration-fast) var(--ease-default);
}
.collapse-btn:hover { color: var(--color-primary); }

.notification-badge { margin-right: var(--space-1); }

/* Message panel */
.msg-bell-wrapper { position: relative; }
.msg-panel {
  position: absolute;
  top: calc(100% + var(--space-2));
  right: 0;
  width: 340px;
  background: var(--bg-surface);
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-lg);
  z-index: var(--z-dropdown);
  overflow: hidden;
}
.msg-panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--space-3) var(--space-4);
  border-bottom: 1px solid var(--border-default);
  font-family: var(--font-mono);
  font-weight: 600;
  font-size: var(--text-base);
  color: var(--text-primary);
}
.msg-panel-list { max-height: 360px; overflow-y: auto; }
.msg-panel-empty {
  padding: var(--space-8) 0;
  text-align: center;
  color: var(--text-tertiary);
  font-size: var(--text-sm);
  font-family: var(--font-mono);
}
.msg-panel-item {
  padding: var(--space-3) var(--space-4);
  border-bottom: 1px solid var(--border-muted);
  cursor: pointer;
  transition: background var(--duration-fast) var(--ease-default);
  position: relative;
}
.msg-panel-item:hover { background: var(--color-primary-bg); }
.msg-panel-item:last-child { border-bottom: none; }
.msg-panel-item.unread { background: var(--color-primary-bg); }
.msg-panel-item.unread::before {
  content: '';
  position: absolute;
  left: var(--space-2);
  top: 50%;
  transform: translateY(-50%);
  width: 5px;
  height: 5px;
  background: var(--color-primary);
}
.msg-panel-title {
  font-size: var(--text-sm);
  color: var(--text-primary);
  margin-bottom: 2px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  padding-left: var(--space-3);
}
.msg-panel-item.unread .msg-panel-title { font-weight: 600; }
.msg-panel-time {
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  color: var(--text-tertiary);
  padding-left: var(--space-3);
}
.msg-panel-footer {
  padding: var(--space-3) 0;
  text-align: center;
  font-family: var(--font-mono);
  font-size: var(--text-sm);
  font-weight: 500;
  color: var(--color-primary);
  cursor: pointer;
  border-top: 1px solid var(--border-default);
  transition: background var(--duration-fast) var(--ease-default);
}
.msg-panel-footer:hover { background: var(--color-primary-bg); }

.user-info {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  cursor: pointer;
  padding: var(--space-1) var(--space-2);
  border-radius: var(--radius-sm);
  transition: background var(--duration-fast) var(--ease-default);
}
.user-info:hover { background: var(--color-primary-bg); }

.username {
  font-family: var(--font-mono);
  font-size: var(--text-sm);
  color: var(--text-primary);
  max-width: 100px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-weight: 500;
}

.arrow {
  font-size: var(--text-xs);
  color: var(--text-tertiary);
}

/* ===== Content ===== */
.content {
  flex: 1;
  overflow-y: auto;
  background: var(--bg-base);
}
</style>
