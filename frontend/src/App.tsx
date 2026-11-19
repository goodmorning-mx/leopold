import { FormEvent, useEffect, useMemo, useRef, useState } from 'react';
import QrScanner from 'qr-scanner';
import Simulator from './simulator/Simulator';
import { authCopy, authError, type AuthLanguage } from './auth-language';
import GlassSurface from './components/GlassSurface';

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
type SpotifyStatus = {
  configured: boolean;
  connected: boolean;
  display_name: string | null;
};

type YouTubeStatus = SpotifyStatus;
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
  const [language, setLanguage] = useState<AuthLanguage>('es');
  const copy = authCopy[language];

  useEffect(() => {
    document.documentElement.lang = language;
    document.title = copy.title;
    return () => {
      document.documentElement.lang = 'es';
      document.title = authCopy.es.title;
    };
  }, [language, copy.title]);

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
      setMessage(error instanceof Error ? error.message : 'Unknown error');
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="auth-page" lang={language}>
      <header className="auth-topbar">
        <GlassSurface className="auth-nav-glass" width="100%" height="100%" borderRadius={0} backgroundOpacity={0.1}>
          <div className="auth-nav-content">
            <a className="auth-brand" href="/" aria-label="Leopold">
              <img src="/brand/leopold-mark.png" alt="" />
              <span>Leopold</span>
            </a>
            <div className="auth-language" role="group" aria-label={copy.language}>
              {Object.keys(authCopy).map((code) => {
                const selected = language === code;
                const flag = code === 'es' ? '🇲🇽' : '🇺🇸';
                const accessibleName = code === 'es' ? 'Español (México)' : 'English (United States)';
                return (
                  <button
                    key={code}
                    className={`language-flag${selected ? ' selected' : ''}`}
                    type="button"
                    aria-label={accessibleName}
                    aria-pressed={selected}
                    title={accessibleName}
                    lang={code}
                    onClick={() => setLanguage(code as AuthLanguage)}
                  >
                    <span aria-hidden="true">{flag}</span>
                  </button>
                );
              })}
            </div>
          </div>
        </GlassSurface>
      </header>
      <GlassSurface className="auth-card-glass" width="min(100%, 480px)" height="auto" borderRadius={22} backgroundOpacity={0.12}>
        <section className="panel auth-panel">
          <img className="auth-wordmark" src="/brand/leopold-wordmark.png" alt="Leopold" />
          <p className="kicker">{context === 'setup' ? copy.setup : copy.account}</p>
          <h1>{mode === 'register' ? copy.registerHeading : copy.loginHeading}</h1>
          <p className="muted">
            {context === 'setup'
              ? copy.setupDescription
              : copy.accountDescription}
          </p>

          <div className="switcher" aria-label={copy.access}>
            <button className={mode === 'login' ? 'selected' : ''} onClick={() => setMode('login')}>
              {copy.login}
            </button>
            <button className={mode === 'register' ? 'selected' : ''} onClick={() => setMode('register')}>
              {copy.register}
            </button>
          </div>

          <form onSubmit={authenticate}>
            {mode === 'register' && (
              <label>
                {copy.name}
                <input name="display_name" autoComplete="name" required maxLength={80} />
              </label>
            )}
            <label>
              {copy.email}
              <input name="email" type="email" autoComplete="email" required maxLength={254} />
            </label>
            <label>
              {copy.password}
              <input
                name="password"
                type="password"
                autoComplete={mode === 'register' ? 'new-password' : 'current-password'}
                required
                minLength={8}
              />
            </label>
            <button className="button primary" disabled={busy}>
              {busy ? copy.busy : mode === 'register' ? copy.createProfile : copy.submit}
            </button>
          </form>
          {message && <p className="notice error" role="alert">{authError(message, language)}</p>}
        </section>
      </GlassSurface>
    </div>
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

  const ssid = new URLSearchParams(window.location.search).get('ssid') ?? 'Leopold-XXXX';

  if (!claim) {
    return (
      <StatusPanel
        title="Preparando el dispositivo"
        copy="Estamos recuperando la sesión de emparejamiento."
        loading
      />
    );
  }

  if (!user) {
    return <AuthForm context="setup" onAuthenticated={setUser} />;
  }

  if (claim.pending) {
    return <WifiInstructions ssid={ssid} />;
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

  return (
    <StatusPanel
      title={`Vinculando para ${user.display_name}`}
      copy="Esto sólo toma unos segundos. No necesitas hacer nada más."
      loading
      message={message}
    />
  );
}

