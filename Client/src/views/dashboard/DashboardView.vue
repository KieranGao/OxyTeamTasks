<template>
  <div class="page-container">
    <!-- Welcome Banner -->
    <div class="welcome-banner">
      <div class="welcome-content">
        <div class="welcome-text">
          <h2 class="welcome-title">{{ greetingText }}，{{ userStore.username }}</h2>
          <p class="welcome-sub">今天是 {{ todayStr }}，{{ motivationalText }}</p>
        </div>
        <div class="welcome-stats">
          <div class="welcome-stat">
            <span class="ws-num">{{ taskStats.pending + taskStats.inProgress }}</span>
            <span class="ws-label">待完成任务</span>
          </div>
          <div class="ws-divider"></div>
          <div class="welcome-stat">
            <span class="ws-num">{{ msgUnreadCount }}</span>
            <span class="ws-label">未读消息</span>
          </div>
          <div class="ws-divider"></div>
          <div class="welcome-stat">
            <span class="ws-num" :style="{ color: checkedInToday ? 'var(--color-success)' : 'var(--color-warning)' }">
              {{ checkedInToday ? '✓' : '!' }}
            </span>
            <span class="ws-label">{{ checkedInToday ? '已打卡' : '待打卡' }}</span>
          </div>
        </div>
      </div>
      <div class="welcome-deco"></div>
    </div>

    <!-- Stat Cards Row -->
    <div class="stat-cards anim-stagger">
      <div class="stat-card-item" @click="$router.push('/taskboard')">
        <div class="sci-icon sci-icon--primary">
          <el-icon :size="24"><Grid /></el-icon>
        </div>
        <div class="sci-body">
          <div class="sci-label">训练任务</div>
          <div class="sci-num">{{ taskStats.pending + taskStats.inProgress + taskStats.completed }}</div>
          <div class="sci-breakdown">
            <span class="sci-dot" style="background:var(--color-info)"></span>{{ taskStats.pending }}
            <span class="sci-dot" style="background:var(--color-warning)"></span>{{ taskStats.inProgress }}
            <span class="sci-dot" style="background:var(--color-success)"></span>{{ taskStats.completed }}
          </div>
        </div>
        <el-icon class="sci-arrow"><ArrowRight /></el-icon>
      </div>

      <div class="stat-card-item" @click="$router.push('/todolist')">
        <div class="sci-icon sci-icon--warning">
          <el-icon :size="24"><List /></el-icon>
        </div>
        <div class="sci-body">
          <div class="sci-label">TODO 清单</div>
          <div class="sci-num">{{ todoStats.active + todoStats.done }}</div>
          <div class="sci-breakdown">
            <span class="sci-dot" style="background:var(--color-warning)"></span>{{ todoStats.active }} 待完成
            <span class="sci-dot" style="background:var(--color-success)"></span>{{ todoStats.done }} 已完成
          </div>
        </div>
        <el-icon class="sci-arrow"><ArrowRight /></el-icon>
      </div>

      <div class="stat-card-item" @click="$router.push('/checkin')">
        <div class="sci-icon" :class="checkedInToday ? 'sci-icon--success' : 'sci-icon--danger'">
          <el-icon :size="24"><Check /></el-icon>
        </div>
        <div class="sci-body">
          <div class="sci-label">每日打卡</div>
          <div class="sci-num" :style="{ color: checkedInToday ? 'var(--color-success)' : 'var(--color-warning)' }">
            {{ checkedInToday ? '已完成' : '待打卡' }}
          </div>
          <div class="sci-breakdown">
            <span v-if="!checkedInToday">
              <el-button type="success" size="small" :loading="checking" @click.stop="handleDashboardCheckin">
                立即打卡
              </el-button>
            </span>
            <span v-else style="color:var(--color-success); font-size:12px">继续保持！</span>
          </div>
        </div>
        <el-icon class="sci-arrow"><ArrowRight /></el-icon>
      </div>

      <div class="stat-card-item" @click="$router.push('/messages')">
        <div class="sci-icon sci-icon--info">
          <el-icon :size="24"><Bell /></el-icon>
        </div>
        <div class="sci-body">
          <div class="sci-label">消息中心</div>
          <div class="sci-num" :style="{ color: msgUnreadCount > 0 ? 'var(--color-primary)' : 'var(--text-secondary)' }">
            {{ msgUnreadCount }}
          </div>
          <div class="sci-breakdown">
            <span style="color:var(--text-secondary); font-size:12px">
              {{ msgUnreadCount > 0 ? '条未读消息' : '暂无新消息' }}
            </span>
          </div>
        </div>
        <el-icon class="sci-arrow"><ArrowRight /></el-icon>
      </div>
    </div>

    <!-- Bottom Row: Tasks + Messages -->
    <el-row :gutter="20" class="bottom-row">
      <!-- Recent Tasks -->
      <el-col :span="14">
        <el-card shadow="hover" class="content-card">
          <template #header>
            <div class="cc-header">
              <span class="cc-title">我的任务</span>
              <el-button type="primary" link size="small" @click="$router.push('/taskboard')">查看全部 →</el-button>
            </div>
          </template>
          <div v-if="taskLoading" class="card-loading">加载中...</div>
          <div v-else-if="recentTasks.length === 0" class="card-empty">
            <el-icon :size="32" color="var(--text-secondary)"><Grid /></el-icon>
            <p>暂无任务</p>
          </div>
          <div v-else class="task-list">
            <div v-for="task in recentTasks" :key="task.id" class="task-row">
              <div class="task-row-left">
                <div class="task-row-title">{{ task.title }}</div>
                <div class="task-row-meta">
                  <el-tag size="small" :type="priorityType(task.priority)">{{ priorityLabel(task.priority) }}</el-tag>
                  <span v-if="task.deadline" class="task-row-deadline">截止 {{ task.deadline.slice(0,10) }}</span>
                </div>
              </div>
              <el-tag size="small" :type="statusType(task.my_status ?? task.status)">
                {{ statusLabel(task.my_status ?? task.status) }}
              </el-tag>
            </div>
          </div>
        </el-card>
      </el-col>

      <!-- Recent Messages -->
      <el-col :span="10">
        <el-card shadow="hover" class="content-card">
          <template #header>
            <div class="cc-header">
              <span class="cc-title">最近消息</span>
              <el-button type="primary" link size="small" @click="$router.push('/messages')">查看全部 →</el-button>
            </div>
          </template>
          <div v-if="msgLoading" class="card-loading">加载中...</div>
          <div v-else-if="msgRecent.length === 0" class="card-empty">
            <el-icon :size="32" color="var(--text-secondary)"><Bell /></el-icon>
            <p>暂无消息</p>
          </div>
          <div v-else class="msg-list">
            <div v-for="msg in msgRecent" :key="msg.id" class="msg-row" :class="{ unread: msg.is_read === 0 }">
              <el-tag size="small" :type="msgTypeColor(msg.type)" style="flex-shrink:0">{{ msgTypeLabel(msg.type) }}</el-tag>
              <span class="msg-row-title">{{ msg.title }}</span>
              <span class="msg-row-time">{{ msg.created_at?.slice(5, 16) }}</span>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { listTasks, listTodo } from '@/api/task'
