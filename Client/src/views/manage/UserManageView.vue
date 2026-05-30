<template>
  <div class="user-manage">
    <div class="page-header">
      <h2>权限管理</h2>
      <p class="subtitle">审核注册请求，管理用户角色与队伍分配</p>
    </div>

    <el-tabs v-model="activeTab" @tab-change="onTabChange">
      <!-- Tab 1: Pending users -->
      <el-tab-pane label="待审核用户" name="pending">
        <el-card class="section-card" shadow="hover">
          <template #header>
            <div class="card-header">
              <span class="card-title">待审核用户</span>
              <el-button size="small" @click="fetchPending" :loading="pendingLoading">刷新</el-button>
            </div>
          </template>
          <el-table :data="pendingUsers" v-loading="pendingLoading" empty-text="暂无待审核用户" stripe>
            <el-table-column prop="uid" label="UID" width="70" />
            <el-table-column prop="username" label="用户名" width="140" />
            <el-table-column prop="email" label="邮箱" min-width="200" />
            <el-table-column prop="role" label="当前角色" width="100">
              <template #default="{ row }">
                <el-tag :type="row.role === 1 ? 'success' : 'info'" size="small">{{ roleLabel(row.role) }}</el-tag>
              </template>
            </el-table-column>
            <el-table-column prop="belong_team_id" label="队伍ID" width="90" />
            <el-table-column label="操作" width="200" fixed="right">
              <template #default="{ row }">
                <el-button type="primary" size="small" @click="openApprove(row)">通过</el-button>
                <el-popconfirm title="确定拒绝此注册申请？该用户将被永久删除。" @confirm="handleReject(row.uid)">
                  <template #reference>
                    <el-button type="danger" size="small">拒绝</el-button>
                  </template>
                </el-popconfirm>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-tab-pane>

      <!-- Tab 2: All active users -->
      <el-tab-pane label="全部用户" name="all">
        <el-card class="section-card" shadow="hover">
          <template #header>
            <div class="card-header">
              <span class="card-title">已激活用户（共 {{ allUsers.length }} 人）</span>
              <el-button size="small" @click="fetchAll" :loading="allLoading">刷新</el-button>
            </div>
          </template>
          <el-table :data="allUsers" v-loading="allLoading" empty-text="暂无用户" stripe>
            <el-table-column prop="uid" label="UID" width="70" />
            <el-table-column prop="username" label="用户名" width="140" />
            <el-table-column prop="email" label="邮箱" min-width="200" />
            <el-table-column prop="role" label="角色" width="100">
              <template #default="{ row }">
                <el-tag :type="row.role === 2 ? 'primary' : row.role === 1 ? 'success' : 'info'" size="small">
                  {{ roleLabel(row.role) }}
                </el-tag>
              </template>
            </el-table-column>
            <el-table-column prop="belong_team_id" label="队伍ID" width="90" />
            <el-table-column label="操作" width="120" fixed="right">
              <template #default="{ row }">
                <el-button type="warning" size="small" @click="openEdit(row)" :disabled="row.role === 2">编辑</el-button>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-tab-pane>
    </el-tabs>

    <!-- Approve dialog -->
    <el-dialog v-model="approveVisible" title="审批通过 — 设置角色与队伍" width="460px" :close-on-click-modal="false">
      <el-form :model="approveForm" label-width="100px">
        <el-form-item label="用户名"><span>{{ approveForm.username }}</span></el-form-item>
        <el-form-item label="邮箱"><span>{{ approveForm.email }}</span></el-form-item>
        <el-form-item label="角色" required>
          <el-select v-model="approveForm.role" style="width: 100%">
            <el-option label="队员" :value="0" />
            <el-option label="队长" :value="1" />
          </el-select>
        </el-form-item>
        <el-form-item label="所属队伍ID">
          <el-input-number v-model="approveForm.belong_team_id" :min="0" style="width: 100%" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="approveVisible = false">取消</el-button>
        <el-button type="primary" @click="handleApprove" :loading="approveLoading">确认通过</el-button>
      </template>
    </el-dialog>

    <!-- Edit dialog -->
    <el-dialog v-model="editVisible" title="编辑用户角色与队伍" width="460px" :close-on-click-modal="false">
      <el-form :model="editForm" label-width="100px">
        <el-form-item label="用户名"><span>{{ editForm.username }}</span></el-form-item>
        <el-form-item label="邮箱"><span>{{ editForm.email }}</span></el-form-item>
        <el-form-item label="角色" required>
          <el-select v-model="editForm.role" style="width: 100%">
            <el-option label="队员" :value="0" />
            <el-option label="队长" :value="1" />
          </el-select>
        </el-form-item>
        <el-form-item label="所属队伍ID">
          <el-input-number v-model="editForm.belong_team_id" :min="0" style="width: 100%" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="editVisible = false">取消</el-button>
        <el-button type="primary" @click="handleSetRole" :loading="editLoading">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { listPendingUsers, approveUser, rejectUser, setUserRole, listAllUsers } from '@/api/user'

