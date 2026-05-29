import { createRouter, createWebHashHistory } from 'vue-router'

import AuthLayout from '@/layout/AuthLayout.vue'
import MainLayout from '@/layout/MainLayout.vue'

import LoginView from '@/views/login/LoginView.vue'
import RegisterView from '@/views/register/RegisterView.vue'
import ResetPasswordView from '@/views/reset/ResetPasswordView.vue'
import DashboardView from '@/views/dashboard/DashboardView.vue'
import TaskBoardView from '@/views/taskboard/TaskBoardView.vue'
import TodoListView from '@/views/todolist/TodoListView.vue'
import CheckinView from '@/views/checkin/CheckinView.vue'
import MessagesView from '@/views/messages/MessagesView.vue'
import ProfileView from '@/views/profile/ProfileView.vue'
import TaskManageView from '@/views/manage/TaskManageView.vue'
import TeamProgressView from '@/views/manage/TeamProgressView.vue'
import UserManageView from '@/views/manage/UserManageView.vue'
import MonitorView from '@/views/manage/MonitorView.vue'
import AllTeamsInfoView from '@/views/manage/AllTeamsInfoView.vue'

const routes = [
  {
    path: '/login',
    component: AuthLayout,
    meta: { guest: true },
    children: [
      { path: '', name: 'Login', component: LoginView },
    ],
  },
  {
    path: '/register',
    component: AuthLayout,
    meta: { guest: true },
    children: [
      { path: '', name: 'Register', component: RegisterView },
    ],
  },
  {
    path: '/reset',
    component: AuthLayout,
    meta: { guest: true },
    children: [
      { path: '', name: 'ResetPassword', component: ResetPasswordView },
    ],
  },
  {
    path: '/',
    component: MainLayout,
    meta: { requiresAuth: true },
    redirect: '/dashboard',
    children: [
      { path: 'dashboard', name: 'Dashboard', component: DashboardView, meta: { title: '工作台' } },
      { path: 'taskboard', name: 'TaskBoard', component: TaskBoardView, meta: { title: '任务看板' } },
      { path: 'todolist', name: 'TodoList', component: TodoListView, meta: { title: 'TODO List' } },
      { path: 'checkin', name: 'Checkin', component: CheckinView, meta: { title: '每日打卡' } },
      { path: 'messages', name: 'Messages', component: MessagesView, meta: { title: '消息中心' } },
      { path: 'profile', name: 'Profile', component: ProfileView, meta: { title: '个人中心' } },
      {
        path: 'manage/tasks',
        name: 'TaskManage',
        component: TaskManageView,
        meta: { title: '任务管理', roles: ['captain', 'coach'] },
      },
      {
        path: 'manage/team',
        name: 'TeamProgress',
        component: TeamProgressView,
        meta: { title: '队伍信息', roles: ['captain', 'coach'] },
      },
      {
        path: 'manage/users',
        name: 'UserManage',
        component: UserManageView,
        meta: { title: '权限管理', roles: ['coach'] },
      },
      {
        path: 'manage/allteams',
        name: 'AllTeamsInfo',
        component: AllTeamsInfoView,
        meta: { title: '全队信息', roles: ['coach'] },
      },
      {
        path: 'manage/monitor',
        name: 'Monitor',
        component: MonitorView,
        meta: { title: '系统监控', roles: ['coach'] },
      },
    ],
  },
]

const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

// Map numeric role (from DB) to string key for route guard matching
const ROLE_KEY = { 0: 'member', 1: 'captain', 2: 'coach' }

// Global navigation guard
router.beforeEach((to, from, next) => {
  const token = localStorage.getItem('token')
  const rawRole = localStorage.getItem('role') || '0'
  const roleKey = ROLE_KEY[rawRole] || 'member'

  // Guest-only pages (login, register, reset) — redirect to dashboard if already logged in
  if (to.matched.some((r) => r.meta.guest)) {
    if (token) {
      return next('/dashboard')
    }
    return next()
  }

  // Protected pages — redirect to login if not authenticated
  if (to.matched.some((r) => r.meta.requiresAuth)) {
    if (!token) {
      return next('/login')
    }

    // Role check
    const routeRoles = to.meta.roles
    if (routeRoles && !routeRoles.includes(roleKey)) {
      return next('/dashboard')
    }
  }

  next()
})

export default router
