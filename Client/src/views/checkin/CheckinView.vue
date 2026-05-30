<template>
  <div class="page-container">
    <div class="page-header">
      <h2>每日打卡</h2>
      <p>记录每日训练出勤，保持良好习惯</p>
    </div>

    <!-- Check-in button -->
    <el-card shadow="hover" class="checkin-card">
      <div v-if="checkedInToday" class="checked-in-state">
        <el-icon :size="48" color="var(--color-success)"><CircleCheckFilled /></el-icon>
        <p class="checked-text">今日已打卡</p>
        <p class="checked-sub">继续保持，明天再来！</p>
      </div>
      <div v-else>
        <el-button type="success" size="large" :loading="checking" @click="handleCheckin" class="checkin-btn">
          <el-icon :size="22"><Check /></el-icon>
          <span style="margin-left:8px">今日打卡</span>
        </el-button>
        <p style="margin-top:12px; color:var(--text-secondary)">点击打卡，记录今天的训练出勤</p>
      </div>
    </el-card>

    <!-- Calendar -->
    <el-card shadow="hover">
      <template #header>
        <div class="cal-header">
          <span>打卡日历</span>
          <div class="cal-nav">
            <el-button size="small" @click="prevMonth">&lt; 上月</el-button>
            <span class="cal-month-label">{{ yearMonth }}</span>
            <el-button size="small" @click="nextMonth">下月 &gt;</el-button>
          </div>
        </div>
      </template>
      <div class="cal-grid">
        <div class="cal-weekday" v-for="d in weekdays" :key="d">{{ d }}</div>
        <div
          v-for="cell in calendarCells"
          :key="cell.key"
          class="cal-cell"
          :class="{
            'cal-today': cell.isToday,
            'cal-checked': cell.checked,
            'cal-other-month': !cell.isCurrentMonth
          }"
        >
          <span class="cal-day-num">{{ cell.day }}</span>
          <el-icon v-if="cell.checked" color="var(--color-success)" :size="16"><Check /></el-icon>
        </div>
      </div>
      <div class="cal-legend">
        <span class="legend-dot checked"></span> 已打卡
        <span class="legend-dot unchecked"></span> 未打卡
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Check, CircleCheckFilled } from '@element-plus/icons-vue'
import { useUserStore } from '@/stores/user'
import { doCheckin, getCheckins } from '@/api/checkin'

const userStore = useUserStore()
const checking = ref(false)
const checkedInToday = ref(false)
const checkedDates = ref(new Set())
const currentYear = ref(new Date().getFullYear())
const currentMonth = ref(new Date().getMonth() + 1)

const weekdays = ['日', '一', '二', '三', '四', '五', '六']

const yearMonth = computed(() => `${currentYear.value}年${currentMonth.value}月`)

function pad(n) { return String(n).padStart(2, '0') }
function dateStr(y, m, d) { return `${y}-${pad(m)}-${pad(d)}` }
function todayDateStr() {
  const now = new Date()
  return dateStr(now.getFullYear(), now.getMonth() + 1, now.getDate())
}

const calendarCells = computed(() => {
  const firstDay = new Date(currentYear.value, currentMonth.value - 1, 1)
  const startDow = firstDay.getDay()
  const lastDay = new Date(currentYear.value, currentMonth.value, 0)
  const totalDays = lastDay.getDate()
  const today = todayDateStr()
  const cells = []

  const prevLast = new Date(currentYear.value, currentMonth.value - 1, 0).getDate()
  for (let i = startDow - 1; i >= 0; i--) {
    const d = prevLast - i
    cells.push({
      key: `prev-${d}`, day: d, isCurrentMonth: false, isToday: false,
      checked: checkedDates.value.has(dateStr(currentYear.value, currentMonth.value - 1, d))
    })
  }
  for (let d = 1; d <= totalDays; d++) {
    const ds = dateStr(currentYear.value, currentMonth.value, d)
    cells.push({
      key: `cur-${d}`, day: d, isCurrentMonth: true,
      isToday: ds === today,
      checked: checkedDates.value.has(ds)
    })
  }
  const remaining = 7 - (cells.length % 7)
  if (remaining < 7) {
    for (let d = 1; d <= remaining; d++) {
      cells.push({
        key: `next-${d}`, day: d, isCurrentMonth: false, isToday: false,
        checked: checkedDates.value.has(dateStr(currentYear.value, currentMonth.value + 1, d))
      })
    }
  }
  return cells
})