const ROLE_LABELS = { 0: '队员', 1: '队长', 2: '教练' }
function roleLabel(r) { return ROLE_LABELS[r] || '队员' }

const activeTab = ref('pending')

// Pending
const pendingUsers = ref([])
const pendingLoading = ref(false)

// All users
const allUsers = ref([])
const allLoading = ref(false)

// Approve
const approveVisible = ref(false)
const approveLoading = ref(false)
const approveForm = ref({ uid: 0, username: '', email: '', role: 0, belong_team_id: 0 })

// Edit
const editVisible = ref(false)
const editLoading = ref(false)
const editForm = ref({ uid: 0, username: '', email: '', role: 0, belong_team_id: 0 })

function onTabChange(name) {
  if (name === 'all') fetchAll()
  else fetchPending()
}

async function fetchPending() {
  pendingLoading.value = true
  try {
    const res = await listPendingUsers()
    if (res.error === 0) pendingUsers.value = res.users || []
    else ElMessage.error('获取待审核列表失败')
  } catch { ElMessage.error('网络请求失败') }
  finally { pendingLoading.value = false }
}

async function fetchAll() {
  allLoading.value = true
  try {
    const res = await listAllUsers()
    if (res.error === 0) allUsers.value = res.users || []
    else ElMessage.error('获取用户列表失败')
  } catch { ElMessage.error('网络请求失败') }
  finally { allLoading.value = false }
}

function openApprove(row) {
  approveForm.value = { uid: row.uid, username: row.username, email: row.email, role: 0, belong_team_id: 0 }
  approveVisible.value = true
}

async function handleApprove() {
  approveLoading.value = true
  try {
    const res = await approveUser({ uid: approveForm.value.uid, role: approveForm.value.role, belong_team_id: approveForm.value.belong_team_id })
    if (res.error === 0) { ElMessage.success('已通过审批'); approveVisible.value = false; await fetchPending() }
    else ElMessage.error('审批操作失败')
  } catch { ElMessage.error('网络请求失败') }
  finally { approveLoading.value = false }
}

async function handleReject(uid) {
  try {
    const res = await rejectUser(uid)
    if (res.error === 0) { ElMessage.success('已拒绝并删除'); await fetchPending() }
    else ElMessage.error('操作失败')
  } catch { ElMessage.error('网络请求失败') }
}

function openEdit(row) {
  editForm.value = { uid: row.uid, username: row.username, email: row.email, role: row.role, belong_team_id: row.belong_team_id }
  editVisible.value = true
}

async function handleSetRole() {
  editLoading.value = true
  try {
    const res = await setUserRole({ uid: editForm.value.uid, role: editForm.value.role, belong_team_id: editForm.value.belong_team_id })
    if (res.error === 0) { ElMessage.success('已更新'); editVisible.value = false; await fetchAll() }
    else ElMessage.error('操作失败')
  } catch { ElMessage.error('网络请求失败') }
  finally { editLoading.value = false }
}

onMounted(fetchPending)
</script>

<style scoped>
.user-manage { padding: 0; }
.page-header { margin-bottom: 24px; }
.page-header h2 { margin: 0 0 4px; font-size: 20px; }
.subtitle { color: var(--text-secondary); font-size: 13px; margin: 0; }
.section-card { margin-bottom: 24px; }
.card-header { display: flex; align-items: center; justify-content: space-between; }
.card-title { font-weight: 600; }

:deep(.el-tabs__item) { font-weight: 500; letter-spacing: 0.2px; }
:deep(.el-tabs__active-bar) { background: var(--gradient-primary); height: 3px; border-radius: 2px; }
</style>
