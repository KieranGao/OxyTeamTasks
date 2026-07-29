<template>
  <div class="page-container">
    <div class="page-header">
      <div class="header-row">
        <div>
          <h2>任务看板</h2>
          <p>查看个人训练任务</p>
        </div>
      </div>
    </div>

    <el-row :gutter="16" class="kanban-row">
      <el-col :span="8" v-for="col in columns" :key="col.status">
        <div class="kanban-col" :style="{ backgroundColor: col.bg }">
          <div class="col-header">
            <span>{{ col.label }}</span>
            <el-tag size="small" :type="col.tagType" effect="dark">{{ col.tasks.length }}</el-tag>
          </div>
          <div class="col-body">
            <div v-if="col.tasks.length === 0" class="col-empty">暂无任务</div>
            <el-card
              v-for="task in col.tasks" :key="task.id"
              shadow="hover" class="task-card"
            >
              <div class="task-title">{{ task.title }}</div>
              <div class="task-desc" v-if="task.description">{{ task.description.length > 60 ? task.description.slice(0, 60) + '...' : task.description }}</div>
              <div class="task-meta">
                <el-tag size="small" :type="priorityType(task.priority)">{{ priorityLabel(task.priority) }}</el-tag>
                <span v-if="task.deadline" class="task-deadline">截止: {{ task.deadline.slice(0,10) }}</span>
              </div>
              <div class="task-creator">创建者: {{ userName(task.uid) }}</div>
              <div v-if="col.status !== 2" class="task-actions" @click.stop>
                <template v-if="col.status === 0">
                  <el-button size="small" type="primary" @click.stop="changeStatus(task, 1)">→ 开始</el-button>
                </template>
                <template v-if="col.status === 1">
                  <el-button size="small" @click.stop="changeStatus(task, 0)">← 退回</el-button>
                  <el-button size="small" type="success" @click.stop="changeStatus(task, 2)">✓ 完成</el-button>
                </template>
              </div>
            </el-card>
          </div>
        </div>
      </el-col>
    </el-row>

  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { listTasks, updateTask } from '@/api/task'
import { listAllUsers } from '@/api/user'
import { ElMessage, ElMessageBox } from 'element-plus'

const userStore = useUserStore()
const allTasks = ref([])

const userMap = {}
function userName(uid) {
  if (!uid) return '-'
  return userMap[uid] || `UID:${uid}`
}

const columns = computed(() => [
  { status: 0, label: '待处理', tagType: 'info', bg: 'var(--border-light)', tasks: allTasks.value.filter(t => (t.my_status ?? t.status) === 0) },
  { status: 1, label: '进行中', tagType: 'warning', bg: 'var(--color-warning-bg)', tasks: allTasks.value.filter(t => (t.my_status ?? t.status) === 1) },
  { status: 2, label: '已完成', tagType: 'success', bg: 'var(--color-success-bg)', tasks: allTasks.value.filter(t => (t.my_status ?? t.status) === 2) },
])

function priorityType(p) { return { 1: 'danger', 2: 'warning', 3: 'info' }[p] || 'info' }
function priorityLabel(p) { return { 1: '高', 2: '中', 3: '低' }[p] || '低' }

async function loadUsers() {
  try {
    const res = await listAllUsers()
    if (res.error === 0 && res.users) {
      for (const u of res.users) userMap[u.uid] = u.username
    }
  } catch (e) { /* ignore */ }
}

async function loadTasks() {
  const uid = parseInt(userStore.uid) || 0
  try {
    const res = await listTasks({ uid, status: -1, assigned_to: '0' })
    if (res.error === 0) {
      // Only show tasks where I'm an assignee, not tasks I created for others
      const taskList = res.tasks || []
      allTasks.value = taskList.filter(t => {
        const assignedUids = String(t.assigned_to || '0').split(',').map(Number)
        return assignedUids.includes(uid)
      })
    } else {
      allTasks.value = []
    }
  } catch (e) {
    allTasks.value = []
  }
}

async function changeStatus(task, newStatus) {
  try {
    if (newStatus === 2) {
      await ElMessageBox.confirm(
        '确认将该任务标记为"已完成"？提交后任务发布者可进行审核打回。',
        '确认完成',
        { confirmButtonText: '确认完成', cancelButtonText: '取消', type: 'warning' }
      )
    }
    const uid = parseInt(userStore.uid) || 0
    const res = await updateTask({
      id: task.id, uid,
      title: task.title, description: task.description || '',
      status: newStatus, priority: task.priority,
      deadline: task.deadline || '', assigned_to: task.assigned_to || '0'
    })
    if (res.error === 0) { ElMessage.success('状态已更新'); loadTasks() }
    else ElMessage.error('更新失败')
  } catch (e) {
    if (e !== 'cancel') ElMessage.error('操作失败')
  }
}

onMounted(async () => { await loadUsers(); await loadTasks() })
</script>

<style scoped>
.header-row { display: flex; justify-content: space-between; align-items: flex-start; }
.kanban-row { min-height: 400px; }
.kanban-col {
  background: var(--bg-surface);
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  padding: var(--space-4);
  display: flex;
  flex-direction: column;
}
.kanban-col:nth-child(1) { border-top: 2px solid var(--primary); }
.kanban-col:nth-child(2) { border-top: 2px solid var(--warning); }
.kanban-col:nth-child(3) { border-top: 2px solid var(--success); }

.col-header {
  display: flex;
  align-items: center;
  gap: var(--space-3);
  margin-bottom: var(--space-4);
  font-family: var(--font-mono);
  font-weight: 700;
  font-size: var(--text-sm);
  text-transform: uppercase;
  letter-spacing: 0.08em;
}
.col-body { flex: 1; display: flex; flex-direction: column; gap: var(--space-3); }
.col-empty { text-align: center; color: var(--text-secondary); padding: var(--space-10) 0; font-family: var(--font-mono); font-size: var(--text-sm); }

.task-card {
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  background: var(--bg-surface);
  transition: border-color 0.15s ease;
}
.task-card:hover {
  border-color: var(--primary);
}

.task-title { font-family: var(--font-mono); font-weight: 600; margin-bottom: var(--space-2); font-size: var(--text-sm); }
.task-meta { display: flex; align-items: center; gap: var(--space-2); margin-bottom: var(--space-2); }
.task-desc { font-size: var(--text-xs); color: var(--text-secondary); margin-bottom: var(--space-2); line-height: 1.5; }
.task-deadline { font-family: var(--font-mono); font-size: var(--text-xs); color: var(--text-secondary); }
.task-creator { font-size: var(--text-xs); color: var(--text-secondary); margin-bottom: var(--space-1); }
.task-actions {
  display: flex;
  gap: var(--space-2);
  margin-top: var(--space-3);
  padding-top: var(--space-3);
  border-top: 1px solid var(--border-default);
}
</style>