import { doCheckin, getCheckins } from '@/api/checkin'
import { getMessages } from '@/api/message'
import { ElMessage } from 'element-plus'
import { Grid, List, Check, Bell, ArrowRight } from '@element-plus/icons-vue'

const userStore = useUserStore()
const taskLoading = ref(true)
const todoLoading = ref(true)
const checkinLoading = ref(true)
const msgLoading = ref(true)
const checking = ref(false)
const checkedInToday = ref(false)
const msgUnreadCount = ref(0)
const msgRecent = ref([])
const recentTasks = ref([])

const taskStats = reactive({ pending: 0, inProgress: 0, completed: 0 })
const todoStats = reactive({ active: 0, done: 0 })

const todayStr = computed(() => {
  const d = new Date()
  const weekdays = ['日', '一', '二', '三', '四', '五', '六']
  return `${d.getFullYear()}年${d.getMonth()+1}月${d.getDate()}日 星期${weekdays[d.getDay()]}`
})

const greetingText = computed(() => {
  const h = new Date().getHours()
  if (h < 6) return '夜深了'
  if (h < 12) return '早上好'
  if (h < 14) return '中午好'
  if (h < 18) return '下午好'
  return '晚上好'
})

const motivationalText = computed(() => {
  const texts = ['保持专注，高效训练', '今天也要加油哦', '持续进步，超越自我', '每一滴汗水都不会白费']
  return texts[new Date().getDate() % texts.length]
})

