<template>
  <div class="page-container">
    <div class="page-header">
      <h2>个人中心</h2>
      <p>查看个人信息与系统设置</p>
    </div>

    <!-- Profile Hero -->
    <div class="profile-hero">
      <div class="hero-bg"></div>
      <div class="hero-content">
        <div class="hero-avatar">
          <el-avatar :size="72" icon="UserFilled" class="hero-avatar-inner" />
          <div class="hero-role-badge">
            <el-tag :type="roleTagColor" size="small" effect="dark">{{ userStore.roleLabel }}</el-tag>
          </div>
        </div>
        <div class="hero-info">
          <h3 class="hero-name">{{ userStore.username }}</h3>
          <p class="hero-email">{{ userStore.email }}</p>
          <div class="hero-meta">
            <span class="hero-meta-item">
              <el-icon><User /></el-icon> UID: {{ userStore.uid }}
            </span>
            <span class="hero-meta-item">
              <el-icon><OfficeBuilding /></el-icon>
              {{ userStore.belongTeamId > 0 ? '队伍 ' + userStore.belongTeamId : '未分配队伍' }}
            </span>
          </div>
        </div>
      </div>
    </div>

    <!-- Quick Stats -->
    <div class="quick-stats anim-stagger">
      <div class="qs-item">
        <div class="qs-icon qs-icon--primary"><el-icon :size="20"><Grid /></el-icon></div>
        <div class="qs-body">
          <span class="qs-num">{{ taskStats.pending + taskStats.inProgress }}</span>
          <span class="qs-label">任务总数</span>
        </div>
      </div>
      <div class="qs-item">
        <div class="qs-icon qs-icon--warning"><el-icon :size="20"><List /></el-icon></div>
        <div class="qs-body">
          <span class="qs-num">{{ todoStats.active }}</span>
          <span class="qs-label">待办事项</span>
        </div>
      </div>
      <div class="qs-item">
        <div class="qs-icon" :class="checkedInToday ? 'qs-icon--success' : 'qs-icon--info'">
          <el-icon :size="20"><Check /></el-icon>
        </div>
        <div class="qs-body">
          <span class="qs-num">{{ checkedInToday ? '✓' : '—' }}</span>
          <span class="qs-label">今日打卡</span>
        </div>
      </div>
      <div class="qs-item">
        <div class="qs-icon qs-icon--danger"><el-icon :size="20"><Bell /></el-icon></div>
        <div class="qs-body">
          <span class="qs-num">{{ msgUnread }}</span>
          <span class="qs-label">未读消息</span>
        </div>
      </div>
    </div>

    <!-- Detail Cards -->
    <el-row :gutter="20" class="detail-row">
      <!-- Personal Info -->
      <el-col :span="14">
        <el-card shadow="hover" class="detail-card">
          <template #header>
            <div class="dc-header">
              <span class="dc-title">个人信息</span>
            </div>
          </template>
          <div class="info-grid">
            <div class="info-row">
              <span class="info-label">用户名</span>
              <span class="info-value">{{ userStore.username }}</span>
            </div>
            <div class="info-row">
              <span class="info-label">邮箱地址</span>
              <span class="info-value">{{ userStore.email }}</span>
            </div>
            <div class="info-row">
              <span class="info-label">角色身份</span>
              <span class="info-value">
                <el-tag :type="roleTagColor" size="small">{{ userStore.roleLabel }}</el-tag>
              </span>
            </div>
            <div class="info-row">
              <span class="info-label">所属队伍</span>
              <span class="info-value" v-if="!editingTeam">
                {{ userStore.belongTeamId > 0 ? '队伍 ' + userStore.belongTeamId : '未分配' }}
                <el-button v-if="userStore.isCoach" link type="primary" size="small" @click="startEditTeam" style="margin-left:8px">
                  修改
                </el-button>
              </span>
              <div v-else class="team-edit-row">
                <el-input-number v-model="teamForm.teamId" :min="0" size="small" style="width:120px" />
                <el-button type="primary" size="small" @click="saveTeam">保存</el-button>
                <el-button size="small" @click="editingTeam = false">取消</el-button>
              </div>
            </div>
            <div class="info-row">
              <span class="info-label">用户 UID</span>
              <span class="info-value" style="font-family:monospace; letter-spacing:1px">{{ userStore.uid }}</span>
            </div>
          </div>
        </el-card>
      </el-col>

      <!-- Settings -->
      <el-col :span="10">
        <el-card shadow="hover" class="detail-card">
          <template #header>
            <div class="dc-header">
              <span class="dc-title">界面设置</span>
            </div>
          </template>
          <div class="setting-list">
            <div class="setting-row">
              <div class="setting-left">
                <el-icon :size="18" style="color:var(--color-primary)"><Moon /></el-icon>
                <div>
                  <div class="setting-name">深色模式</div>
                  <div class="setting-desc">切换界面明暗主题</div>
                </div>
              </div>
              <el-switch :model-value="appStore.theme === 'dark'" @change="appStore.toggleTheme()" />
            </div>
            <div class="setting-row">
              <div class="setting-left">
                <el-icon :size="18" style="color:var(--color-warning)"><Notification /></el-icon>
                <div>
                  <div class="setting-name">消息通知</div>
                  <div class="setting-desc">实时推送已开启</div>
                </div>
              </div>
              <el-tag type="success" size="small">在线</el-tag>
            </div>
            <div class="setting-row">
              <div class="setting-left">
                <el-icon :size="18" style="color:var(--color-info)"><Connection /></el-icon>
                <div>
                  <div class="setting-name">WebSocket 连接</div>
                  <div class="setting-desc">PushServer 实时通道</div>
                </div>
              </div>
              <el-tag type="success" size="small">已连接</el-tag>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed, reactive, ref, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { useAppStore } from '@/stores/app'
