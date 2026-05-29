import request from './request'

export const createTask = (data) => request.post('/task_create', data)
export const updateTask = (data) => request.post('/task_update', data)
export const deleteTask = (data) => request.post('/task_delete', data)
export const getTask    = (id)   => request.post('/task_get', { id })
export const listTasks  = (p)    => request.post('/task_list', p)
export const addTodo    = (data) => request.post('/todo_add', data)
export const updateTodo = (data) => request.post('/todo_update', data)
export const deleteTodo = (data) => request.post('/todo_delete', data)
export const listTodo   = (p)    => request.post('/todo_list', p)
