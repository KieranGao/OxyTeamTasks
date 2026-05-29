<template>
  <div class="page-container">
    <div class="page-header">
      <h2>个人中心</h2>
      <p>查看个人信息、训练统计与系统设置</p>
    </div>

    <el-row :gutter="20">
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>个人信息</template>
          <div class="info-item">
            <span class="label">用户名</span>
            <span class="value">{{ userStore.username }}</span>
          </div>
          <div class="info-item">
            <span class="label">邮箱</span>
            <span class="value">{{ userStore.email }}</span>
          </div>
          <div class="info-item">
            <span class="label">角色</span>
            <el-tag :type="roleTagColor" size="small">{{ userStore.roleLabel }}</el-tag>
          </div>
          <div class="info-item">
            <span class="label">所属队伍</span>
            <template v-if="editingTeam">
              <div class="team-edit-row">
                <el-input-number
                  v-model="teamForm.teamId"
                  :min="0"
                  placeholder="队伍 ID"
                  size="small"
                  style="width: 120px"
                />
                <el-button type="primary" size="small" @click="saveTeam">保存</el-button>
                <el-button size="small" @click="editingTeam = false">取消</el-button>
              </div>
            </template>
            <template v-else>
              <span class="value">
                {{ userStore.belongTeamId > 0 ? '队伍 ' + userStore.belongTeamId : '未分配' }}
              </span>
              <el-button
                v-if="userStore.isCoach"
                link
                type="primary"
                size="small"
                style="margin-left: 8px"
                @click="startEditTeam"
              >
                修改
              </el-button>
            </template>
          </div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>训练统计</template>
          <p style="color: var(--text-secondary)">统计功能开发中...</p>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>界面设置</template>
          <div class="setting-item">
            <span>深色模式</span>
            <el-switch
              :model-value="appStore.theme === 'dark'"
              @change="appStore.toggleTheme()"
            />
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { useUserStore } from '@/stores/user'
import { useAppStore } from '@/stores/app'
import { ElMessage } from 'element-plus'

const userStore = useUserStore()
const appStore = useAppStore()

// role: 0=队员(gray) 1=队长(green) 2=教练(red)
const roleTagColor = computed(() => {
  const map = { 0: 'info', 1: 'success', 2: 'danger' }
  return map[userStore.role] || 'info'
})

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
</script>

<style scoped>
.el-card:first-child { border-top: 3px solid var(--color-primary); }
.el-card:nth-child(2) { border-top: 3px solid var(--color-warning); }
.el-card:nth-child(3) { border-top: 3px solid var(--color-info); }

.info-item,
.setting-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 0;
  border-bottom: 1px solid var(--border-light);
  transition: background var(--transition-fast);
}

.info-item:last-child,
.setting-item:last-child {
  border-bottom: none;
}

.info-item:hover,
.setting-item:hover {
  background: var(--color-primary-bg);
  border-radius: var(--radius-sm);
  margin: 0 -8px;
  padding-left: 8px;
  padding-right: 8px;
}

.info-item .label {
  color: var(--text-secondary);
  font-size: 13px;
}

.info-item .value {
  color: var(--text-primary);
  font-weight: 500;
}

.team-edit-row {
  display: flex;
  align-items: center;
  gap: 6px;
}
</style>