function WifiInstructions({ ssid }: { ssid: string }) {
  const [copied, setCopied] = useState(false);

  async function copyNetwork() {
    await navigator.clipboard.writeText(ssid);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1800);
  }

  return (
    <section className="panel wifi-panel">
      <div className="wordmark">GOODMORNING</div>
      <p className="kicker">Agregar dispositivo · Paso 2 de 2</p>
      <h1>Conéctate a Leopold.</h1>
      <p className="muted">
        La sesión ya está guardada en tu perfil. Ahora conecta temporalmente el teléfono al dispositivo.
      </p>
      <div className="network-card">
        <span>Red Wi‑Fi</span>
        <strong>{ssid}</strong>
        <button className="text-button" onClick={() => void copyNetwork()}>
          {copied ? 'Copiada' : 'Copiar nombre'}
        </button>
      </div>
      <ol className="setup-steps">
        <li><span>1</span><div><strong>Abre Ajustes → Wi‑Fi</strong><small>Esta página puede quedarse abierta.</small></div></li>
        <li><span>2</span><div><strong>Selecciona {ssid}</strong><small>El portal de Leopold aparecerá automáticamente.</small></div></li>
        <li><span>3</span><div><strong>Elige el Wi‑Fi de tu casa</strong><small>Cuando Leopold tenga internet, terminaremos solos.</small></div></li>
      </ol>
      <div className="waiting-line"><div className="spinner small" /> Esperando conexión del dispositivo…</div>
    </section>
  );
}

