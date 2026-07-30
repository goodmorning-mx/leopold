import { FormEvent, useEffect, useMemo, useRef, useState } from 'react';

type User = { email: string; display_name: string };
type ClaimStatus = {
  valid: boolean;
  claimed: boolean;
  pending: boolean;
  expires_at: string | null;
};
type Mode = 'register' | 'login';

const apiUrl = import.meta.env.VITE_API_URL ?? 'http://localhost:8000';

async function api<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${apiUrl}${path}`, {
    ...init,
    credentials: 'include',
    headers: {
      ...(init?.body ? { 'Content-Type': 'application/json' } : {}),
      ...init?.headers,
    },
  });
  if (!response.ok) {
    const payload = (await response.json().catch(() => null)) as { detail?: string } | null;
    throw new Error(payload?.detail ?? `Error ${response.status}`);
  }
  return response.status === 204 ? (undefined as T) : ((await response.json()) as T);
}

function setupToken(): string | null {
  const match = window.location.pathname.match(/^\/setup\/([A-Za-z0-9_-]+)$/);
  return match?.[1] ?? null;
}

function App() {
  const token = useMemo(setupToken, []);
  const [claim, setClaim] = useState<ClaimStatus | null>(null);
  const [user, setUser] = useState<User | null>(null);
  const [mode, setMode] = useState<Mode>('register');
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');
  const autoClaimStarted = useRef(false);

  useEffect(() => {
    if (!token) return;
    let stopped = false;
    const loadStatus = async () => {
      try {
        const status = await api<ClaimStatus>(`/v1/onboarding/${token}`);
        if (!stopped) setClaim(status);
        return status;
      } catch (error) {
        if (!stopped) {
          setMessage(error instanceof Error ? error.message : 'No pudimos encontrar Leopold');
        }
        return null;
      }
    };
    void loadStatus();
    const interval = window.setInterval(async () => {
      const status = await loadStatus();
      if (status?.valid || status?.claimed) window.clearInterval(interval);
    }, 1500);
    void api<User>('/v1/auth/me').then(setUser).catch(() => null);
    return () => {
      stopped = true;
      window.clearInterval(interval);
    };
  }, [token]);

  useEffect(() => {
    if (
      token &&
      user &&
      claim?.valid &&
      !claim.claimed &&
      !autoClaimStarted.current
    ) {
      autoClaimStarted.current = true;
      void claimDevice();
    }
  }, [claim, token, user]);

  async function authenticate(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    setBusy(true);
    setMessage('');
    try {
      const payload = mode === 'register'
        ? {
            email: form.get('email'),
            display_name: form.get('display_name'),
            password: form.get('password'),
          }
        : { email: form.get('email'), password: form.get('password') };
      const currentUser = await api<User>(`/v1/auth/${mode}`, {
        method: 'POST',
        body: JSON.stringify(payload),
      });
      setUser(currentUser);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos iniciar sesión');
    } finally {
      setBusy(false);
    }
  }

  async function claimDevice() {
    if (!token) return;
    setBusy(true);
    setMessage('');
    try {
      const status = await api<ClaimStatus>(`/v1/onboarding/${token}/claim`, {
        method: 'POST',
      });
      setClaim(status);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos vincular Leopold');
    } finally {
      setBusy(false);
    }
  }

  if (!token) {
    return (
      <main className="shell">
        <section className="card landing">
          <div className="orb" aria-hidden="true"><span /><span /></div>
          <p className="eyebrow">Leopold Pocket</p>
          <h1>Tu pequeño acceso a lo importante.</h1>
          <p className="lede">Enciende tu Leopold y escanea el QR de su pantalla para comenzar.</p>
          <div className="signal"><i /> Sistema de configuración disponible</div>
        </section>
      </main>
    );
  }

  if (!claim || claim.pending) {
    return (
      <main className="shell">
        <section className="card centered">
          <div className="loader" />
          <p className="eyebrow">Configurando Leopold</p>
          <h1>Conectándolo a internet…</h1>
          <p className="lede">Mantén esta página abierta. Continuará automáticamente.</p>
        </section>
      </main>
    );
  }

  if (claim.claimed) {
    return (
      <main className="shell">
        <section className="card centered success">
          <div className="check">✓</div>
          <p className="eyebrow">Todo listo</p>
          <h1>Leopold ya es tuyo.</h1>
          <p className="lede">Mira su pantalla. En unos segundos continuará automáticamente.</p>
        </section>
      </main>
    );
  }

  if (!claim.valid) {
    return (
      <main className="shell">
        <section className="card centered">
          <p className="eyebrow">Enlace vencido</p>
          <h1>Genera un QR nuevo.</h1>
          <p className="lede">Reinicia Leopold para obtener un enlace de configuración fresco.</p>
        </section>
      </main>
    );
  }

  return (
    <main className="shell">
      <section className="card">
        <header className="brand">
          <div className="mini-orb"><span /><span /></div>
          <div><p className="eyebrow">Configurar Leopold</p><p className="step">Conexión segura · 2 minutos</p></div>
        </header>

        {!user ? (
          <>
            <h1>{mode === 'register' ? 'Crea tu cuenta.' : 'Qué gusto verte.'}</h1>
            <p className="lede">
              {mode === 'register'
                ? 'Esta cuenta guardará tus servicios y preferencias.'
                : 'Inicia sesión para vincular este Leopold.'}
            </p>
            <div className="tabs">
              <button className={mode === 'register' ? 'active' : ''} onClick={() => setMode('register')}>Crear cuenta</button>
              <button className={mode === 'login' ? 'active' : ''} onClick={() => setMode('login')}>Ya tengo cuenta</button>
            </div>
            <form onSubmit={authenticate}>
              {mode === 'register' && <label>Tu nombre<input name="display_name" autoComplete="name" required maxLength={80} /></label>}
              <label>Correo<input name="email" type="email" autoComplete="email" required maxLength={254} /></label>
              <label>Contraseña<input name="password" type="password" autoComplete={mode === 'register' ? 'new-password' : 'current-password'} required minLength={8} /></label>
              <button className="primary" disabled={busy}>{busy ? 'Conectando…' : mode === 'register' ? 'Continuar' : 'Entrar'}</button>
            </form>
          </>
        ) : (
          <>
            <p className="eyebrow">Hola, {user.display_name}</p>
            <h1>Vinculando tu Leopold…</h1>
            <p className="lede">No necesitas hacer nada más.</p>
            <div className="device">
              <div className="mini-orb"><span /><span /></div>
              <div><strong>Leopold Pocket</strong><small>Finalizando configuración</small></div>
              <i />
            </div>
            <div className="loader" />
          </>
        )}
        {message && <p className="error" role="alert">{message}</p>}
        <p className="privacy">El QR expira automáticamente. Tu contraseña nunca se comparte con el dispositivo.</p>
      </section>
    </main>
  );
}

export default App;