function prevMonth() {
  if (currentMonth.value === 1) { currentYear.value--; currentMonth.value = 12 }
  else currentMonth.value--
  loadMonthCheckins()
}

function nextMonth() {
  if (currentMonth.value === 12) { currentYear.value++; currentMonth.value = 1 }
  else currentMonth.value++
  loadMonthCheckins()
}

async function loadMonthCheckins() {
  const from = dateStr(currentYear.value, currentMonth.value, 1)
  const lastDay = new Date(currentYear.value, currentMonth.value, 0).getDate()
  const to = dateStr(currentYear.value, currentMonth.value, lastDay)
  try {
    const res = await getCheckins({ uid: parseInt(userStore.uid) || 0, date_from: from, date_to: to })
    if (res.error === 0 && res.records) {
      checkedDates.value = new Set(res.records.map(r => r.checkin_date))
    }
  } catch (e) { /* ignore */ }
}

async function handleCheckin() {
  checking.value = true
  try {
    const res = await doCheckin({ uid: parseInt(userStore.uid) || 0 })
    if (res.error === 0) {
      checkedInToday.value = true
      checkedDates.value.add(todayDateStr())
      ElMessage.success('打卡成功！')
    } else if (res.error === 3001) {
      checkedInToday.value = true
      checkedDates.value.add(todayDateStr())
      ElMessage.info('今日已打卡，无需重复操作')
    } else {
      ElMessage.error('打卡失败，请稍后重试')
    }
  } catch (e) { ElMessage.error('网络错误，打卡失败') }
  checking.value = false
}

async function initToday() {
  const today = todayDateStr()
  try {
    const res = await getCheckins({ uid: parseInt(userStore.uid) || 0, date_from: today, date_to: today })
    if (res.error === 0 && res.records && res.records.length > 0) {
      checkedInToday.value = true
    }
  } catch (e) { /* ignore */ }
}

onMounted(async () => {
  await initToday()
  await loadMonthCheckins()
})
</script>

<style scoped>
.checkin-card { margin-bottom: 20px; text-align: center; border-top: 4px solid var(--color-success); overflow: visible; }
.checked-in-state { padding: 24px 0; }
.checked-text { font-size: 20px; margin: 12px 0 4px; color: var(--color-success); font-weight: 700; }
.checked-sub { color: var(--text-secondary); font-size: 13px; }
.checkin-btn { padding: 16px 56px; font-size: 18px; border-radius: var(--radius-lg); font-weight: 600; letter-spacing: 1px; animation: pulse-glow 2s infinite; }

@keyframes pulse-glow {
  0%, 100% { box-shadow: 0 0 0 0 rgba(34, 197, 94, 0.35); }
  50% { box-shadow: 0 0 0 12px rgba(34, 197, 94, 0); }
}

.cal-header { display: flex; justify-content: space-between; align-items: center; }
.cal-nav { display: flex; align-items: center; }
.cal-month-label { font-weight: 700; margin: 0 16px; min-width: 110px; text-align: center; font-size: 15px; }

.cal-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 6px; }
.cal-weekday { text-align: center; font-weight: 600; font-size: 12px; padding: 10px 0; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 1px; }
.cal-cell {
  text-align: center; padding: 10px 4px; border-radius: var(--radius-sm);
  min-height: 56px; display: flex; flex-direction: column; align-items: center; justify-content: center;
  background: var(--bg-secondary);
  border: 1px solid transparent;
  transition: all var(--transition-fast);
}
.cal-cell:hover { transform: translateY(-2px); box-shadow: var(--shadow-sm); }
.cal-day-num { font-size: 14px; margin-bottom: 3px; font-weight: 500; }
.cal-cell.cal-other-month .cal-day-num { color: var(--text-secondary); opacity: 0.25; }
.cal-cell.cal-today { border: 2px solid var(--color-primary); box-shadow: 0 0 0 3px var(--color-primary-bg); }
.cal-cell.cal-checked { background: var(--color-success-bg); border-color: rgba(34, 197, 94, 0.2); }

.cal-legend { margin-top: 16px; display: flex; gap: 20px; align-items: center; justify-content: center; font-size: 13px; color: var(--text-secondary); }
.legend-dot { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 4px; }
.legend-dot.checked { background: var(--color-success); box-shadow: 0 2px 6px rgba(34, 197, 94, 0.3); }
.legend-dot.unchecked { background: var(--bg-secondary); border: 1px solid var(--border-light); }
</style>
