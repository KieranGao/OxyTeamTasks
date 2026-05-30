<template>
  <div class="page-container">
    <div class="page-header">
      <h2>队伍信息</h2>
      <p>队伍成员、任务完成情况及打卡统计</p>
    </div>

    <!-- Overall stats -->
    <el-row :gutter="16" class="overall-row" v-if="!loading">
      <el-col :span="6">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num">{{ members.length }}</div>
          <div class="summary-label">队员总数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num">{{ overall.totalTasks }}</div>
          <div class="summary-label">总任务数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num" style="color:var(--color-warning)">{{ overall.unfinished }}</div>
          <div class="summary-label">未完成任务</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num" style="color:var(--color-success)">{{ overall.avgRate }}%</div>
          <div class="summary-label">平均完成率</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- Member roster table -->
    <el-card shadow="hover" style="margin-bottom:16px" v-if="members.length">
      <template #header>队员名册</template>
      <el-table :data="members" stripe>
        <el-table-column prop="username" label="姓名" width="120" />
        <el-table-column label="角色" width="80">
          <template #default="{ row }">
            <el-tag size="small" :type="row.role === 1 ? 'warning' : row.role === 2 ? 'danger' : ''">
              {{ roleLabel(row.role) }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="任务完成率" width="160">
          <template #default="{ row }">
            <el-progress :percentage="row.taskRate" :stroke-width="10" />
          </template>
        </el-table-column>
        <el-table-column label="待处理" width="80">
          <template #default="{ row }">{{ row.pending }}</template>
        </el-table-column>
        <el-table-column label="进行中" width="80">
          <template #default="{ row }">{{ row.inProgress }}</template>
        </el-table-column>
        <el-table-column label="已完成" width="80">
          <template #default="{ row }">{{ row.completed }}</template>
        </el-table-column>
      </el-table>
    </el-card>

    <!-- Individual tasks per member -->
    <el-card v-for="m in membersWithTasks" :key="'tasks-' + m.uid" shadow="hover" style="margin-bottom:16px">
      <template #header>
        <div class="member-task-header">
          <span>{{ m.username }} 的任务 ({{ m.tasks.length }})</span>
          <el-tag size="small" :type="m.role === 2 ? 'danger' : m.role === 1 ? 'warning' : ''">
            {{ roleLabel(m.role) }}
          </el-tag>
        </div>
      </template>
      <el-table :data="m.tasks" stripe max-height="300">
        <el-table-column prop="title" label="标题" min-width="130" show-overflow-tooltip />
        <el-table-column label="描述" min-width="150" show-overflow-tooltip>
          <template #default="{ row }">{{ row.description || '-' }}</template>
        </el-table-column>
        <el-table-column label="创建者" width="100">
          <template #default="{ row }">{{ userName(row.uid) }}</template>
        </el-table-column>
        <el-table-column label="状态" width="90">
          <template #default="{ row }">
            <el-tag size="small" :type="statusType(row._displayStatus)">{{ statusLabel(row._displayStatus) }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="优先级" width="80">
          <template #default="{ row }">
            <el-tag size="small" :type="priorityType(row.priority)">{{ priorityLabel(row.priority) }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="截止" width="110">
          <template #default="{ row }">{{ row.deadline ? row.deadline.slice(0,10) : '-' }}</template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-card shadow="hover" style="margin-bottom:16px" v-if="!loading && membersWithTasks.length === 0">
      <el-empty description="暂无队伍任务" :image-size="60" />
    </el-card>

    <!-- Member progress cards -->
    <div v-loading="loading">
      <el-empty v-if="!loading && members.length === 0" description="暂无队员数据" />
      <el-row :gutter="16">
        <el-col v-for="m in members" :key="m.uid" :xs="24" :md="12" :lg="8" style="margin-bottom:16px">
          <el-card shadow="hover" class="member-card">
            <template #header>
              <div class="member-header">
                <span class="member-name">{{ m.username }}</span>
                <el-tag size="small" :type="m.role === 2 ? 'danger' : m.role === 1 ? 'warning' : ''">
                  {{ roleLabel(m.role) }}
                </el-tag>
              </div>
            </template>

            <div class="member-section">
              <div class="section-title">任务进度</div>
              <el-progress
                :percentage="m.taskRate"
                :color="m.taskRate === 100 ? 'var(--color-success)' : 'var(--color-primary)'"
                :stroke-width="14"
              />
              <div class="stat-row">
                <span class="stat-chip pending">待处理 {{ m.pending }}</span>
                <span class="stat-chip progress">进行中 {{ m.inProgress }}</span>
                <span class="stat-chip done">已完成 {{ m.completed }}</span>
              </div>
            </div>

          </el-card>
        </el-col>
      </el-row>
    </div>

    <!-- Check-in stats -->
    <el-card shadow="hover" style="margin-top:16px">
      <template #header>打卡统计</template>
      <!-- Today's check-in summary -->
      <div v-if="!loading" style="margin-bottom:16px">
        <h4 style="margin-bottom:8px">今日打卡 ({{ checkedInToday.length }}/{{ members.length }})</h4>
        <div style="display:flex; flex-wrap:wrap; gap:8px">
          <el-tag v-for="m in checkedInToday" :key="'ci-'+m.uid" type="success">{{ m.username }}</el-tag>
          <el-tag v-for="m in notCheckedInToday" :key="'nc-'+m.uid" type="info">{{ m.username }}</el-tag>
          <span v-if="members.length === 0" style="color:var(--text-secondary)">暂无队员数据</span>
        </div>
      </div>

      <!-- Weekly check-in counts per member -->
      <div v-if="!loading">
        <h4 style="margin-bottom:8px">本周打卡天数</h4>
        <el-row :gutter="12">
          <el-col v-for="m in members" :key="'wk-'+m.uid" :xs="12" :md="8" :lg="6" style="margin-bottom:12px">
            <el-card shadow="never" class="checkin-member-card" @click="openMemberCheckinDialog(m)">
              <div style="display:flex; justify-content:space-between; align-items:center">
                <span style="font-weight:600">{{ m.username }}</span>
                <el-tag :type="weeklyCount(m.uid) > 0 ? 'success' : 'info'" size="small">{{ weeklyCount(m.uid) }} 天</el-tag>
              </div>
            </el-card>
          </el-col>
        </el-row>
      </div>

      <!-- Member check-in history dialog -->
      <el-dialog v-model="dialogVisible" :title="dialogTitle" width="480px" destroy-on-close>
        <div v-if="dialogUid > 0">
          <div style="display:flex; justify-content:center; align-items:center; margin-bottom:16px">
            <el-button size="small" @click="subDialogMonth">&lt; 上月</el-button>
            <span style="font-weight:600; margin:0 12px">{{ dialogYearMonth }}</span>
            <el-button size="small" @click="addDialogMonth">下月 &gt;</el-button>
          </div>
          <div class="cal-grid">
            <div class="cal-weekday" v-for="d in weekdays" :key="d">{{ d }}</div>
            <div
              v-for="cell in dialogCalendarCells"
              :key="cell.key"
              class="cal-cell"
              :class="{ 'cal-other-month': !cell.isCurrentMonth, 'cal-checked': cell.checked }"
            >
              <span class="cal-day-num">{{ cell.day }}</span>
              <el-icon v-if="cell.checked" color="var(--color-success)" :size="16"><Check /></el-icon>
            </div>
          </div>
        </div>
      </el-dialog>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { listTasks } from '@/api/task'
import { listAllUsers } from '@/api/user'
import { getCheckins } from '@/api/checkin'
import { Check } from '@element-plus/icons-vue'

const userStore = useUserStore()
const members = ref([])
const allTasks = ref([])
const loading = ref(true)

// Check-in state
const checkinRecords = ref([])
const dialogVisible = ref(false)
const dialogUid = ref(0)
const dialogTitle = ref('')
const dialogYear = ref(new Date().getFullYear())
const dialogMonth = ref(new Date().getMonth() + 1)
const dialogCheckedSet = ref(new Set())
const weekdays = ['日', '一', '二', '三', '四', '五', '六']

const userMap = {}
function userName(uid) {
  if (!uid) return '-'
  return userMap[uid] || `UID:${uid}`
}

const overall = computed(() => {
  let total = 0, unfinished = 0, rates = 0, count = 0
  for (const m of members.value) {
    total += m.totalTasks
    unfinished += m.pending + m.inProgress
    if (m.totalTasks > 0) { rates += m.taskRate; count++ }
  }
  return {
    totalTasks: total,
    unfinished,
    avgRate: count > 0 ? Math.round(rates / count) : 0
  }
})

const membersWithTasks = computed(() =>
  members.value.map(m => ({
    ...m,
    tasks: allTasks.value.filter(t => {
      const assignedUids = String(t.assigned_to || '0').split(',').map(Number)
      return assignedUids.includes(m.uid)
    }).map(t => {
      let displayStatus = t.status
      if (t.assignee_statuses) {
        const as = t.assignee_statuses.find(a => a.assignee_uid === m.uid)
        if (as) displayStatus = as.status
      }
      return { ...t, _displayStatus: displayStatus }
    })
  })).filter(m => m.tasks.length > 0)
)

function roleLabel(r) { return { 0: '队员', 1: '队长', 2: '教练' }[r] || '未知' }
function statusType(s) { return { 0: 'info', 1: 'warning', 2: 'success' }[s] || 'info' }
function statusLabel(s) { return { 0: '待处理', 1: '进行中', 2: '已完成' }[s] || '未知' }
function priorityType(p) { return { 1: 'danger', 2: 'warning', 3: 'info' }[p] || 'info' }
function priorityLabel(p) { return { 1: '高', 2: '中', 3: '低' }[p] || '低' }

// --- Check-in helpers ---
function pad2(n) { return String(n).padStart(2, '0') }
function dateStr2(y, m, d) { return `${y}-${pad2(m)}-${pad2(d)}` }

const todayStr = computed(() => {
  const now = new Date()
  return dateStr2(now.getFullYear(), now.getMonth() + 1, now.getDate())
})

const weekRange = computed(() => {
  const now = new Date()
  const day = now.getDay()
  const mondayOffset = day === 0 ? -6 : 1 - day
  const monday = new Date(now)
  monday.setDate(now.getDate() + mondayOffset)
  const sunday = new Date(monday)
  sunday.setDate(monday.getDate() + 6)
  return {
    from: dateStr2(monday.getFullYear(), monday.getMonth() + 1, monday.getDate()),
    to: dateStr2(sunday.getFullYear(), sunday.getMonth() + 1, sunday.getDate())
  }
})

const checkedInToday = computed(() =>
  members.value.filter(m => checkinRecords.value.some(r => r.uid === m.uid && r.checkin_date === todayStr.value))
)

const notCheckedInToday = computed(() =>
  members.value.filter(m => !checkinRecords.value.some(r => r.uid === m.uid && r.checkin_date === todayStr.value))
)

function weeklyCount(uid) {
  return checkinRecords.value.filter(r =>
    r.uid === uid && r.checkin_date >= weekRange.value.from && r.checkin_date <= weekRange.value.to
  ).length
}

const dialogYearMonth = computed(() => `${dialogYear.value}年${dialogMonth.value}月`)

function buildCalendarCells(year, month, checkedSet) {
  const firstDay = new Date(year, month - 1, 1)
  const lastDay = new Date(year, month, 0)
  const startDow = firstDay.getDay()
  const totalDays = lastDay.getDate()
  const cells = []
  const prevLast = new Date(year, month - 1, 0).getDate()
  for (let i = startDow - 1; i >= 0; i--) {
    const d = prevLast - i
    cells.push({ key: `prev-${d}`, day: d, isCurrentMonth: false, checked: checkedSet.has(dateStr2(year, month - 1, d)) })
  }
  for (let d = 1; d <= totalDays; d++) {
    cells.push({ key: `cur-${d}`, day: d, isCurrentMonth: true, checked: checkedSet.has(dateStr2(year, month, d)) })
  }
  const remaining = 7 - (cells.length % 7)
  if (remaining < 7) {
    for (let d = 1; d <= remaining; d++) {
      cells.push({ key: `next-${d}`, day: d, isCurrentMonth: false, checked: checkedSet.has(dateStr2(year, month + 1, d)) })
    }
  }
  return cells
}

const dialogCalendarCells = computed(() => buildCalendarCells(dialogYear.value, dialogMonth.value, dialogCheckedSet.value))

async function loadCheckinData() {
  try {
    const res = await getCheckins({ uid: 0, date_from: weekRange.value.from, date_to: weekRange.value.to })
    if (res.error === 0 && res.records) checkinRecords.value = res.records
  } catch (e) { /* ignore */ }
}

function subDialogMonth() {
  if (dialogMonth.value === 1) { dialogYear.value--; dialogMonth.value = 12 }
  else dialogMonth.value--
  loadDialogCheckins()
}

function addDialogMonth() {
  if (dialogMonth.value === 12) { dialogYear.value++; dialogMonth.value = 1 }
  else dialogMonth.value++
  loadDialogCheckins()
}

async function loadDialogCheckins() {
  const from = dateStr2(dialogYear.value, dialogMonth.value, 1)
  const lastDay = new Date(dialogYear.value, dialogMonth.value, 0).getDate()
  const to = dateStr2(dialogYear.value, dialogMonth.value, lastDay)
  try {
    const res = await getCheckins({ uid: dialogUid.value, date_from: from, date_to: to })
    if (res.error === 0 && res.records) dialogCheckedSet.value = new Set(res.records.map(r => r.checkin_date))
  } catch (e) { /* ignore */ }
}

async function openMemberCheckinDialog(member) {
  dialogUid.value = member.uid
  dialogTitle.value = `${member.username} 的打卡记录`
  dialogYear.value = new Date().getFullYear()
  dialogMonth.value = new Date().getMonth() + 1
  dialogVisible.value = true
  await loadDialogCheckins()
}

async function loadData() {
  loading.value = true
  try {
    const myTeamId = parseInt(userStore.belongTeamId) || 0
    const [usersRes, tasksRes] = await Promise.all([
      listAllUsers(),
      listTasks({ uid: 0, status: -1, assigned_to: '0' })
    ])

    const users = (usersRes.error === 0 && usersRes.users) ? usersRes.users : []
    allTasks.value = (tasksRes.error === 0 && tasksRes.tasks) ? tasksRes.tasks : []
    const teamUsers = users.filter(u => u.status === 1 && (myTeamId === 0 || u.belong_team_id === myTeamId))

    for (const u of users) userMap[u.uid] = u.username

    const result = []
    for (const u of teamUsers) {
      const userTasks = allTasks.value.filter(t => {
        const assignedUids = String(t.assigned_to || '0').split(',').map(Number)
        return assignedUids.includes(u.uid)
      })
      // Use per-assignee status when available
      let pending = 0, inProgress = 0, completed = 0
      for (const t of userTasks) {
        let s = t.status
        if (t.assignee_statuses) {
          const as = t.assignee_statuses.find(a => a.assignee_uid === u.uid)
          if (as) s = as.status
        }
        if (s === 0) pending++
        else if (s === 1) inProgress++
        else if (s === 2) completed++
      }
      const totalTasks = userTasks.length
      const taskRate = totalTasks > 0 ? Math.round((completed / totalTasks) * 100) : 0

      result.push({
        uid: u.uid, username: u.username, role: u.role,
        pending, inProgress, completed, totalTasks, taskRate
      })
    }
    members.value = result
  } catch (e) { members.value = [] }
  await loadCheckinData()
  loading.value = false
}

onMounted(loadData)
</script>

<style scoped>
.overall-row { margin-bottom: 24px; }
.summary-card {
  text-align: center;
  border-top: 4px solid var(--color-primary);
  transition: all var(--transition-fast);
  overflow: hidden;
}
.summary-card:hover { transform: translateY(-3px); box-shadow: var(--shadow-md); }
.summary-num {
  font-size: 34px; font-weight: 800;
  background: var(--gradient-primary);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  letter-spacing: -1px;
}
.summary-label { font-size: 13px; color: var(--text-secondary); margin-top: 6px; font-weight: 500; }

.member-card { height: 100%; transition: all var(--transition-fast); }
.member-card:hover { box-shadow: var(--shadow-md); }
.member-header { display: flex; justify-content: space-between; align-items: center; }
.member-name { font-weight: 700; font-size: 15px; }
.member-section { margin-bottom: 4px; }
.section-title { font-size: 12px; color: var(--text-secondary); margin-bottom: 10px; text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600; }
.stat-row { display: flex; gap: 8px; margin-top: 12px; flex-wrap: wrap; }
.stat-chip {
  font-size: 12px; padding: 3px 10px; border-radius: var(--radius-sm);
  background: var(--border-light); font-weight: 500;
}
.stat-chip.pending { color: var(--color-info); }
.stat-chip.progress { color: var(--color-warning); }
.stat-chip.done { color: var(--color-success); }
.member-task-header { display: flex; justify-content: space-between; align-items: center; }

/* Check-in */
.checkin-member-card { cursor: pointer; transition: all var(--transition-fast); border-left: 3px solid var(--color-primary); }
.checkin-member-card:hover { box-shadow: var(--shadow-md); transform: translateY(-2px); }
.cal-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 5px; }
.cal-weekday { text-align: center; font-weight: 600; font-size: 12px; padding: 10px 0; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 1px; }
.cal-cell {
  text-align: center; padding: 8px 4px; border-radius: var(--radius-sm);
  min-height: 48px; display: flex; flex-direction: column; align-items: center; justify-content: center;
  background: var(--bg-secondary);
  border: 1px solid transparent;
  transition: all var(--transition-fast);
}
.cal-day-num { font-size: 13px; margin-bottom: 2px; font-weight: 500; }
.cal-cell.cal-other-month .cal-day-num { color: var(--text-secondary); opacity: 0.2; }
.cal-cell.cal-checked { background: var(--color-success-bg); border-color: rgba(34, 197, 94, 0.2); }
</style>
