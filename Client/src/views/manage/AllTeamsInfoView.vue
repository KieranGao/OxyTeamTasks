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
.overall-row { margin-bottom: 20px; }
.summary-card { text-align: center; }
.summary-num { font-size: 32px; font-weight: 700; }
.summary-label { font-size: 13px; color: var(--text-secondary); margin-top: 4px; }

.team-card { height: 100%; }
.team-header { display: flex; justify-content: space-between; align-items: center; }
.team-name { font-weight: 600; font-size: 15px; }
.team-section { margin-bottom: 4px; }
.section-title { font-size: 13px; color: var(--text-secondary); margin-bottom: 8px; }
.member-tags { display: flex; flex-wrap: wrap; gap: 2px; }
</style>
