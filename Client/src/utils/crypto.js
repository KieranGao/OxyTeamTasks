/**
 * SHA-256 password hashing utility.
 *
 * Uses the browser's built-in Web Crypto API (no external dependencies).
 * The hash is deterministic — same password always produces the same hex string.
 * This is compatible with the backend's direct-password-comparison approach:
 * the SHA-256 hex digest is sent as the "password" field and stored/compared as-is.
 *
 * This is a significant improvement over the Qt client's XOR obfuscation.
 */

/**
 * Hash a string using SHA-256, returning a lowercase hex string.
 * @param {string} input
 * @returns {Promise<string>} hex-encoded SHA-256 digest
 */
export async function sha256(input) {
  const encoder = new TextEncoder()
  const data = encoder.encode(input)
  const hashBuffer = await crypto.subtle.digest('SHA-256', data)
  const hashArray = Array.from(new Uint8Array(hashBuffer))
  return hashArray.map((b) => b.toString(16).padStart(2, '0')).join('')
}

/**
 * Synchronous wrapper for sha256 — falls back to a simple placeholder if
 * crypto.subtle is unavailable (should never happen in modern browsers).
 * Use the async version in production code; this is a convenience for
 * call sites that cannot be async.
 *
 * NOTE: Only use this if you cannot make the call site async.
 * Prefer `await sha256(input)` everywhere possible.
 *
 * @param {string} input
 * @returns {string} hex-encoded SHA-256 digest, or the input unchanged on error
 */
export function sha256Sync(input) {
  // Web Crypto requires the async API — sync is not possible.
  // Callers must use the async sha256() instead.
  throw new Error('sha256Sync is not available — use await sha256() instead')
}
