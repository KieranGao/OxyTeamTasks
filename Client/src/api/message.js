import request from './request'

export const getMessages = (params) => request.post('/msg_list', params)
export const markRead = (data) => request.post('/msg_read', data)
export const deleteMessage = (data) => request.post('/msg_delete', data)
