import type { ErrorResponse } from './models'
import { getToken } from './token.ts'

export class ApiError extends Error {
  public status: number;
  public key: string;

  constructor(
    status: number,
    key: string,             // ErrorResponse.key
    message: string,
  ) { super(message)
    this.key = key;
    this.status = status;
  }
}

let onUnauthorized: (() => void) | null = null
export const setUnauthorizedHandler = (fn: () => void) => { onUnauthorized = fn }

export const customFetch = async <T>(url: string, options: RequestInit): Promise<T> => {
  let res: Response
  try {
    const headers = new Headers(options.headers)
    const token = getToken()
    if (token && !headers.has('Authorization')) headers.set('Authorization', `Bearer ${token}`)
    res = await fetch(url, { ...options, headers, credentials: 'include' })
  } catch {
    throw new ApiError(0, 'network', 'Сервер недоступен')
  }

  if (res.status === 401) onUnauthorized?.()

  const type = res.headers.get('Content-Type') ?? ''
  if (!res.ok) {
    const err: ErrorResponse = type.includes('application/json')
      ? await res.json().catch(() => ({}))
      : {}
    throw new ApiError(res.status, err.key ?? 'error', err.message ?? res.statusText)
  }

  if ([204, 205, 304].includes(res.status)) return undefined as T
  if (type.includes('application/json')) return res.json()
  return res.blob() as T
}