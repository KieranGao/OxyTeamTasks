import request from './request'

export const doCheckin = (data) => request.post('/checkin', data)
export const getCheckins = (params) => request.post('/checkin_list', params)
