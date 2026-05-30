<template>
  <div class="auth-layout">
    <div class="auth-bg-pattern"></div>
    <div class="auth-card">
      <div class="auth-card-stripe"></div>
      <div class="auth-brand">
        <div class="auth-logo-glow"></div>
        <h1>OxyTeamTask</h1>
        <p>分布式训练任务协同系统</p>
      </div>
      <router-view />
    </div>
    <div class="auth-footer">© 2026 OxyTeamTask. Built for competitive programmers.</div>
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

.auth-bg-pattern {
  position: absolute;
  inset: 0;
  background-image:
    radial-gradient(ellipse at 25% 60%, rgba(79, 110, 247, 0.12) 0%, transparent 50%),
    radial-gradient(ellipse at 75% 40%, rgba(124, 92, 252, 0.08) 0%, transparent 50%),
    radial-gradient(ellipse at 50% 80%, rgba(34, 197, 94, 0.04) 0%, transparent 40%);
  pointer-events: none;
}

.auth-card {
  width: 440px;
  padding: 44px;
  background: var(--bg-secondary);
  border-radius: var(--radius-xl);
  box-shadow: var(--shadow-xl);
  position: relative;
  overflow: hidden;
  z-index: 1;
}

.auth-card-stripe {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: var(--gradient-primary);
}

.auth-brand {
  text-align: center;
  margin-bottom: 36px;
  position: relative;
}

.auth-logo-glow {
  position: absolute;
  top: -40px;
  left: 50%;
  transform: translateX(-50%);
  width: 200px;
  height: 100px;
  background: radial-gradient(ellipse, rgba(79, 110, 247, 0.1), transparent 70%);
  pointer-events: none;
}

.auth-brand h1 {
  font-size: 28px;
  font-weight: 800;
  background: var(--gradient-primary);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  letter-spacing: -0.5px;
  position: relative;
  z-index: 1;
}

.auth-brand p {
  margin-top: 8px;
  font-size: 13px;
  color: var(--text-secondary);
  letter-spacing: 0.3px;
}

.auth-footer {
  position: absolute;
  bottom: 24px;
  left: 50%;
  transform: translateX(-50%);
  font-size: 12px;
  color: rgba(255, 255, 255, 0.25);
  letter-spacing: 0.5px;
}
</style>
