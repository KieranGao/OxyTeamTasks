<template>
  <div class="page-container">
    <div class="page-header">
      <div class="header-row">
        <div>
          <h2>任务管理</h2>
          <p>管理我创建的任务</p>
        </div>
        <el-button type="primary" @click="openCreate">+ 新建任务</el-button>
      </div>
    </div>

    <!-- Filters -->
    <div class="filter-bar">
      <el-select v-model="filterStatus" placeholder="状态筛选" clearable style="width:130px" @change="loadTasks">
        <el-option :value="-1" label="全部状态" />
        <el-option :value="0" label="待处理" />
        <el-option :value="1" label="进行中" />
        <el-option :value="2" label="已完成" />
      </el-select>
      <span class="task-count">共 {{ tasks.length }} 条</span>
    </div>

    <!-- Task Cards -->
    <div v-loading="loading">
      <el-empty v-if="!loading && tasks.length === 0" description="暂无任务" />
      <div v-for="task in tasks" :key="task.id" class="task-card-wrapper">
        <el-card shadow="hover" class="task-card">
          <div class="card-body">
            <div class="card-main">
              <div class="task-title">{{ task.title }}</div>
              <div class="task-desc">{{ task.description || '暂无描述' }}</div>
            </div>
            <div class="card-meta">
              <div class="meta-row">
                <span class="meta-label">指派人:</span>
                <template v-if="!task.assigned_to || task.assigned_to === '0'">
                  <el-tag size="small" type="info">未指派</el-tag>
                </template>
                <template v-else>
                  <el-tag v-for="uid in task.assigned_to.split(',').filter(Boolean)" :key="uid" size="small" style="margin:1px">
                    {{ userName(Number(uid)) }}
                  </el-tag>
                </template>
              </div>
              <div class="meta-row">
                <span class="meta-label">截止:</span>
                <span>{{ task.deadline ? task.deadline.slice(0,10) : '-' }}</span>
              </div>
              <div class="meta-row">
                <span class="meta-label">状态:</span>
                <template v-if="hasAssignees(task) && getStatusCounts(task).total > 1">
                  <span class="status-counts">
                    <el-tag size="small" type="success">已完成[{{ getStatusCounts(task).completed }}/{{ getStatusCounts(task).total }}]</el-tag>
                    <el-tag size="small" type="warning">进行中[{{ getStatusCounts(task).inProgress }}/{{ getStatusCounts(task).total }}]</el-tag>
                    <el-tag size="small" type="info">未开始[{{ getStatusCounts(task).pending }}/{{ getStatusCounts(task).total }}]</el-tag>
                  </span>
                </template>
                <el-tag v-else size="small" :type="statusType(task.status)">{{ statusLabel(task.status) }}</el-tag>
              </div>
              <div class="meta-row">
                <span class="meta-label">优先级:</span>
                <el-tag size="small" :type="priorityType(task.priority)">{{ priorityLabel(task.priority) }}</el-tag>
              </div>
            </div>
          </div>
          <div class="card-actions">
            <el-button v-if="hasAssignees(task)" size="small" @click="openStats(task)">统计</el-button>
            <el-button size="small" @click="openEdit(task)">编辑</el-button>
            <el-button size="small" type="danger" @click="handleDelete(task)">删除</el-button>
          </div>
        </el-card>
      </div>
    </div>

    <!-- Create / Edit Dialog -->
    <el-dialog v-model="dialogVisible" :title="editing ? '编辑任务' : '新建任务'" width="560px" @closed="resetForm">
      <el-form :model="form" label-width="80px" :rules="rules" ref="formRef">
        <el-form-item label="标题" prop="title">
          <el-input v-model="form.title" placeholder="请输入任务标题" />
        </el-form-item>
        <el-form-item label="描述">
          <el-input v-model="form.description" type="textarea" :rows="3" placeholder="任务描述（可选）" />
        </el-form-item>
        <el-form-item v-if="editing" label="状态">
          <el-select v-model="form.status" style="width:100%">
            <el-option :value="0" label="待处理" />
            <el-option :value="1" label="进行中" />
            <el-option :value="2" label="已完成" />
          </el-select>
        </el-form-item>
        <el-form-item label="优先级">
          <el-select v-model="form.priority" style="width:100%">
            <el-option :value="1" label="高" />
            <el-option :value="2" label="中" />
            <el-option :value="3" label="低" />
          </el-select>
        </el-form-item>
        <el-form-item label="指派人">
          <div style="display:flex;flex-direction:column;gap:8px;width:100%">
            <el-select v-model="form.assigned_to" multiple clearable placeholder="选择指派人（可多选）" style="width:100%">
              <el-option v-for="u in assignableUsers" :key="u.uid" :value="String(u.uid)" :label="u.username" />
            </el-select>
            <div class="batch-row">
              <el-button v-if="userStore.isCaptain" size="small" @click="assignAllTeam">指派全队</el-button>
              <template v-if="userStore.isCoach">
                <el-select v-model="selectedTeamForBatch" size="small" placeholder="选择队伍" clearable style="width:120px">
                  <el-option v-for="t in availableTeams" :key="t.id" :value="t.id" :label="'队伍 ' + t.id" />
                </el-select>
                <el-button size="small" @click="assignTeamMembers" :disabled="!selectedTeamForBatch">指派该队全员</el-button>
                <el-button size="small" @click="assignAllMembers">指派所有成员</el-button>
              </template>
            </div>
          </div>
        </el-form-item>
        <el-form-item label="截止日期">
          <el-date-picker v-model="form.deadline" type="date" value-format="YYYY-MM-DD" style="width:100%" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" @click="submitForm" :loading="submitting">{{ editing ? '保存' : '创建' }}</el-button>
      </template>
    </el-dialog>

    <!-- Stats Dialog -->
    <el-dialog v-model="statsDialogVisible" title="任务完成统计" width="480px">
      <template v-if="statsTask">
        <div class="stats-task-title">任务: {{ statsTask.title }}</div>
        <div class="stats-section">
          <div class="stats-section-header">
            <el-tag type="success">已完成 ({{ getStatusCounts(statsTask).completed }})</el-tag>
          </div>
          <div class="stats-user-list">
            <span v-for="a in getCompletedAssignees(statsTask)" :key="a.uid" class="stats-user-row">
              <el-tag size="small" type="success" effect="plain">{{ a.name }}</el-tag>
              <el-button size="small" type="danger" link @click="rejectAssignee(statsTask, a.uid)">打回</el-button>
            </span>
            <span v-if="getCompletedAssignees(statsTask).length === 0" class="stats-empty">-</span>
          </div>
        </div>
        <div class="stats-section">
          <div class="stats-section-header">
            <el-tag type="warning">进行中 ({{ getStatusCounts(statsTask).inProgress }})</el-tag>
          </div>
          <div class="stats-user-list">
            <el-tag v-for="name in getUsersByStatus(statsTask, 1)" :key="name" size="small" type="warning" effect="plain">{{ name }}</el-tag>
            <span v-if="getUsersByStatus(statsTask, 1).length === 0" class="stats-empty">-</span>
          </div>
        </div>
        <div class="stats-section">
          <div class="stats-section-header">
            <el-tag type="info">未开始 ({{ getStatusCounts(statsTask).pending }})</el-tag>
          </div>
          <div class="stats-user-list">
            <el-tag v-for="name in getUsersByStatus(statsTask, 0)" :key="name" size="small" type="info" effect="plain">{{ name }}</el-tag>
            <span v-if="getUsersByStatus(statsTask, 0).length === 0" class="stats-empty">-</span>
          </div>
        </div>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, nextTick } from 'vue'