function priorityType(p) { return { 1: 'danger', 2: 'warning', 3: 'info' }[p] || 'info' }
function priorityLabel(p) { return { 1: '高', 2: '中', 3: '低' }[p] || '低' }
function statusType(s) { return { 0: 'info', 1: 'warning', 2: 'success' }[s] || 'info' }
function statusLabel(s) { return { 0: '待处理', 1: '进行中', 2: '已完成' }[s] || '未知' }

function msgTypeColor(type) {
  const map = { task_new: 'primary', task_update: 'warning', task_done: 'success', task_remind: 'danger', checkin_remind: 'success', user_register: 'warning', kicked: 'danger' }
  return map[type] || 'info'
}
function msgTypeLabel(type) {
  const map = { task_new: '新任务', task_update: '变更', task_done: '完成', task_remind: '到期', checkin_remind: '打卡', user_register: '注册', kicked: '下线' }
  return map[type] || '通知'
}

async function handleDashboardCheckin() {
  checking.value = true
  try {
    const res = await doCheckin({ uid: parseInt(userStore.uid) || 0 })
    if (res.error === 0) { checkedInToday.value = true; ElMessage.success('打卡成功！') }
    else if (res.error === 3001) { checkedInToday.value = true; ElMessage.info('今日已打卡') }
    else ElMessage.error('打卡失败')
  } catch { ElMessage.error('网络错误') }
  checking.value = false
}

onMounted(async () => {
  const uid = parseInt(userStore.uid) || 0

  // Tasks
  try {
    const res = await listTasks({ uid, status: -1, assigned_to: '0' })
    if (res.error === 0 && res.tasks) {
      const myTasks = res.tasks.filter(t => {
        const uids = String(t.assigned_to || '0').split(',').map(Number)
        return uids.includes(uid)
      })
      for (const t of myTasks) {
        const s = t.my_status ?? t.status
        if (s === 0) taskStats.pending++
        else if (s === 1) taskStats.inProgress++
        else if (s === 2) taskStats.completed++
      }
      recentTasks.value = myTasks
        .filter(t => (t.my_status ?? t.status) !== 2)
        .sort((a, b) => (a.priority || 3) - (b.priority || 3))
        .slice(0, 6)
    }
  } catch {} finally { taskLoading.value = false }

  // TODOs
  try {
    const res = await listTodo({ uid, is_finished: 0 })
    if (res.error === 0 && res.todos) {
      for (const t of res.todos) {
        if (t.is_finished === 1) todoStats.done++; else todoStats.active++
      }
    }
  } catch {} finally { todoLoading.value = false }

  // Checkin
  try {
    const now = new Date()
    const ds = `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}-${String(now.getDate()).padStart(2,'0')}`
    const res = await getCheckins({ uid, date_from: ds, date_to: ds })
    if (res.error === 0 && res.records?.length > 0) checkedInToday.value = true
  } catch {} finally { checkinLoading.value = false }

  // Messages
  try {
    const res = await getMessages({ uid, page: 1, page_size: 5 })
    if (res.error === 0) {
      msgUnreadCount.value = res.unread_count || 0
      msgRecent.value = res.messages || []
    }
  } catch {} finally { msgLoading.value = false }
})
</script>

<style scoped>
/* Welcome Banner */
.welcome-banner {
  background: var(--bg-surface);
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  padding: var(--space-8) var(--space-9);
  margin-bottom: var(--space-6);
}
.welcome-content {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.welcome-title {
  font-family: var(--font-mono);
  font-size: var(--text-xl);
  font-weight: 700;
  color: var(--text-primary);
  margin-bottom: var(--space-1);
}
.welcome-sub {
  font-size: var(--text-sm);
  color: var(--text-secondary);
}
.welcome-stats {
  display: flex;
  align-items: center;
  gap: var(--space-6);
}
.welcome-stat { text-align: center; }
.ws-num {
  display: block;
  font-family: var(--font-mono);
  font-size: var(--text-3xl);
  font-weight: 700;
  line-height: 1.2;
  color: var(--text-primary);
}
.ws-label {
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-secondary);
  margin-top: var(--space-1);
  display: block;
}
.ws-divider { width: 1px; height: 40px; background: var(--border-default); }

