<template>
  <div class="page-container">
    <div class="page-header">
      <h2>TODO List</h2>
      <p>管理个人待办事项，设置优先级与截止时间</p>
    </div>

    <!-- Add form -->
    <div class="add-bar">
      <el-input v-model="newTodo.content" placeholder="新增待办..." class="add-input" @keyup.enter="handleAdd" />
      <el-select v-model="newTodo.priority" style="width:100px">
        <el-option :value="1" label="高" />
        <el-option :value="2" label="中" />
        <el-option :value="3" label="低" />
      </el-select>
      <el-date-picker
        v-model="newTodo.deadline" type="date" placeholder="截止日期"
        value-format="YYYY-MM-DD" style="width:160px"
      />
      <el-button type="primary" @click="handleAdd">添加</el-button>
    </div>

    <!-- Filters -->
    <div class="filter-bar">
      <el-radio-group v-model="filter" @change="loadTodos">
        <el-radio-button :value="0">全部</el-radio-button>
        <el-radio-button :value="2">未完成 ({{ counts.active }})</el-radio-button>
        <el-radio-button :value="1">已完成 ({{ counts.done }})</el-radio-button>
      </el-radio-group>
    </div>

    <!-- Todo List -->
    <el-card shadow="hover">
      <el-empty v-if="sortedTodos.length === 0" description="暂无待办事项" />
      <div v-else class="todo-list">
        <div
          v-for="todo in sortedTodos" :key="todo.id"
          class="todo-item" :class="{ finished: todo.is_finished === 1 }"
        >
          <el-checkbox
            :model-value="todo.is_finished === 1"
            @change="toggleFinish(todo)"
            size="large"
          />
          <span class="todo-content">{{ todo.content }}</span>
          <el-tag size="small" :type="priorityType(todo.priority)">{{ priorityLabel(todo.priority) }}</el-tag>
          <span v-if="todo.deadline" class="todo-deadline">{{ todo.deadline.slice(0,10) }}</span>
          <el-button type="primary" link size="small" @click="openEdit(todo)">
            <el-icon><Edit /></el-icon>
          </el-button>
          <el-button type="danger" link size="small" @click="handleDelete(todo)">
            <el-icon><Delete /></el-icon>
          </el-button>
        </div>
      </div>
    </el-card>

    <!-- Edit Dialog -->
    <el-dialog v-model="editVisible" title="编辑待办" width="420px">
      <el-form :model="editForm" label-width="80px">
        <el-form-item label="内容">
          <el-input v-model="editForm.content" />
        </el-form-item>
        <el-form-item label="优先级">
          <el-select v-model="editForm.priority" style="width:100%">
            <el-option :value="1" label="高" />
            <el-option :value="2" label="中" />
            <el-option :value="3" label="低" />
          </el-select>
        </el-form-item>
        <el-form-item label="截止日期">
          <el-date-picker v-model="editForm.deadline" type="date" value-format="YYYY-MM-DD" style="width:100%" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="editVisible = false">取消</el-button>
        <el-button type="primary" @click="saveEdit">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { useUserStore } from '@/stores/user'
import { addTodo, listTodo, updateTodo, deleteTodo } from '@/api/task'
import { ElMessage, ElMessageBox } from 'element-plus'

const userStore = useUserStore()
const todos = ref([])
const filter = ref(0)
const newTodo = reactive({ content: '', priority: 3, deadline: '' })
const editVisible = ref(false)
const editForm = reactive({ id: 0, content: '', priority: 3, deadline: '' })

const counts = computed(() => {
  let active = 0, done = 0
  for (const t of todos.value) {
    if (t.is_finished === 1) { done++; } else { active++; }
  }
  return { active, done }
})

const sortedTodos = computed(() => {
  let list = todos.value
  if (filter.value !== 0) list = list.filter(t => t.is_finished === filter.value)
  return list.slice().sort((a, b) => {
    if (a.priority !== b.priority) return a.priority - b.priority
    if (a.deadline && b.deadline) return a.deadline.localeCompare(b.deadline)
    if (a.deadline) return -1
    if (b.deadline) return 1
    return 0
  })
})