import { useUserStore } from '@/stores/user'
import { listTasks, createTask, updateTask, deleteTask } from '@/api/task'
import { listAllUsers } from '@/api/user'
import { ElMessage, ElMessageBox } from 'element-plus'

const userStore = useUserStore()
const tasks = ref([])
const users = ref([])
const loading = ref(false)
const dialogVisible = ref(false)
const editing = ref(false)
const submitting = ref(false)
const filterStatus = ref(-1)
const selectedTeamForBatch = ref(null)
const statsDialogVisible = ref(false)
const statsTask = ref(null)
const formRef = ref(null)

const form = reactive({ id: 0, title: '', description: '', status: 0, priority: 3, deadline: '', assigned_to: [] })

const rules = { title: [{ required: true, message: '请输入任务标题', trigger: 'blur' }] }

const userMap = {}
function userName(uid) {
  if (!uid) return '-'
  return userMap[uid] || `UID:${uid}`
}

function statusType(s) { return { 0: 'info', 1: 'warning', 2: 'success' }[s] || 'info' }
function statusLabel(s) { return { 0: '待处理', 1: '进行中', 2: '已完成' }[s] || '未知' }
function priorityType(p) { return { 1: 'danger', 2: 'warning', 3: 'info' }[p] || 'info' }
function priorityLabel(p) { return { 1: '高', 2: '中', 3: '低' }[p] || '低' }

