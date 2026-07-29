<template>
  <div class="auth-layout">
    <div class="auth-grid"></div>
    <div class="auth-card">
      <div class="auth-brand">
        <div class="brand-marker">&gt;_</div>
        <h1>OxyTeamTask</h1>
        <p>分布式训练任务协同系统</p>
      </div>
      <router-view />
    </div>
    <div class="auth-footer">
      <span class="footer-code">© 2026</span>
      <span class="footer-sep">·</span>
      <span>Built for competitive programmers</span>
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
  background: var(--bg-base);
  position: relative;
  overflow: hidden;
}

.auth-grid {
  position: absolute;
  inset: 0;
  background-image:
    linear-gradient(var(--border-subtle) 1px, transparent 1px),
    linear-gradient(90deg, var(--border-subtle) 1px, transparent 1px);
  background-size: 48px 48px;
  opacity: 0.4;
  pointer-events: none;
}

.auth-card {
  width: 400px;
  padding: var(--space-8);
  background: var(--bg-surface);
  border: 1px solid var(--border-default);
  border-radius: var(--radius-md);
  position: relative;
  z-index: 1;
}

.auth-brand {
  text-align: center;
  margin-bottom: var(--space-8);
}

.brand-marker {
  display: inline-block;
  font-family: var(--font-mono);
  font-size: var(--text-2xl);
  font-weight: 700;
  color: var(--color-primary);
  margin-bottom: var(--space-3);
  letter-spacing: var(--tracking-tight);
}

.auth-brand h1 {
  font-family: var(--font-mono);
  font-size: var(--text-xl);
  font-weight: 700;
  color: var(--text-primary);
  letter-spacing: var(--tracking-tight);
}

.auth-brand p {
  margin-top: var(--space-2);
  font-size: var(--text-sm);
  color: var(--text-secondary);
}

.auth-footer {
  position: absolute;
  bottom: var(--space-6);
  left: 50%;
  transform: translateX(-50%);
  font-family: var(--font-mono);
  font-size: var(--text-xs);
  color: var(--text-tertiary);
  display: flex;
  align-items: center;
  gap: var(--space-2);
}

.footer-code { color: var(--text-secondary); }
.footer-sep { color: var(--text-tertiary); }
</style>
