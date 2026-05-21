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
 * @param {object} data — { uid, belong_captain_id }
 */
export function updateTeamInfo(data) {
  return request.post('/user_update_team', data)
}
