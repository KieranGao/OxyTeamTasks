<template>
  <div class="page-container">
    <div class="page-header">
      <h2>全队信息</h2>
      <p>查看所有队伍的成员构成</p>
    </div>

    <!-- Overall summary -->
    <el-row :gutter="16" class="overall-row" v-if="!loading">
      <el-col :span="12">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num">{{ teams.length }}</div>
          <div class="summary-label">队伍总数</div>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="hover" class="summary-card">
          <div class="summary-num">{{ overall.totalMembers }}</div>
          <div class="summary-label">队员总数</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- Per-team cards -->
    <div v-loading="loading">
      <el-empty v-if="!loading && teams.length === 0" description="暂无队伍数据" />
      <el-row :gutter="16">
        <el-col v-for="team in teams" :key="team.id" :xs="24" :md="12" :lg="8" style="margin-bottom:16px">
          <el-card shadow="hover" class="team-card">
            <template #header>
              <div class="team-header">
                <span class="team-name">队伍 {{ team.id }}</span>
                <el-tag size="small">{{ team.memberCount }} 人</el-tag>
              </div>
            </template>

            <div class="team-section" v-if="team.captain">
              <div class="section-title">队长</div>
              <el-tag type="warning" size="small">{{ team.captain.username }}</el-tag>
            </div>

            <el-divider v-if="team.captain && team.members.length" />

            <div class="team-section" v-if="team.members.length">
              <div class="section-title">队员 ({{ team.members.length }})</div>
              <div class="member-tags">
                <el-tag v-for="m in team.members" :key="m.uid" size="small" style="margin:2px"
                  :type="m.role === 2 ? 'danger' : m.role === 1 ? 'warning' : ''">
                  {{ m.username }}
                </el-tag>
              </div>
            </div>

            <el-empty v-if="!team.captain && !team.members.length" description="暂无成员" :image-size="40" />
          </el-card>
        </el-col>
      </el-row>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { listAllUsers } from '@/api/user'

const users = ref([])
const loading = ref(true)

const teams = computed(() => {
  const teamMap = {}
  for (const u of users.value) {
    if (!u.belong_team_id || u.belong_team_id <= 0) continue
    const tid = u.belong_team_id
    if (!teamMap[tid]) teamMap[tid] = { members: [] }
    teamMap[tid].members.push(u)
  }

  return Object.entries(teamMap).map(([tid, data]) => {
    const captain = data.members.find(u => u.role === 1)
    const regularMembers = data.members.filter(u => u.role !== 1)
    return {
      id: parseInt(tid),
      memberCount: data.members.length,
      captain: captain || null,
      members: regularMembers.sort((a, b) => b.role - a.role || a.uid - b.uid)
    }
  }).sort((a, b) => a.id - b.id)
})

const overall = computed(() => {
  let totalMembers = 0
  for (const t of teams.value) totalMembers += t.memberCount
  return { totalMembers }
})

onMounted(async () => {
  try {
    const res = await listAllUsers()
    if (res.error === 0 && res.users) {
      users.value = res.users.filter(u => u.status === 1)
    }
  } catch (e) { /* ignore */ }
  loading.value = false
})
</script>

<style scoped>
.overall-row { margin-bottom: var(--space-6); }

.summary-card {
  text-align: center;
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  background: var(--bg-surface);
}
.summary-card :deep(.el-card__body) { padding: var(--space-5) var(--space-4); }

.summary-num {
  font-family: var(--font-mono); font-weight: 700;
  font-size: var(--text-3xl); color: var(--text-primary);
  line-height: 1;
}

.summary-label {
  font-family: var(--font-mono); font-size: var(--text-xs);
  text-transform: uppercase; letter-spacing: 0.08em;
  color: var(--text-secondary); margin-top: var(--space-2);
}

.team-card {
  height: 100%;
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  background: var(--bg-surface);
}

.team-header { display: flex; justify-content: space-between; align-items: center; }

.team-name {
  font-family: var(--font-mono); font-weight: 700;
  font-size: var(--text-base);
}

.team-section { margin-bottom: var(--space-1); }

.section-title {
  font-family: var(--font-mono); font-size: var(--text-xs);
  text-transform: uppercase; letter-spacing: 0.08em;
  color: var(--text-secondary); margin-bottom: var(--space-2);
}

.member-tags { display: flex; flex-wrap: wrap; gap: var(--space-1); }
</style>
