<template>
  <div class="page-container">
    <div class="page-header">
      <h2>工作台</h2>
      <p>欢迎回来，{{ userStore.username }}</p>
    </div>

    <el-row :gutter="20">
      <el-col :span="8">
        <el-card shadow="hover" class="stat-card" @click="$router.push('/taskboard')">
          <template #header>
            <div class="card-header">
              <span>训练任务概览</span>
              <el-button type="primary" link>查看任务看板 →</el-button>
            </div>
          </template>
          <div v-if="taskLoading" class="loading-placeholder">加载中...</div>
          <div v-else class="stat-grid">
            <div class="stat-item pending">
              <span class="stat-num">{{ taskStats.pending }}</span>
              <span class="stat-label">待处理</span>
            </div>
            <div class="stat-item progress">
              <span class="stat-num">{{ taskStats.inProgress }}</span>
              <span class="stat-label">进行中</span>
            </div>
            <div class="stat-item done">
              <span class="stat-num">{{ taskStats.completed }}</span>
              <span class="stat-label">已完成</span>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="8">
        <el-card shadow="hover" class="stat-card" @click="$router.push('/todolist')">
          <template #header>
            <div class="card-header">
              <span>TODO 清单</span>
              <el-button type="primary" link>查看 TODO →</el-button>
            </div>
          </template>
          <div v-if="todoLoading" class="loading-placeholder">加载中...</div>
          <div v-else class="stat-grid">
            <div class="stat-item progress">
              <span class="stat-num">{{ todoStats.active }}</span>
              <span class="stat-label">待完成</span>
            </div>
            <div class="stat-item done">
              <span class="stat-num">{{ todoStats.done }}</span>
              <span class="stat-label">已完成</span>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="8">
        <el-card shadow="hover" class="stat-card">
          <template #header>
            <div class="card-header">
              <span>每日打卡</span>
              <el-button type="primary" link @click.stop="$router.push('/checkin')">查看打卡详情 →</el-button>
            </div>
          </template>
          <div v-if="checkinLoading" class="loading-placeholder">加载中...</div>
          <div v-else class="checkin-area">
            <div v-if="checkedInToday" class="checked-in-state">
              <el-icon :size="36" color="var(--color-success)"><CircleCheckFilled /></el-icon>
              <span style="font-size:16px; color:var(--color-success); font-weight:600; margin:8px 0">今日已打卡</span>
            </div>
            <div v-else>
              <el-button type="success" :loading="checking" @click.stop="handleDashboardCheckin" style="width:100%">
                <el-icon :size="18"><Check /></el-icon>
                <span style="margin-left:6px">今日打卡</span>
              </el-button>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { listTasks, listTodo } from '@/api/task'
import { doCheckin, getCheckins } from '@/api/checkin'
import { ElMessage } from 'element-plus'
import { Check, CircleCheckFilled } from '@element-plus/icons-vue'

const userStore = useUserStore()
const taskLoading = ref(true)
const todoLoading = ref(true)
const checkinLoading = ref(true)
const checking = ref(false)
const checkedInToday = ref(false)

const taskStats = reactive({ pending: 0, inProgress: 0, completed: 0 })
const todoStats = reactive({ active: 0, done: 0 })

onMounted(async () => {
  try {
    const uid = parseInt(userStore.uid) || 0
    const tasksRes = await listTasks({ uid, status: -1, assigned_to: '0' })
    if (tasksRes.error === 0 && tasksRes.tasks) {
      for (const t of tasksRes.tasks) {
        // Only count tasks assigned to me, not tasks I created for others
        const assignedUids = String(t.assigned_to || '0').split(',').map(Number)
        if (!assignedUids.includes(uid)) continue
        const s = t.my_status ?? t.status
        if (s === 0) { taskStats.pending++; }
        else if (s === 1) { taskStats.inProgress++; }
        else if (s === 2) { taskStats.completed++; }
      }
    }
  } catch (e) {
    // silently fail — dashboard is non-critical
  } finally {
    taskLoading.value = false
  }

  try {
    const uid = parseInt(userStore.uid) || 0
    const todoRes = await listTodo({ uid, is_finished: 0 })
    if (todoRes.error === 0) {
      if (todoRes.todos) {
        for (const t of todoRes.todos) {
          if (t.is_finished === 1) { todoStats.done++; }
          else { todoStats.active++; }
        }
      }
    }
  } catch (e) {
    // silently fail
  } finally {
    todoLoading.value = false
  }

  // Check-in init
  try {
    const uid = parseInt(userStore.uid) || 0
    const today = new Date()
    const ds = `${today.getFullYear()}-${String(today.getMonth()+1).padStart(2,'0')}-${String(today.getDate()).padStart(2,'0')}`
    const res = await getCheckins({ uid, date_from: ds, date_to: ds })
    if (res.error === 0 && res.records && res.records.length > 0) {
      checkedInToday.value = true
    }
  } catch (e) { /* ignore */ }
  checkinLoading.value = false
})

async function handleDashboardCheckin() {
  checking.value = true
  try {
    const res = await doCheckin({ uid: parseInt(userStore.uid) || 0 })
    if (res.error === 0) {
      checkedInToday.value = true
      ElMessage.success('打卡成功！')
    } else if (res.error === 3001) {
      checkedInToday.value = true
      ElMessage.info('今日已打卡，无需重复操作')
    } else {
      ElMessage.error('打卡失败')
    }
  } catch (e) { ElMessage.error('网络错误') }
  checking.value = false
}
</script>

<style scoped>
.stat-card {
  cursor: pointer;
  transition: transform var(--transition-fast), box-shadow var(--transition-fast);
  border: 1px solid var(--border-light);
  overflow: hidden;
}
.stat-card:hover {
  transform: translateY(-3px);
  box-shadow: var(--shadow-lg);
}
.stat-card:first-child { border-top: 3px solid var(--color-primary); }
.stat-card:nth-child(2) { border-top: 3px solid var(--color-warning); }
.stat-card:nth-child(3) { border-top: 3px solid var(--color-success); }

.card-header { display: flex; justify-content: space-between; align-items: center; }
.card-header span { font-weight: 600; }

.stat-grid { display: flex; justify-content: space-around; text-align: center; }
.stat-item { padding: 10px; }
.stat-num { display: block; font-size: 30px; font-weight: 700; line-height: 1.2; letter-spacing: -0.5px; }
.stat-label { font-size: 12px; color: var(--text-secondary); margin-top: 4px; display: block; }
.stat-item.pending .stat-num { color: var(--color-info); }
.stat-item.progress .stat-num { color: var(--color-warning); }
.stat-item.done .stat-num { color: var(--color-success); }

.loading-placeholder { text-align: center; color: var(--text-secondary); padding: 28px 0; }
.checkin-area { text-align: center; padding: 8px 0; }
.checked-in-state { display: flex; flex-direction: column; align-items: center; }
</style>
