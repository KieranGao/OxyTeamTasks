<template>
  <div class="page-container">
    <div class="page-header">
      <h2>工作台</h2>
      <p>欢迎回来，{{ userStore.username }}</p>
    </div>

    <el-row :gutter="20">
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>训练任务概览</template>
          <p style="color: var(--text-secondary)">任务看板功能开发中...</p>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>今日打卡</template>
          <p style="color: var(--text-secondary)">打卡功能开发中...</p>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>未读消息</template>
          <p style="color: var(--text-secondary)">消息中心开发中...</p>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { useUserStore } from '@/stores/user'
const userStore = useUserStore()
</script>
