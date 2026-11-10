import {useState, type FormEvent} from "react";
import styles from "./auth.module.css"
import {loginAdmin} from "../api/admin.ts";
import {ApiError} from "../api/fetch.ts";

export const AuthPage = ({onLogin}: { onLogin: () => void }) => {
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string>();
  const [loading, setLoading] = useState(false);

  const submit = async (e: FormEvent) => {
    e.preventDefault();
    setLoading(true);
    setError(undefined);
    try {
      await loginAdmin(password);
      onLogin();
    } catch (err) {
      setError(err instanceof ApiError && err.status === 401 ? "Неверный пароль" : (err as Error).message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className={styles.authPage}>
      <form className={styles.authForm} onSubmit={submit}>
        <h2>Введите пароль</h2>
        <input
          type="password"
          autoFocus
          value={password}
          onChange={e => setPassword(e.target.value)}
        />
        {error && <label className={styles.error}>{error}</label>}
        <button className={styles.loginButton} disabled={!password || loading}>Войти</button>
      </form>
    </div>
  )
}
