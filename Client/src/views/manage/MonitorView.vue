<template>
  <div class="monitor">
    <div class="page-header">
      <h2>系统监控</h2>
      <p class="subtitle">查看各微服务运行状态与系统日志</p>
    </div>

    <!-- Server Status -->
    <el-card class="section-card" shadow="hover">
      <template #header>
        <div class="card-header">
          <span class="card-title">服务器状态</span>
          <span class="auto-refresh">每 10 秒自动刷新</span>
        </div>
      </template>
      <el-row :gutter="16" v-loading="statusLoading">
        <el-col :span="8" v-for="srv in servers" :key="srv.service" style="margin-bottom: 16px">
          <el-card shadow="hover" class="server-card">
            <div class="server-header">
              <span class="server-name">{{ srv.service }}</span>
              <el-tag :type="srv.status === 'online' ? 'success' : 'danger'" size="small" effect="dark">
                {{ srv.status === 'online' ? '在线' : '离线' }}
              </el-tag>
            </div>
            <div class="server-info">
              <p>{{ srv.host }}:{{ srv.port }}</p>
              <p v-if="srv.service && srv.service.startsWith('PushServer')" class="conn-count">
                当前连接: <strong>{{ srv.connections ?? 0 }}</strong>
              </p>
              <p class="heartbeat-time" v-if="srv.last_heartbeat > 0">
                最后心跳: {{ formatTime(srv.last_heartbeat) }}
              </p>
              <p class="heartbeat-time" v-else>暂无心跳</p>
            </div>
          </el-card>
        </el-col>
        <el-col :span="24" v-if="servers.length === 0 && !statusLoading">
          <el-empty description="暂无服务器状态数据" />
        </el-col>
      </el-row>
    </el-card>

    <!-- System Logs -->
    <el-card class="section-card" shadow="hover">
      <template #header>
        <div class="card-header">
          <span class="card-title">系统日志</span>
          <div class="log-filters">
            <el-select v-model="filterService" placeholder="全部服务" clearable size="small" style="width: 140px">
              <el-option label="全部服务" value="" />
              <el-option label="GateServer" value="GateServer" />
              <el-option label="UMSServer" value="UMSServer" />
              <el-option label="StatusServer" value="StatusServer" />
              <el-option label="PushServer" value="PushServer" />
              <el-option label="TaskServer" value="TaskServer" />
              <el-option label="MailerServer" value="MailerServer" />
            </el-select>
            <el-select v-model="filterLevel" placeholder="全部级别" clearable size="small" style="width: 120px; margin-left: 8px">
              <el-option label="全部级别" value="" />
              <el-option label="INFO" value="INFO" />
              <el-option label="WARN" value="WARN" />
              <el-option label="ERROR" value="ERROR" />
            </el-select>
            <el-button size="small" @click="fetchLogs" :loading="logsLoading" style="margin-left: 8px">刷新</el-button>
          </div>
        </div>
      </template>
      <el-table :data="logs" v-loading="logsLoading" stripe max-height="400">
        <el-table-column label="时间" width="180">
          <template #default="{ row }">
            {{ formatTime(row.timestamp) }}
          </template>
        </el-table-column>
        <el-table-column prop="service" label="服务" width="120">
          <template #default="{ row }">
            <el-tag size="small">{{ row.service }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="level" label="级别" width="80">
          <template #default="{ row }">
            <el-tag :type="levelColor(row.level)" size="small">{{ row.level }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="message" label="消息" min-width="300" show-overflow-tooltip />
      </el-table>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { queryLogs, queryServerStatus } from '@/api/user'

const servers = ref([])
const statusLoading = ref(false)
const logs = ref([])
const logsLoading = ref(false)
const filterService = ref('')
const filterLevel = ref('')
let statusTimer = null

function levelColor(level) {
  const map = { DEBUG: 'info', INFO: '', WARN: 'warning', ERROR: 'danger' }
  return map[level] || 'info'
}

function formatTime(ts) {
  if (!ts) return '-'
  const d = new Date(ts * 1000)
  const pad = n => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

async function fetchServerStatus() {
  statusLoading.value = true
  try {
    const res = await queryServerStatus()
    if (res.error === 0) servers.value = res.servers || []
  } catch { /* silent */ }
  finally { statusLoading.value = false }
}

async function fetchLogs() {
  logsLoading.value = true
  try {
    const res = await queryLogs({ service: filterService.value, level: filterLevel.value, limit: 100 })
    if (res.error === 0) logs.value = res.entries || []
    else ElMessage.error('日志查询失败')
  } catch { ElMessage.error('网络请求失败') }
  finally { logsLoading.value = false }
}

onMounted(() => {
  fetchServerStatus()
  fetchLogs()
  statusTimer = setInterval(fetchServerStatus, 10000)
})

onUnmounted(() => {
  if (statusTimer) clearInterval(statusTimer)
})
</script>

<style scoped>
.monitor { padding: 0; }
.page-header { margin-bottom: 24px; }
.page-header h2 { margin: 0 0 4px; font-size: 20px; }
.subtitle { color: var(--text-secondary); font-size: 13px; margin: 0; }
.section-card { margin-bottom: 24px; }
.card-header { display: flex; align-items: center; justify-content: space-between; }
.card-title { font-weight: 600; }
.auto-refresh { font-size: 12px; color: var(--text-secondary); }
.log-filters { display: flex; align-items: center; }
.server-card {
  border-top: 4px solid var(--color-primary);
  transition: all var(--transition-fast);
  overflow: hidden;
}
.server-card:hover { transform: translateY(-3px); box-shadow: var(--shadow-md); }
.server-card .server-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; }
.server-card .server-name { font-weight: 700; font-size: 15px; }
.server-info p { margin: 5px 0; font-size: 13px; color: var(--text-secondary); }
.conn-count { font-size: 13px; color: var(--text-primary); margin: 5px 0; font-weight: 600; }
.heartbeat-time { font-size: 12px; color: var(--text-secondary); }
</style>
