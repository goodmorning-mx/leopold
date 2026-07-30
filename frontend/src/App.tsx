import { FormEvent, useEffect, useMemo, useRef, useState } from 'react';

type User = { email: string; display_name: string };
type Device = {
  device_id: string;
  label: string | null;
  status: string;
  firmware_version: string | null;
  last_seen_at: string | null;
};
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
    cache: 'no-store',
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

function currentSetupToken(): string | null {
  return window.location.pathname.match(/^\/setup\/([A-Za-z0-9_-]+)$/)?.[1] ?? null;
}

function AuthForm({
  context,
  onAuthenticated,
}: {
  context: 'account' | 'setup';
  onAuthenticated: (user: User) => void;
}) {
  const [mode, setMode] = useState<Mode>('login');
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');

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
      const user = await api<User>(`/v1/auth/${mode}`, {
        method: 'POST',
        body: JSON.stringify(payload),
      });
      onAuthenticated(user);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos continuar');
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="panel auth-panel">
      <div className="wordmark">GOODMORNING</div>
      <p className="kicker">{context === 'setup' ? 'Configurar Leopold' : 'Tu cuenta'}</p>
      <h1>{mode === 'register' ? 'Crea tu perfil.' : 'Inicia sesión.'}</h1>
      <p className="muted">
        {context === 'setup'
          ? 'Usaremos esta cuenta para guardar tus dispositivos y servicios.'
          : 'Administra tus dispositivos Leopold desde un solo lugar.'}
      </p>

      <div className="switcher" aria-label="Elegir acceso">
        <button className={mode === 'login' ? 'selected' : ''} onClick={() => setMode('login')}>
          Iniciar sesión
        </button>
        <button className={mode === 'register' ? 'selected' : ''} onClick={() => setMode('register')}>
          Crear cuenta
        </button>
      </div>

      <form onSubmit={authenticate}>
        {mode === 'register' && (
          <label>
            Nombre
            <input name="display_name" autoComplete="name" required maxLength={80} />
          </label>
        )}
        <label>
          Correo
          <input name="email" type="email" autoComplete="email" required maxLength={254} />
        </label>
        <label>
          Contraseña
          <input
            name="password"
            type="password"
            autoComplete={mode === 'register' ? 'new-password' : 'current-password'}
            required
            minLength={8}
          />
        </label>
        <button className="button primary" disabled={busy}>
          {busy ? 'Continuando…' : mode === 'register' ? 'Crear perfil' : 'Entrar'}
        </button>
      </form>
      {message && <p className="notice error" role="alert">{message}</p>}
    </section>
  );
}

function SetupFlow({ token }: { token: string }) {
  const [claim, setClaim] = useState<ClaimStatus | null>(null);
  const [user, setUser] = useState<User | null>(null);
  const [message, setMessage] = useState('');
  const autoClaimStarted = useRef(false);

  useEffect(() => {
    let stopped = false;
    const load = async () => {
      try {
        const status = await api<ClaimStatus>(`/v1/onboarding/${token}`);
        if (!stopped) setClaim(status);
        return status;
      } catch (error) {
        if (!stopped) setMessage(error instanceof Error ? error.message : 'No pudimos encontrar Leopold');
        return null;
      }
    };
    void load();
    void api<User>('/v1/auth/me').then(setUser).catch(() => null);
    const interval = window.setInterval(async () => {
      const status = await load();
      if (status?.valid || status?.claimed) window.clearInterval(interval);
    }, 1500);
    return () => {
      stopped = true;
      window.clearInterval(interval);
    };
  }, [token]);

  useEffect(() => {
    if (!user || !claim?.valid || claim.claimed || autoClaimStarted.current) return;
    autoClaimStarted.current = true;
    void api<ClaimStatus>(`/v1/onboarding/${token}/claim`, { method: 'POST' })
      .then(setClaim)
      .catch((error: Error) => {
        autoClaimStarted.current = false;
        setMessage(error.message);
      });
  }, [claim, token, user]);

  if (!claim || claim.pending) {
    return (
      <StatusPanel
        title="Conectando tu Leopold"
        copy="Ya guardamos la sesión. Esperamos a que el dispositivo se conecte a internet."
        loading
      />
    );
  }

  if (claim.claimed) {
    return (
      <StatusPanel
        title="Leopold está listo"
        copy="El dispositivo ya aparece en tu perfil."
        action={<a className="button primary" href="/">Ir a mis dispositivos</a>}
      />
    );
  }

  if (!claim.valid) {
    return (
      <StatusPanel
        title="La sesión terminó"
        copy="Reinicia Leopold y vuelve a escanear el QR para comenzar otra vez."
        action={<a className="button secondary" href="/">Volver a mi cuenta</a>}
      />
    );
  }

  if (!user) {
    return <AuthForm context="setup" onAuthenticated={setUser} />;
  }

  return (
    <StatusPanel
      title={`Vinculando para ${user.display_name}`}
      copy="Esto sólo toma unos segundos. No necesitas hacer nada más."
      loading
      message={message}
    />
  );
}

