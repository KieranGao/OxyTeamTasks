<template>
  <div class="reset-view">
    <el-steps :active="step" align-center finish-status="success" style="margin-bottom: 28px">
      <el-step title="验证身份" />
      <el-step title="重置密码" />
      <el-step title="完成" />
    </el-steps>

    <!-- Step 1: Verify identity (username + email + verify code) -->
    <el-form
      v-if="step === 0"
      ref="verifyFormRef"
      :model="verifyForm"
      :rules="verifyRules"
      label-position="top"
    >
      <el-form-item label="用户名" prop="user">
        <el-input
          v-model="verifyForm.user"
          placeholder="请输入您的用户名"
          :prefix-icon="User"
          size="large"
        />
      </el-form-item>

      <el-form-item label="注册邮箱" prop="email">
        <el-input
          v-model="verifyForm.email"
          placeholder="请输入注册时使用的邮箱"
          :prefix-icon="Message"
          size="large"
        />
      </el-form-item>

      <el-form-item label="验证码" prop="verify_code">
        <div class="verify-row">
          <el-input
            v-model="verifyForm.verify_code"
            placeholder="请输入验证码"
            size="large"
          />
          <el-button
            type="primary"
            :disabled="sendingCode || countdown > 0"
            :loading="sendingCode"
            size="large"
            @click="sendVerifyCode"
          >
            {{ countdown > 0 ? `${countdown}s` : '发送验证码' }}
          </el-button>
        </div>
      </el-form-item>

      <el-form-item>
        <el-button type="primary" size="large" style="width: 100%" @click="handleVerify">
          下一步
        </el-button>
      </el-form-item>
    </el-form>

    <!-- Step 2: Set new password -->
    <el-form
      v-if="step === 1"
      ref="resetFormRef"
      :model="resetForm"
      :rules="resetRules"
      label-position="top"
      @keyup.enter="handleReset"
    >
      <el-form-item label="新密码" prop="password">
        <el-input
          v-model="resetForm.password"
          type="password"
          placeholder="请输入新密码（至少6位）"
          :prefix-icon="Lock"
          size="large"
          show-password
        />
      </el-form-item>

      <el-form-item label="确认新密码" prop="confirm">
        <el-input
          v-model="resetForm.confirm"
          type="password"
          placeholder="请再次输入新密码"
          :prefix-icon="Lock"
          size="large"
          show-password
        />
      </el-form-item>

      <el-form-item>
        <el-button
          type="primary"
          size="large"
          :loading="resetLoading"
          style="width: 100%"
          @click="handleReset"
        >
          {{ resetLoading ? '重置中...' : '重置密码' }}
        </el-button>
      </el-form-item>
      <el-form-item>
        <el-button size="large" style="width: 100%" @click="step = 0">返回上一步</el-button>
      </el-form-item>
    </el-form>

    <!-- Step 3: Done -->
    <div v-if="step === 2" class="reset-done">
      <el-result icon="success" title="密码重置成功" sub-title="请使用新密码重新登录">
        <template #extra>
          <el-button type="primary" size="large" @click="goLogin">返回登录</el-button>
        </template>
      </el-result>
    </div>

    <div v-if="step !== 2" class="reset-footer">
      <router-link to="/login">返回登录</router-link>
    </div>
  </div>
</template>

<script setup>
import { reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '@/stores/user'
import { getVerifyCode } from '@/api/user'
import { sha256 } from '@/utils/crypto'
import { ElMessage } from 'element-plus'
import { User, Message, Lock } from '@element-plus/icons-vue'

const router = useRouter()
const userStore = useUserStore()

const step = ref(0)

// --- Step 1: Verify identity ---
const verifyFormRef = ref(null)
const sendingCode = ref(false)
const countdown = ref(0)
let countdownTimer = null

const verifyForm = reactive({
  user: '',
  email: '',
  verify_code: '',
})

const verifyRules = {
  user: [
    { required: true, message: '请输入用户名', trigger: 'blur' },
  ],
  email: [
    { required: true, message: '请输入邮箱地址', trigger: 'blur' },
    { type: 'email', message: '请输入有效的邮箱地址', trigger: 'blur' },
  ],
  verify_code: [
    { required: true, message: '请输入验证码', trigger: 'blur' },
  ],
}

async function sendVerifyCode() {
  try {
    await verifyFormRef.value.validateField('email')
  } catch {
    return
  }

  sendingCode.value = true
  try {
    const res = await getVerifyCode(verifyForm.email)
    if (res.error !== 0) {
      ElMessage.error('验证码发送失败，请稍后重试')
      return
    }
    ElMessage.success('验证码已发送，请查收邮箱')
    countdown.value = 60
    countdownTimer = setInterval(() => {
      countdown.value--
      if (countdown.value <= 0) {
        clearInterval(countdownTimer)
        countdownTimer = null
      }
    }, 1000)
  } catch {
    ElMessage.error('验证码发送失败，请稍后重试')
  } finally {
    sendingCode.value = false
  }
}

function handleVerify() {
  verifyFormRef.value.validate((valid) => {
    if (!valid) return
    step.value = 1
  })
}

// --- Step 2: Reset password ---
const resetFormRef = ref(null)
const resetLoading = ref(false)

const resetForm = reactive({
  password: '',
  confirm: '',
})

const validateResetConfirm = (rule, value, callback) => {
  if (value !== resetForm.password) {
    callback(new Error('两次输入的密码不一致'))
  } else {
    callback()
  }
}

const resetRules = {
  password: [
    { required: true, message: '请输入新密码', trigger: 'blur' },
    { min: 6, message: '密码长度至少为 6 位', trigger: 'blur' },
  ],
  confirm: [
    { required: true, message: '请再次输入新密码', trigger: 'blur' },
    { validator: validateResetConfirm, trigger: 'blur' },
  ],
}

async function handleReset() {
  const valid = await resetFormRef.value.validate().catch(() => false)
  if (!valid) return

  resetLoading.value = true
  try {
    const hashedPassword = await sha256(resetForm.password)
    const hashedConfirm = await sha256(resetForm.confirm)
    await userStore.resetPassword({
      user: verifyForm.user,
      email: verifyForm.email,
      password: hashedPassword,
      confirm: hashedConfirm,
      verify_code: verifyForm.verify_code,
    })
    step.value = 2
  } catch {
    ElMessage.error('密码重置失败，请检查信息是否正确')
  } finally {
    resetLoading.value = false
  }
}

function goLogin() {
  router.push('/login')
}
</script>

<style scoped>
.reset-view {
  width: 100%;
}

.reset-view :deep(.el-input__wrapper:focus-within) {
  box-shadow: 0 0 0 2px var(--color-primary-bg), 0 0 0 3px var(--color-primary) !important;
}

.reset-view :deep(.el-button--large) {
  height: 44px;
  font-size: 15px;
  font-weight: 600;
  letter-spacing: 2px;
}

.verify-row {
  display: flex;
  gap: 8px;
}

.verify-row .el-input {
  flex: 1;
}

.verify-row .el-button {
  min-width: 120px;
}

.reset-done {
  padding: 20px 0;
}

.reset-footer {
  text-align: center;
  margin-top: 12px;
}

.reset-footer a {
  font-size: 13px;
  color: var(--text-secondary);
  transition: color var(--transition-fast);
}

.reset-footer a:hover {
  color: var(--color-primary);
}
</style>