import { listTasks, listTodo } from '@/api/task'
import { getCheckins } from '@/api/checkin'
import { getMessages } from '@/api/message'
import { ElMessage } from 'element-plus'
import { User, Grid, List, Check, Bell, Moon, Notification, Connection, OfficeBuilding } from '@element-plus/icons-vue'

const userStore = useUserStore()
const appStore = useAppStore()

const roleTagColor = computed(() => {
  const map = { 0: 'info', 1: 'success', 2: 'danger' }
  return map[userStore.role] || 'info'
})

// Quick stats
const taskStats = reactive({ pending: 0, inProgress: 0, completed: 0 })
const todoStats = reactive({ active: 0, done: 0 })
const checkedInToday = ref(false)
const msgUnread = ref(0)

// Team editing
const editingTeam = ref(false)
const teamForm = reactive({ teamId: 0 })

function startEditTeam() {
  teamForm.teamId = userStore.belongTeamId
  editingTeam.value = true
}

async function saveTeam() {
  try {
    await userStore.updateTeam(userStore.uid, teamForm.teamId)
    ElMessage.success('所属队伍已更新')
    editingTeam.value = false
  } catch {
    ElMessage.error('更新失败，请稍后重试')
  }
}

onMounted(async () => {
  const uid = parseInt(userStore.uid) || 0

  // Tasks
  try {
    const res = await listTasks({ uid, status: -1, assigned_to: '0' })
    if (res.error === 0 && res.tasks) {
      for (const t of res.tasks) {
        const uids = String(t.assigned_to || '0').split(',').map(Number)
        if (!uids.includes(uid)) continue
        const s = t.my_status ?? t.status
        if (s === 0) taskStats.pending++
        else if (s === 1) taskStats.inProgress++
        else if (s === 2) taskStats.completed++
      }
    }
  } catch {}

  // TODOs
  try {
    const res = await listTodo({ uid, is_finished: 0 })
    if (res.error === 0 && res.todos) {
      for (const t of res.todos) {
        if (t.is_finished === 1) todoStats.done++; else todoStats.active++
      }
    }
  } catch {}

  // Checkin
  try {
    const now = new Date()
    const ds = `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}-${String(now.getDate()).padStart(2,'0')}`
    const res = await getCheckins({ uid, date_from: ds, date_to: ds })
    if (res.error === 0 && res.records?.length > 0) checkedInToday.value = true
  } catch {}

  // Messages
  try {
    const res = await getMessages({ uid, page: 1, page_size: 1 })
    if (res.error === 0) msgUnread.value = res.unread_count || 0
  } catch {}
})
</script>