function StatusPanel({
  title,
  copy,
  loading = false,
  action,
  message,
}: {
  title: string;
  copy: string;
  loading?: boolean;
  action?: React.ReactNode;
  message?: string;
}) {
  return (
    <section className="panel status-panel">
      <div className="wordmark">GOODMORNING</div>
      {loading ? <div className="spinner" /> : <div className="success-mark">✓</div>}
      <h1>{title}</h1>
      <p className="muted">{copy}</p>
      {action}
      {message && <p className="notice error">{message}</p>}
    </section>
  );
}

function Profile({ user, onLogout }: { user: User; onLogout: () => void }) {
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState(true);
  const [showAdd, setShowAdd] = useState(false);
  const [message, setMessage] = useState('');

  async function loadDevices() {
    setLoading(true);
    try {
      setDevices(await api<Device[]>('/v1/me/devices'));
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos cargar tus dispositivos');
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadDevices();
  }, []);

  async function removeDevice(device: Device) {
    const name = device.label || device.device_id;
    if (!window.confirm(`¿Quitar ${name} de tu cuenta?`)) return;
    setMessage('');
    try {
      await api<void>(`/v1/me/devices/${device.device_id}`, { method: 'DELETE' });
      setDevices((current) => current.filter((item) => item.device_id !== device.device_id));
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos quitar el dispositivo');
    }
  }

  return (
    <main className="account-shell">
      <header className="topbar">
        <div className="wordmark">GOODMORNING</div>
        <button className="text-button" onClick={onLogout}>Cerrar sesión</button>
      </header>

      <section className="account-heading">
        <p className="kicker">Perfil</p>
        <h1>Hola, {user.display_name}</h1>
        <p className="muted">{user.email}</p>
      </section>

      <section className="devices-section">
        <div className="section-heading">
          <div>
            <p className="kicker">Mis dispositivos</p>
            <h2>Leopold</h2>
          </div>
          <button className="button primary compact" onClick={() => setShowAdd((value) => !value)}>
            {showAdd ? 'Cerrar' : 'Agregar dispositivo'}
          </button>
        </div>

        {showAdd && (
          <div className="instruction">
            <strong>Agregar un Leopold</strong>
            <ol>
              <li>Enciende o restaura el dispositivo.</li>
              <li>Escanea el único QR de su pantalla.</li>
              <li>Configura Wi‑Fi. Volverás aquí automáticamente.</li>
            </ol>
          </div>
        )}

        {loading ? (
          <div className="empty"><div className="spinner small" /> Cargando dispositivos…</div>
        ) : devices.length === 0 ? (
          <div className="empty">
            <strong>Aún no tienes dispositivos.</strong>
            <span>Pulsa “Agregar dispositivo” para configurar el primero.</span>
          </div>
        ) : (
          <div className="device-list">
            {devices.map((device) => (
              <article className="device-row" key={device.device_id}>
                <div className="device-icon">L</div>
                <div className="device-copy">
                  <strong>{device.label || 'Leopold Pocket'}</strong>
                  <span>{device.device_id}</span>
                  <small>
                    <i className={device.status === 'online' ? 'online' : ''} />
                    {device.status === 'online' ? 'Conectado' : 'Sin conexión'}
                    {device.firmware_version ? ` · Firmware ${device.firmware_version}` : ''}
                  </small>
                </div>
                <button className="remove-button" onClick={() => void removeDevice(device)}>
                  Quitar
                </button>
              </article>
            ))}
          </div>
        )}
        {message && <p className="notice error" role="alert">{message}</p>}
      </section>
    </main>
  );
}

function App() {
  const token = useMemo(currentSetupToken, []);
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(!token);

  useEffect(() => {
    if (token) return;
    void api<User>('/v1/auth/me')
      .then(setUser)
      .catch(() => null)
      .finally(() => setLoading(false));
  }, [token]);

  async function logout() {
    await api<void>('/v1/auth/logout', { method: 'POST' });
    setUser(null);
  }

  if (token) return <main className="center-shell"><SetupFlow token={token} /></main>;
  if (loading) return <main className="center-shell"><StatusPanel title="Cargando tu perfil" copy="" loading /></main>;
  if (!user) return <main className="center-shell"><AuthForm context="account" onAuthenticated={setUser} /></main>;
  return <Profile user={user} onLogout={() => void logout()} />;
}

export default App;
