<template>
  <div class="register-view">
    <el-form
      ref="formRef"
      :model="form"
      :rules="rules"
      label-position="top"
      @keyup.enter="handleRegister"
    >
      <el-form-item label="用户名" prop="user">
        <el-input
          v-model="form.user"
          placeholder="请输入用户名"
          :prefix-icon="User"
          size="large"
        />
      </el-form-item>

      <el-form-item label="邮箱" prop="email">
        <el-input
          v-model="form.email"
          placeholder="请输入邮箱地址"
          :prefix-icon="Message"
          size="large"
        />
      </el-form-item>

      <el-form-item label="密码" prop="password">
        <el-input
          v-model="form.password"
          type="password"
          placeholder="请输入密码（至少6位）"
          :prefix-icon="Lock"
          size="large"
          show-password
        />
      </el-form-item>

      <el-form-item label="确认密码" prop="confirm">
        <el-input
          v-model="form.confirm"
          type="password"
          placeholder="请再次输入密码"
          :prefix-icon="Lock"
          size="large"
          show-password
        />
      </el-form-item>

      <el-form-item label="验证码" prop="verify_code">
        <div class="verify-row">
          <el-input
            v-model="form.verify_code"
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
        <el-button
          type="primary"
          size="large"
          :loading="loading"
          style="width: 100%"
          @click="handleRegister"
        >
          {{ loading ? '注册中...' : '注 册' }}
        </el-button>
      </el-form-item>
    </el-form>

    <div class="register-footer">
      <router-link to="/login">已有账号？立即登录</router-link>
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

const formRef = ref(null)
const loading = ref(false)
const sendingCode = ref(false)
const countdown = ref(0)
let countdownTimer = null

const form = reactive({
  user: '',
  email: '',
  password: '',
  confirm: '',
  verify_code: '',
})

const validateConfirm = (rule, value, callback) => {
  if (value !== form.password) {
    callback(new Error('两次输入的密码不一致'))
  } else {
    callback()
  }
}

const rules = {
  user: [
    { required: true, message: '请输入用户名', trigger: 'blur' },
    { min: 2, max: 20, message: '用户名长度为 2-20 个字符', trigger: 'blur' },
  ],
  email: [
    { required: true, message: '请输入邮箱地址', trigger: 'blur' },
    { type: 'email', message: '请输入有效的邮箱地址', trigger: 'blur' },
  ],
  password: [
    { required: true, message: '请输入密码', trigger: 'blur' },
    { min: 6, message: '密码长度至少为 6 位', trigger: 'blur' },
  ],
  confirm: [
    { required: true, message: '请再次输入密码', trigger: 'blur' },
    { validator: validateConfirm, trigger: 'blur' },
  ],
  verify_code: [
    { required: true, message: '请输入验证码', trigger: 'blur' },
  ],
}

async function sendVerifyCode() {
  try {
    await formRef.value.validateField('email')
  } catch {
    return
  }

  sendingCode.value = true
  try {
    const res = await getVerifyCode(form.email)
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

async function handleRegister() {
  const valid = await formRef.value.validate().catch(() => false)
  if (!valid) return

  loading.value = true
  try {
    const hashedPassword = await sha256(form.password)
    const hashedConfirm = await sha256(form.confirm)
    await userStore.register({
      user: form.user,
      email: form.email,
      password: hashedPassword,
      confirm: hashedConfirm,
      verify_code: form.verify_code,
    })
    ElMessage.success('注册成功，等待管理员审核后即可登录')
    router.push('/login')
  } catch {
    ElMessage.error('注册失败，请检查信息后重试')
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.register-view {
  width: 100%;
}

.register-view :deep(.el-input__wrapper:focus-within) {
  box-shadow: 0 0 0 2px var(--color-primary-bg), 0 0 0 3px var(--color-primary) !important;
}

.register-view :deep(.el-button--large) {
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

.register-footer {
  text-align: center;
  margin-top: 12px;
}

.register-footer a {
  font-size: 13px;
  color: var(--text-secondary);
  transition: color var(--transition-fast);
}

.register-footer a:hover {
  color: var(--color-primary);
}
</style>