const availableTeams = computed(() => {
  const teamMap = {}
  for (const u of users.value) {
    if (u.belong_team_id && u.belong_team_id > 0) {
      if (!teamMap[u.belong_team_id]) teamMap[u.belong_team_id] = { id: u.belong_team_id, count: 0 }
      teamMap[u.belong_team_id].count++
    }
  }
  return Object.values(teamMap)
})

const assignableUsers = computed(() => {
  if (userStore.isCoach) return users.value
  const myTeamId = parseInt(userStore.belongTeamId) || 0
  return users.value.filter(u => u.belong_team_id === myTeamId)
})

function assignAllTeam() {
  const myTeamId = parseInt(userStore.belongTeamId) || 0
  form.assigned_to = users.value
    .filter(u => u.belong_team_id === myTeamId)
    .map(u => String(u.uid))
}

function assignTeamMembers() {
  if (!selectedTeamForBatch.value) return
  form.assigned_to = users.value
    .filter(u => u.belong_team_id === selectedTeamForBatch.value)
    .map(u => String(u.uid))
}

function assignAllMembers() {
  form.assigned_to = users.value.map(u => String(u.uid))
}

function hasAssignees(task) {
  const assigned = task.assigned_to
  return assigned && assigned !== '0' && assigned !== ''
}

function getStatusCounts(task) {
  const assignedUids = (task.assigned_to || '').split(',').filter(Boolean).map(Number)
  const total = assignedUids.length
  if (total === 0) return { completed: 0, inProgress: 0, pending: 0, total: 0 }
  let completed = 0, inProgress = 0
  const statuses = task.assignee_statuses || []
  for (const uid of assignedUids) {
    const found = statuses.find(s => s.assignee_uid === uid)
    const s = found ? found.status : task.status
    if (s === 2) completed++
    else if (s === 1) inProgress++
  }
  return { completed, inProgress, pending: total - completed - inProgress, total }
}

function getUsersByStatus(task, status) {
  const assignedUids = (task.assigned_to || '').split(',').filter(Boolean).map(Number)
  const statuses = task.assignee_statuses || []
  return assignedUids.filter(uid => {
    const found = statuses.find(s => s.assignee_uid === uid)
    const s = found ? found.status : task.status
    return s === status
  }).map(uid => userName(uid))
}

function getCompletedAssignees(task) {
  const assignedUids = (task.assigned_to || '').split(',').filter(Boolean).map(Number)
  const statuses = task.assignee_statuses || []
  return assignedUids.filter(uid => {
    const found = statuses.find(s => s.assignee_uid === uid)
    const s = found ? found.status : task.status
    return s === 2
  }).map(uid => ({ uid, name: userName(uid) }))
}

async function rejectAssignee(task, assigneeUid) {
  try {
    await ElMessageBox.confirm(
      `确定将 ${userName(assigneeUid)} 的任务状态打回到"进行中"？`,
      '确认打回',
      { confirmButtonText: '确定打回', cancelButtonText: '取消', type: 'warning' }
    )
  } catch { return }
  try {
    const res = await updateTask({
      id: task.id, uid: assigneeUid,
      title: task.title, description: task.description || '',
      status: 1, priority: task.priority,
      deadline: task.deadline || '', assigned_to: task.assigned_to || '0'
    })
    if (res.error === 0) {
      ElMessage.success('已打回')
      await loadTasks()
      statsTask.value = tasks.value.find(t => t.id === task.id) || null
      if (!statsTask.value) statsDialogVisible.value = false
    } else {
      ElMessage.error('操作失败')
    }
  } catch (e) {
    ElMessage.error('操作失败')
  }
}

function openStats(task) {
  statsTask.value = task
  statsDialogVisible.value = true
}

async function loadUsers() {
  try {
    const res = await listAllUsers()
    if (res.error === 0 && res.users) {
      users.value = res.users.filter(u => u.status === 1)
      for (const u of res.users) userMap[u.uid] = u.username
    }
  } catch (e) { /* ignore */ }
}

async function loadTasks() {
  loading.value = true
  try {
    const myUid = parseInt(userStore.uid) || 0
    const res = await listTasks({ uid: myUid, status: filterStatus.value, assigned_to: '0' })
    if (res.error === 0) {
      let taskList = res.tasks || []
      // Only show tasks I created (backend also returns tasks where I'm an assignee)
      taskList = taskList.filter(t => t.uid === myUid)
      tasks.value = taskList
    } else {
      tasks.value = []
    }
  } catch (e) { tasks.value = [] }
  loading.value = false
}

function openCreate() {
  editing.value = false
  resetForm()
  dialogVisible.value = true
}

