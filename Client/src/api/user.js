import request from './request'

/**
 * Request email verification code
 * @param {string} email
 */
export function getVerifyCode(email) {
  return request.post('/get_verify_code', { email })
}

/**
 * Register a new user
 * @param {object} data — { user, email, password, confirm, verify_code }
 */
export function registerUser(data) {
  return request.post('/user_register', data)
}

/**
 * Reset password
 * @param {object} data — { user, email, password, confirm, verify_code }
 */
export function resetPassword(data) {
  return request.post('/user_resetpass', data)
}

/**
 * Login
 * @param {object} data — { email, password }
 */
export function loginUser(data) {
  return request.post('/user_login', data)
}

/**
 * Update team affiliation (coach only)
 * @param {object} data — { uid, belong_team_id }
 */
export function updateTeamInfo(data) {
  return request.post('/user_update_team', data)
}

/**
 * List pending (unapproved) users — coach only
 */
export function listPendingUsers() {
  return request.post('/user_list_pending', {})
}

/**
 * Approve a pending user
 * @param {object} data — { uid, role, belong_team_id }
 */
export function approveUser(data) {
  return request.post('/user_approve', data)
}

/**
 * Reject a pending user (deletes the record)
 * @param {number} uid
 */
export function rejectUser(uid) {
  return request.post('/user_reject', { uid })
}

/**
 * Set user role and team — coach only
 * @param {object} data — { uid, role, belong_team_id }
 */
export function setUserRole(data) {
  return request.post('/user_set_role', data)
}

/**
 * List all active (approved) users — coach only
 */
export function listAllUsers() {
  return request.post('/user_list_all', {})
}

/**
 * Query system logs
 * @param {object} params — { service, level, limit }
 */
export function queryLogs(params) {
  return request.post('/monitor/query_logs', params)
}

/**
 * Query server status
 */
export function queryServerStatus() {
  return request.post('/monitor/server_status', {})
}
