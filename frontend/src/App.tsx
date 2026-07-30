import { FormEvent, useEffect, useMemo, useState } from 'react';

type User = { email: string; display_name: string };
type ClaimStatus = { valid: boolean; claimed: boolean; expires_at: string | null };
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

  useEffect(() => {
    if (!token) return;
    void Promise.all([
      api<ClaimStatus>(`/v1/onboarding/${token}`),
      api<User>('/v1/auth/me').catch(() => null),
    ]).then(([claimStatus, currentUser]) => {
      setClaim(claimStatus);
      setUser(currentUser);
    }).catch((error: Error) => setMessage(error.message));
  }, [token]);

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

  if (!claim) {
    return (
      <main className="shell"><section className="card centered"><div className="loader" /><p>Buscando tu Leopold…</p></section></main>
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
            <h1>Encontramos tu Leopold.</h1>
            <p className="lede">Vincúlalo a tu cuenta para enviarle experiencias y conectar servicios.</p>
            <div className="device">
              <div className="mini-orb"><span /><span /></div>
              <div><strong>Leopold Pocket</strong><small>Listo para vincular</small></div>
              <i />
            </div>
            <button className="primary" disabled={busy} onClick={claimDevice}>{busy ? 'Vinculando…' : 'Vincular Leopold'}</button>
          </>
        )}
        {message && <p className="error" role="alert">{message}</p>}
        <p className="privacy">El QR expira automáticamente. Tu contraseña nunca se comparte con el dispositivo.</p>
      </section>
    </main>
  );
}

export default App;
