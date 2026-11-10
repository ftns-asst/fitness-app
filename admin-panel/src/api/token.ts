const KEY = "admin_token";

export const getToken = (): string | null => {
  try {
    return localStorage.getItem(KEY);
  } catch {
    return null;
  }
}

export const setToken = (token: string) => {
  try {
    localStorage.setItem(KEY, token);
  } catch { /* ignore */
  }
}

export const clearToken = () => {
  try {
    localStorage.removeItem(KEY);
  } catch { /* ignore */
  }
}