function PairingScanner({ onClose }: { onClose: () => void }) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [message, setMessage] = useState('');

  function openPairing(rawValue: string) {
    try {
      const url = new URL(rawValue);
      const apiHost = new URL(apiUrl).host;
      const validHost = url.host === window.location.host || url.host === apiHost;
      const validPath =
        /^\/setup\/[A-Fa-f0-9]{32,64}$/.test(url.pathname) ||
        /^\/v1\/onboarding\/[A-Fa-f0-9]{32,64}\/continue$/.test(url.pathname);
      if (!validHost || !validPath) throw new Error('Este QR no pertenece a un Leopold');
      window.location.assign(url.toString());
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos leer el QR');
    }
  }

  useEffect(() => {
    if (!videoRef.current) return;
    const scanner = new QrScanner(
      videoRef.current,
      (result) => openPairing(result.data),
      {
        highlightScanRegion: true,
        highlightCodeOutline: true,
        returnDetailedScanResult: true,
      },
    );
    void scanner.start().catch(() => {
      setMessage('Permite el acceso a la cámara o selecciona una foto del QR.');
    });
    return () => scanner.destroy();
  }, []);

  async function scanFile(event: React.ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    if (!file) return;
    try {
      const result = await QrScanner.scanImage(file, { returnDetailedScanResult: true });
      openPairing(result.data);
    } catch {
      setMessage('No encontramos un QR de Leopold en esa imagen.');
    }
  }

  return (
    <div className="modal-backdrop" role="dialog" aria-modal="true" aria-label="Escanear Leopold">
      <section className="scanner-modal">
        <div className="modal-heading">
          <div>
            <p className="kicker">Agregar dispositivo · Paso 1 de 2</p>
            <h2>Escanea tu Leopold</h2>
          </div>
          <button className="close-button" onClick={onClose} aria-label="Cerrar">×</button>
        </div>
        <p className="muted">Apunta al QR que aparece en la pantalla del dispositivo.</p>
        <div className="camera-frame"><video ref={videoRef} muted playsInline /></div>
        <label className="button secondary file-button">
          Elegir foto del QR
          <input type="file" accept="image/*" capture="environment" onChange={(event) => void scanFile(event)} />
        </label>
        {message && <p className="notice error">{message}</p>}
      </section>
    </div>
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
  const [showScanner, setShowScanner] = useState(false);
  const [message, setMessage] = useState('');
  const [spotify, setSpotify] = useState<SpotifyStatus | null>(null);
  const [youtube, setYoutube] = useState<YouTubeStatus | null>(null);

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
    void api<SpotifyStatus>('/v1/integrations/spotify/status')
      .then(setSpotify)
      .catch((error: Error) => setMessage(error.message));
    void api<YouTubeStatus>('/v1/integrations/youtube/status')
      .then(setYoutube)
      .catch((error: Error) => setMessage(error.message));
    const result = new URLSearchParams(window.location.search).get('spotify');
    if (result === 'connected') {
      setMessage('Spotify quedó conectado. Leopold recibirá el reproductor automáticamente.');
      window.history.replaceState({}, '', '/');
    } else if (result === 'denied') {
      setMessage('La conexión con Spotify fue cancelada.');
      window.history.replaceState({}, '', '/');
    }
    const youtubeResult = new URLSearchParams(window.location.search).get('youtube');
    if (youtubeResult === 'connected') {
      setMessage('YouTube quedó conectado. Leopold recibirá tu experiencia visual automáticamente.');
      window.history.replaceState({}, '', '/');
    } else if (youtubeResult === 'denied') {
      setMessage('La conexión con YouTube fue cancelada.');
      window.history.replaceState({}, '', '/');
    }
  }, []);

  function connectSpotify() {
    window.location.assign(`${apiUrl}/v1/integrations/spotify/connect`);
  }

  async function disconnectSpotify() {
    if (!window.confirm('¿Desconectar Spotify de todos tus dispositivos Leopold?')) return;
    try {
      await api<void>('/v1/integrations/spotify', { method: 'DELETE' });
      setSpotify({ configured: true, connected: false, display_name: null });
      setMessage('Spotify fue desconectado. Leopold volvió a su pantalla principal.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos desconectar Spotify');
    }
  }

  function connectYoutube() {
    window.location.assign(`${apiUrl}/v1/integrations/youtube/connect`);
  }

  async function disconnectYoutube() {
    if (!window.confirm('¿Desconectar YouTube de todos tus dispositivos Leopold?')) return;
    try {
      await api<void>('/v1/integrations/youtube', { method: 'DELETE' });
      setYoutube({ configured: true, connected: false, display_name: null });
      setMessage('YouTube fue desconectado de Leopold.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'No pudimos desconectar YouTube');
    }
  }

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
          <button className="button primary compact" onClick={() => setShowScanner(true)}>
            Agregar dispositivo
          </button>
        </div>

        {showScanner && <PairingScanner onClose={() => setShowScanner(false)} />}

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

      <section className="services-section">
        <div className="section-heading">
          <div>
            <p className="kicker">Servicios</p>
            <h2>Experiencias</h2>
          </div>
        </div>
        <article className="service-row spotify-service">
          <div className="spotify-mark" aria-hidden="true">●</div>
          <div className="device-copy">
            <strong>Spotify</strong>
            <span>
              {spotify?.connected
                ? `Conectado${spotify.display_name ? ` como ${spotify.display_name}` : ''}`
                : 'Controla la música desde Leopold'}
            </span>
            <small>
              {spotify?.connected
                ? 'El reproductor ya se envía a tus dispositivos'
                : spotify?.configured === false
                  ? 'Pendiente de configurar en el servidor'
                  : 'Requiere Spotify Premium para controlar reproducción'}
            </small>
          </div>
          {spotify?.connected ? (
            <button className="remove-button" onClick={() => void disconnectSpotify()}>
              Desconectar
            </button>
          ) : (
            <button
              className="button spotify-button compact"
              disabled={!spotify?.configured}
              onClick={connectSpotify}
            >
              Conectar
            </button>
          )}
        </article>
        <article className="service-row youtube-service">
          <div className="youtube-mark" aria-hidden="true">▶</div>
          <div className="device-copy">
            <strong>YouTube</strong>
            <span>
              {youtube?.connected
                ? `Conectado${youtube.display_name ? ` como ${youtube.display_name}` : ''}`
                : 'Lleva tus suscripciones y descubrimientos a Leopold'}
            </span>
            <small>
              {youtube?.connected
                ? 'Cloud envía una experiencia visual a tus dispositivos'
                : youtube?.configured === false
                  ? 'Pendiente de configurar OAuth en el servidor'
                  : 'Acceso de sólo lectura; Leopold nunca publica por ti'}
            </small>
          </div>
          {youtube?.connected ? (
            <button className="remove-button" onClick={() => void disconnectYoutube()}>
              Desconectar
            </button>
          ) : (
            <button
              className="button youtube-button compact"
              disabled={!youtube?.configured}
              onClick={connectYoutube}
            >
              Conectar
            </button>
          )}
        </article>
      </section>
      <section className="account-simulator" aria-labelledby="account-simulator-title">
        <div className="section-heading">
          <div>
            <p className="kicker">Explorar Leopold</p>
            <h2 id="account-simulator-title">Simulador</h2>
          </div>
          <a className="button secondary compact" href="/simulator" target="_blank" rel="noopener noreferrer">
            Abrir en otra pestaña
          </a>
        </div>
        <iframe className="account-simulator-frame" src="/simulator" title="Simulador de Leopold" loading="lazy" />
      </section>
    </main>
  );
}

function App() {
  if (window.location.pathname === '/simulator') return <Simulator />;
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