function priorityType(p) { return { 1: 'danger', 2: 'warning', 3: 'info' }[p] || 'info' }
function priorityLabel(p) { return { 1: '高', 2: '中', 3: '低' }[p] || '低' }

async function loadTodos() {
  const uid = parseInt(userStore.uid) || 0
  try {
    const res = await listTodo({ uid, is_finished: 0 })
    if (res.error === 0) todos.value = res.todos || []
  } catch (e) { todos.value = [] }
}

async function handleAdd() {
  if (!newTodo.content.trim()) { ElMessage.warning('请输入待办内容'); return }
  const uid = parseInt(userStore.uid) || 0
  try {
    const res = await addTodo({ uid, content: newTodo.content.trim(), priority: newTodo.priority, deadline: newTodo.deadline || '' })
    if (res.error === 0) { ElMessage.success('已添加'); newTodo.content = ''; newTodo.deadline = ''; loadTodos() }
    else ElMessage.error('添加失败')
  } catch (e) { ElMessage.error('操作失败') }
}

async function toggleFinish(todo) {
  const uid = parseInt(userStore.uid) || 0
  const newVal = todo.is_finished === 1 ? 2 : 1
  try {
    const res = await updateTodo({ id: todo.id, uid, content: todo.content, priority: todo.priority, deadline: todo.deadline || '', is_finished: newVal })
    if (res.error === 0) loadTodos()
    else ElMessage.error('操作失败')
  } catch (e) { ElMessage.error('操作失败') }
}

function openEdit(todo) {
  editForm.id = todo.id
  editForm.content = todo.content
  editForm.priority = todo.priority
  editForm.deadline = todo.deadline ? todo.deadline.slice(0, 10) : ''
  editVisible.value = true
}

async function handleDelete(todo) {
  try {
    await ElMessageBox.confirm(`确定删除待办 "${todo.content}"？`, '确认删除', { type: 'warning' })
  } catch { return }
  const uid = parseInt(userStore.uid) || 0
  try {
    const res = await deleteTodo({ id: todo.id, uid })
    if (res.error === 0) { ElMessage.success('已删除'); loadTodos() }
    else ElMessage.error('删除失败')
  } catch (e) { ElMessage.error('操作失败') }
}

async function saveEdit() {
  const uid = parseInt(userStore.uid) || 0
  const todo = todos.value.find(t => t.id === editForm.id)
  try {
    const res = await updateTodo({
      id: editForm.id, uid,
      content: editForm.content, priority: editForm.priority,
      deadline: editForm.deadline || '', is_finished: todo?.is_finished || 2
    })
    if (res.error === 0) { ElMessage.success('已保存'); editVisible.value = false; loadTodos() }
    else ElMessage.error('保存失败')
  } catch (e) { ElMessage.error('操作失败') }
}

onMounted(loadTodos)
</script>

<style scoped>
.add-bar {
  display: flex; gap: 10px; margin-bottom: 20px;
  padding: 16px 20px;
  background: var(--glass-bg);
  backdrop-filter: var(--glass-blur);
  border-radius: var(--radius-lg);
  border: 1px solid var(--border-light);
  box-shadow: var(--shadow-sm);
}
.add-input { flex: 1; }
.filter-bar { margin-bottom: 20px; }
.todo-list { display: flex; flex-direction: column; }
.todo-item {
  display: flex; align-items: center; gap: 12px;
  padding: 14px 12px;
  border-bottom: 1px solid var(--border-light);
  transition: all var(--transition-fast);
  border-radius: var(--radius-sm);
}
.todo-item:last-child { border-bottom: none; }
.todo-item:hover { background: var(--color-primary-bg); }
.todo-item.finished .todo-content { text-decoration: line-through; opacity: 0.35; }
.todo-content { flex: 1; font-size: 14px; line-height: 1.5; }
.todo-deadline { font-size: 12px; color: var(--text-secondary); white-space: nowrap; background: var(--bg-primary); padding: 2px 8px; border-radius: var(--radius-sm); }
</style>
