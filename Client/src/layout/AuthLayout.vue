<template>
  <div class="auth-layout">
    <div class="auth-card">
      <div class="auth-brand">
        <h1>OxyTeamTask</h1>
        <p>分布式训练任务协同系统</p>
      </div>
      <router-view />
    </div>
  </div>
</template>

<script setup>
</script>

<style scoped>
.auth-layout {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  background: linear-gradient(135deg, #1d1e2c 0%, #2d3a5c 50%, #1a365d 100%);
}

:root.dark .auth-layout {
  background: linear-gradient(135deg, #0d0d0d 0%, #1a1a2e 50%, #0d1321 100%);
}

.auth-card {
  width: 440px;
  padding: 40px;
  background: var(--bg-secondary);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-lg);
}

.auth-brand {
  text-align: center;
  margin-bottom: 32px;
}

.auth-brand h1 {
  font-size: 24px;
  font-weight: 700;
  color: var(--color-primary);
  letter-spacing: 1px;
}

.auth-brand p {
  margin-top: 6px;
  font-size: 13px;
  color: var(--text-secondary);
}
</style>