function openEdit(task) {
  editing.value = true
  form.id = task.id
  form.title = task.title
  form.description = task.description || ''
  form.status = task.status
  form.priority = task.priority
  form.deadline = task.deadline ? task.deadline.slice(0, 10) : ''
  form.assigned_to = task.assigned_to && task.assigned_to !== '0'
    ? task.assigned_to.split(',').filter(Boolean)
    : []
  dialogVisible.value = true
}

function resetForm() {
  form.id = 0; form.title = ''; form.description = ''
  form.status = 0; form.priority = 3; form.deadline = ''; form.assigned_to = []
  selectedTeamForBatch.value = null
  editing.value = false
  nextTick(() => formRef.value?.clearValidate())
}

async function submitForm() {
  const valid = await formRef.value.validate().catch(() => false)
  if (!valid) return
  submitting.value = true
  try {
    const uid = parseInt(userStore.uid) || 0
    const assignedToStr = Array.isArray(form.assigned_to) ? form.assigned_to.join(',') : (form.assigned_to || '0')
    if (editing.value) {
      // uid=0 for shared field edit (not personal status change)
      const res = await updateTask({
        id: form.id, uid: 0,
        title: form.title, description: form.description,
        status: form.status, priority: form.priority,
        deadline: form.deadline || '', assigned_to: assignedToStr
      })
      if (res.error === 0) { ElMessage.success('已更新'); dialogVisible.value = false; loadTasks() }
      else ElMessage.error('更新失败')
    } else {
      const res = await createTask({
        uid, title: form.title, description: form.description,
        priority: form.priority, deadline: form.deadline || '', assigned_to: assignedToStr
      })
      if (res.error === 0) { ElMessage.success('已创建'); dialogVisible.value = false; loadTasks() }
      else ElMessage.error('创建失败')
    }
  } catch (e) { ElMessage.error('操作失败') }
  submitting.value = false
}

async function handleDelete(task) {
  try { await ElMessageBox.confirm(`确定删除任务 "${task.title}"？`, '确认删除', { type: 'warning' }) }
  catch { return }
  try {
    const res = await deleteTask({ id: task.id, uid: 0 })
    if (res.error === 0) { ElMessage.success('已删除'); loadTasks() }
    else ElMessage.error('删除失败')
  } catch (e) { ElMessage.error('操作失败') }
}

onMounted(async () => {
  await loadUsers()
  await loadTasks()
})
</script>

<style scoped>
.header-row { display: flex; justify-content: space-between; align-items: flex-start; }
.filter-bar {
  display: flex; gap: 12px; align-items: center; margin-bottom: 20px;
  padding: 14px 20px; background: var(--glass-bg); backdrop-filter: var(--glass-blur);
  border-radius: var(--radius-lg); border: 1px solid var(--border-light);
  box-shadow: var(--shadow-sm);
}
.task-count { color: var(--text-secondary); font-size: 13px; font-weight: 500; }

.batch-row { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; }
.task-card-wrapper { margin-bottom: 14px; }
.task-card {
  border-left: 4px solid var(--color-primary);
  transition: all var(--transition-fast);
}
.task-card:hover { transform: translateX(3px); box-shadow: var(--shadow-md); }
.card-body { display: flex; gap: 24px; }
.card-main { flex: 1; min-width: 0; }
.card-meta { display: flex; flex-direction: column; gap: 8px; min-width: 200px; flex-shrink: 0; }
.meta-row { display: flex; align-items: center; gap: 6px; font-size: 13px; }
.meta-label { color: var(--text-secondary); min-width: 48px; font-weight: 500; }
.card-actions { display: flex; justify-content: flex-end; gap: 8px; margin-top: 14px; padding-top: 14px; border-top: 1px solid var(--border-light); }

.task-title { font-weight: 700; font-size: 16px; margin-bottom: 8px; letter-spacing: -0.2px; }
.task-desc { font-size: 13px; color: var(--text-secondary); line-height: 1.6;
  overflow: hidden; text-overflow: ellipsis; display: -webkit-box;
  -webkit-line-clamp: 3; -webkit-box-orient: vertical; }

.status-counts { display: flex; gap: 4px; flex-wrap: wrap; }

.stats-task-title { font-weight: 700; font-size: 15px; margin-bottom: 18px; }
.stats-section { margin-bottom: 16px; }
.stats-section-header { margin-bottom: 8px; font-weight: 600; }
.stats-user-list { display: flex; gap: 6px; flex-wrap: wrap; }
.stats-user-row { display: inline-flex; align-items: center; gap: 2px; }
.stats-empty { color: var(--text-secondary); font-size: 13px; }
</style>
