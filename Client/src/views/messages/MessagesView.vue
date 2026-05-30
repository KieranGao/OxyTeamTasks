<template>
  <div class="page-container">
    <div class="page-header">
      <h2>消息中心</h2>
      <p>查看实时消息、未读消息和历史消息</p>
    </div>

    <el-card shadow="hover">
      <template #header>
        <div class="msg-header">
          <span>消息列表 <el-tag v-if="unreadCount > 0" type="danger" size="small" style="margin-left:8px">{{ unreadCount }} 条未读</el-tag></span>
          <el-button v-if="unreadCount > 0" type="primary" link size="small" @click="handleMarkAllRead">全部已读</el-button>
        </div>
      </template>

      <el-table :data="messages" v-loading="loading" empty-text="暂无消息" stripe>
        <el-table-column label="类型" width="110">
          <template #default="{ row }">
            <el-tag size="small" :type="msgTypeColor(row.type)">{{ msgTypeLabel(row.type) }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="title" label="标题" min-width="200" show-overflow-tooltip>
          <template #default="{ row }">
            <span :class="{ 'msg-unread': row.is_read === 0 }">{{ row.title }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="created_at" label="时间" width="170" />
        <el-table-column label="状态" width="80">
          <template #default="{ row }">
            <el-tag v-if="row.is_read === 0" type="danger" size="small">未读</el-tag>
            <el-tag v-else type="info" size="small">已读</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="140" fixed="right">
          <template #default="{ row }">
            <el-button v-if="row.is_read === 0" type="primary" link size="small" @click="handleMarkRead(row)">标记已读</el-button>
            <el-button type="danger" link size="small" @click="handleDelete(row)">删除</el-button>
          </template>
        </el-table-column>
      </el-table>

      <div class="msg-pagination" v-if="total > pageSize">
        <el-pagination
          v-model:current-page="currentPage"
          :page-size="pageSize"
          :total="total"
          layout="prev, pager, next"
          @current-change="loadMessages"
        />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { getMessages, markRead, deleteMessage } from '@/api/message'
import { useUserStore } from '@/stores/user'
import { ElMessage, ElMessageBox } from 'element-plus'

const userStore = useUserStore()
const messages = ref([])
const loading = ref(false)
const unreadCount = ref(0)
const total = ref(0)
const currentPage = ref(1)
const pageSize = 20

function msgTypeColor(type) {
  const map = {
    task_new: 'primary', task_update: 'warning', task_done: 'success',
    task_remind: 'danger', checkin_remind: 'success', kicked: 'danger',
    user_register: 'warning'
  }
  return map[type] || 'info'
}

function msgTypeLabel(type) {
  const map = {
    task_new: '新任务', task_update: '任务变更', task_done: '任务完成',
    task_remind: '截止提醒', checkin_remind: '打卡提醒', kicked: '下线通知',
    user_register: '注册审核'
  }
  return map[type] || '通知'
}

async function loadMessages() {
  loading.value = true
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await getMessages({ uid, page: currentPage.value, page_size: pageSize })
    if (res.error === 0) {
      messages.value = res.messages || []
      unreadCount.value = res.unread_count || 0
      total.value = res.total || 0
    }
  } catch (e) { /* ignore */ }
  loading.value = false
}

async function handleMarkRead(row) {
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await markRead({ uid, ids: [row.id] })
    if (res.error === 0) { row.is_read = 1; unreadCount.value = Math.max(0, unreadCount.value - 1) }
  } catch (e) { ElMessage.error('操作失败') }
}

async function handleMarkAllRead() {
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await markRead({ uid, ids: [] })
    if (res.error === 0) {
      messages.value.forEach(m => m.is_read = 1)
      unreadCount.value = 0
      ElMessage.success('全部已读')
    }
  } catch (e) { ElMessage.error('操作失败') }
}

async function handleDelete(row) {
  try {
    await ElMessageBox.confirm('确定删除该消息？', '确认删除', { type: 'warning' })
  } catch { return }
  try {
    const uid = parseInt(userStore.uid) || 0
    const res = await deleteMessage({ uid, ids: [row.id] })
    if (res.error === 0) { ElMessage.success('已删除'); loadMessages() }
  } catch (e) { ElMessage.error('删除失败') }
}

onMounted(loadMessages)
</script>

<style scoped>
.msg-header { display: flex; justify-content: space-between; align-items: center; }
.msg-unread { font-weight: 600; color: var(--color-primary); }
.msg-pagination { margin-top: 20px; display: flex; justify-content: center; }

:deep(.el-table) { border-radius: var(--radius-md); overflow: hidden; }
:deep(.el-table th.el-table__cell) { font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
:deep(.el-table .el-table__row) { transition: background var(--transition-fast); }
:deep(.el-table .el-table__row:hover > td) { background: var(--color-primary-bg) !important; }
</style>