/* Stat Cards */
.stat-cards {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: var(--space-4);
  margin-bottom: var(--space-6);
}
.stat-card-item {
  background: var(--bg-surface);
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  padding: var(--space-5);
  display: flex;
  align-items: center;
  gap: var(--space-4);
  cursor: pointer;
  transition: border-color 0.15s ease;
}
.stat-card-item:hover {
  border-color: var(--primary);
}
.sci-icon {
  width: 44px;
  height: 44px;
  border-radius: var(--radius-md);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  color: #fff;
  background: var(--bg-elevated);
}
.sci-icon--primary { color: var(--primary); background: var(--bg-elevated); }
.sci-icon--success { color: var(--success); background: var(--bg-elevated); }
.sci-icon--warning { color: var(--warning); background: var(--bg-elevated); }
.sci-icon--danger { color: var(--danger); background: var(--bg-elevated); }
.sci-icon--info { color: var(--text-secondary); background: var(--bg-elevated); }
.sci-body { flex: 1; min-width: 0; }
.sci-label {
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-secondary);
  margin-bottom: var(--space-1);
}
.sci-num {
  font-family: var(--font-mono);
  font-size: var(--text-3xl);
  font-weight: 700;
  line-height: 1.2;
  color: var(--text-primary);
}
.sci-breakdown {
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  color: var(--text-secondary);
  margin-top: var(--space-2);
  display: flex;
  align-items: center;
  gap: var(--space-2);
}
.sci-dot {
  display: inline-block;
  width: 6px;
  height: 6px;
  border-radius: var(--radius-sm);
  margin-right: 2px;
}
.sci-arrow { color: var(--text-secondary); font-size: 16px; opacity: 0; transition: opacity 0.15s ease; }
.stat-card-item:hover .sci-arrow { opacity: 1; }

/* Content Cards */
.bottom-row { margin-bottom: var(--space-5); }
.content-card { height: 100%; border: 1px solid var(--border-default); border-radius: var(--radius-md); }
.cc-header { display: flex; justify-content: space-between; align-items: center; }
.cc-title {
  font-family: var(--font-mono);
  font-weight: 600;
  font-size: var(--text-sm);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
.card-loading { text-align: center; color: var(--text-secondary); padding: var(--space-10) 0; font-family: var(--font-mono); }
.card-empty { text-align: center; color: var(--text-secondary); padding: var(--space-10) 0; }
.card-empty p { margin-top: var(--space-2); font-size: var(--text-sm); }

/* Task list */
.task-list { display: flex; flex-direction: column; }
.task-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--space-3) 0;
  border-bottom: 1px solid var(--border-muted);
}
.task-row:last-child { border-bottom: none; }
.task-row:hover { background: var(--bg-elevated); }
.task-row-title { font-size: var(--text-sm); font-weight: 500; margin-bottom: var(--space-1); }
.task-row-meta { display: flex; align-items: center; gap: var(--space-2); }
.task-row-deadline { font-family: var(--font-mono); font-size: var(--text-xs); color: var(--text-secondary); }

/* Message list */
.msg-list { display: flex; flex-direction: column; }
.msg-row {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) 0;
  border-bottom: 1px solid var(--border-muted);
}
.msg-row:last-child { border-bottom: none; }
.msg-row:hover { background: var(--bg-elevated); }
.msg-row.unread .msg-row-title { font-weight: 600; }
.msg-row-title { flex: 1; font-size: var(--text-sm); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.msg-row-time { font-family: var(--font-mono); font-size: var(--text-xs); color: var(--text-secondary); flex-shrink: 0; }

@media (max-width: 1200px) {
  .stat-cards { grid-template-columns: repeat(2, 1fr); }
}
</style>
