<template>
  <div class="main-layout" :class="{ 'sidebar-collapsed': appStore.sidebarCollapsed }">
    <!-- Sidebar -->
    <aside class="sidebar">
      <div class="sidebar-logo">
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

          <el-badge :value="0" :max="99" class="notification-badge">
            <el-button icon="Bell" circle text />
          </el-badge>

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
import { computed, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useUserStore } from '@/stores/user'
import { useAppStore } from '@/stores/app'
import {
  HomeFilled, Grid, List, Check, Bell, EditPen, DataLine, DataBoard,
  User, Monitor, Expand, Fold, Moon, Sunny, ArrowDown, SwitchButton,
} from '@element-plus/icons-vue'

const route = useRoute()
const router = useRouter()
const userStore = useUserStore()
const appStore = useAppStore()

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
  border-bottom: 1px solid rgba(255, 255, 255, 0.06);
}

.logo-text {
  font-size: 16px;
  font-weight: 700;
  color: #fff;
  letter-spacing: 1px;
}

.logo-icon {
  font-size: 18px;
  font-weight: 700;
  color: var(--color-primary-light);
}

.menu-group-title {
  padding: 8px 20px 4px;
  font-size: 11px;
  color: rgba(255, 255, 255, 0.3);
  text-transform: uppercase;
  letter-spacing: 1px;
}

/* Override Element Plus menu styles within sidebar */
.sidebar :deep(.el-menu) {
  border-right: none;
  flex: 1;
  overflow-y: auto;
  overflow-x: hidden;
}

.sidebar :deep(.el-menu-item) {
  height: 44px;
  line-height: 44px;
  margin: 2px 8px;
  border-radius: var(--radius-sm);
}

.sidebar :deep(.el-menu-item.is-active) {
  background-color: var(--bg-sidebar-active) !important;
}

.sidebar :deep(.el-divider--horizontal) {
  margin: 8px 12px;
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
  background: var(--bg-secondary);
  border-bottom: 1px solid var(--border-light);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 20px;
  box-shadow: var(--shadow-sm);
}

.header-left {
  display: flex;
  align-items: center;
  gap: 12px;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 12px;
}

.collapse-btn {
  font-size: 18px;
}

.notification-badge {
  margin-right: 4px;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 4px 8px;
  border-radius: var(--radius-sm);
  transition: background var(--transition-fast);
}

.user-info:hover {
  background: var(--bg-primary);
}

.username {
  font-size: 13px;
  color: var(--text-primary);
  max-width: 100px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.arrow {
  font-size: 12px;
  color: var(--text-secondary);
}

/* ===== Content ===== */
.content {
  flex: 1;
  overflow-y: auto;
  background: var(--bg-primary);
}
</style>