<style scoped>
/* Profile Hero */
.profile-hero {
  position: relative;
  border-radius: var(--radius-lg);
  overflow: hidden;
  margin-bottom: 24px;
  background: var(--gradient-primary);
  padding: 36px 32px;
}
.hero-bg {
  position: absolute;
  inset: 0;
  background:
    radial-gradient(ellipse at 80% 20%, rgba(255,255,255,0.12) 0%, transparent 50%),
    radial-gradient(ellipse at 20% 80%, rgba(255,255,255,0.06) 0%, transparent 50%);
  pointer-events: none;
}
.hero-content {
  display: flex;
  align-items: center;
  gap: 24px;
  position: relative;
  z-index: 1;
}
.hero-avatar { position: relative; }
.hero-avatar-inner {
  border: 3px solid rgba(255,255,255,0.3);
  box-shadow: 0 4px 16px rgba(0,0,0,0.2);
}
.hero-role-badge {
  position: absolute;
  bottom: -4px;
  right: -4px;
}
.hero-info { color: #fff; }
.hero-name {
  font-size: 22px;
  font-weight: 700;
  margin-bottom: 4px;
  letter-spacing: -0.3px;
}
.hero-email {
  font-size: 13px;
  opacity: 0.8;
  margin-bottom: 12px;
}
.hero-meta {
  display: flex;
  gap: 20px;
}
.hero-meta-item {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  opacity: 0.7;
}

/* Quick Stats */
.quick-stats {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 16px;
  margin-bottom: 24px;
}
.qs-item {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 18px 20px;
  background: var(--bg-secondary);
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  transition: all var(--transition-fast);
}
.qs-item:hover {
  transform: translateY(-2px);
  box-shadow: var(--shadow-md);
}
.qs-icon {
  width: 44px;
  height: 44px;
  border-radius: var(--radius-md);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  color: #fff;
}
.qs-icon--primary { background: var(--gradient-primary); }
.qs-icon--success { background: var(--gradient-success); }
.qs-icon--warning { background: var(--gradient-warning); }
.qs-icon--danger { background: var(--gradient-danger); }
.qs-icon--info { background: linear-gradient(135deg, #94a3b8, #64748b); }
.qs-body { display: flex; flex-direction: column; }
.qs-num { font-size: 22px; font-weight: 700; line-height: 1.2; color: var(--text-primary); }
.qs-label { font-size: 12px; color: var(--text-secondary); margin-top: 2px; }

/* Detail Cards */
.detail-row { margin-bottom: 20px; }
.dc-header { display: flex; align-items: center; }
.dc-title { font-weight: 600; font-size: 14px; }

.info-grid { display: flex; flex-direction: column; }
.info-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 14px 8px;
  border-bottom: 1px solid var(--border-light);
  transition: all var(--transition-fast);
  border-radius: var(--radius-sm);
}
.info-row:last-child { border-bottom: none; }
.info-row:hover { background: var(--color-primary-bg); margin: 0 -8px; padding-left: 16px; padding-right: 16px; }
.info-label { color: var(--text-secondary); font-size: 13px; font-weight: 500; }
.info-value { color: var(--text-primary); font-weight: 600; font-size: 14px; }

.team-edit-row { display: flex; align-items: center; gap: 6px; }

.setting-list { display: flex; flex-direction: column; }
.setting-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 8px;
  border-bottom: 1px solid var(--border-light);
  transition: all var(--transition-fast);
  border-radius: var(--radius-sm);
}
.setting-row:last-child { border-bottom: none; }
.setting-row:hover { background: var(--color-primary-bg); margin: 0 -8px; padding-left: 16px; padding-right: 16px; }
.setting-left { display: flex; align-items: center; gap: 12px; }
.setting-name { font-size: 14px; font-weight: 500; color: var(--text-primary); }
.setting-desc { font-size: 12px; color: var(--text-secondary); margin-top: 2px; }
</style>
