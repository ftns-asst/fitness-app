import {useEffect, useState} from "react";
import './App.css'
import {FilesPage} from "./pages/files.tsx";
import {AuthPage} from "./pages/auth.tsx";
import {setUnauthorizedHandler} from "./api/fetch.ts";
import {clearToken, getToken} from "./api/token.ts";

function App() {
  const [authed, setAuthed] = useState(() => getToken() !== null);

  const logout = () => {
    clearToken();
    setAuthed(false);
  };

  useEffect(() => {
    setUnauthorizedHandler(logout);
  }, []);

  if (!authed) return <AuthPage onLogin={() => setAuthed(true)}/>;

  return (
    <div className="app">
      <header className="appHeader">
        <button onClick={logout}>Выйти</button>
      </header>
      <main className="appContent">
        <FilesPage/>
      </main>
    </div>
  )
}

export default App
