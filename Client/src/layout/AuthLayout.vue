<template>
  <div class="auth-layout">
    <div class="auth-card">
      <div class="auth-card-stripe"></div>
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
  background: var(--gradient-auth);
  position: relative;
  overflow: hidden;
}

.auth-layout::before {
  content: '';
  position: absolute;
  top: -50%;
  left: -50%;
  width: 200%;
  height: 200%;
  background: radial-gradient(ellipse at 30% 50%, rgba(79, 110, 247, 0.15), transparent 50%),
              radial-gradient(ellipse at 70% 50%, rgba(124, 92, 252, 0.1), transparent 50%);
  pointer-events: none;
}

.auth-card {
  width: 440px;
  padding: 40px;
  background: var(--bg-secondary);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-lg);
  position: relative;
  overflow: hidden;
  z-index: 1;
}

.auth-card-stripe {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 3px;
  background: var(--gradient-primary);
}

.auth-brand {
  text-align: center;
  margin-bottom: 32px;
}

.auth-brand h1 {
  font-size: 26px;
  font-weight: 700;
  background: var(--gradient-primary);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  letter-spacing: 0.5px;
}

.auth-brand p {
  margin-top: 8px;
  font-size: 13px;
  color: var(--text-secondary);
}
</style>
