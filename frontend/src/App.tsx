import { FormEvent, Suspense, lazy, useEffect, useId, useMemo, useRef, useState } from 'react';
import QrScanner from 'qr-scanner';
import { authCopy, authError, pageCopy, type AccountNotice, type AuthLanguage, type ScannerNotice } from './auth-language';
import GlassSurface from './components/GlassSurface';
import GlyphRainBackground from './components/GlyphRainBackground';

const Simulator = lazy(() => import('./simulator/Simulator'));

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

function LeopoldCardNav({
  language,
  onLanguageChange,
  variant,
  onModeChange,
  onLogout,
}: {
  language: AuthLanguage;
  onLanguageChange: (language: AuthLanguage) => void;
  variant: 'auth' | 'account' | 'simulator';
  onModeChange?: (mode: Mode) => void;
  onLogout?: () => void;
}) {
  const [open, setOpen] = useState(false);
  const menuId = useId();
  const copy = authCopy[language];

  useEffect(() => {
    if (!open) return;
    const closeOnOutsideClick = (event: PointerEvent) => {
      if (event.target instanceof Element && !event.target.closest('.auth-card-nav')) setOpen(false);
    };
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setOpen(false);
    };
    document.addEventListener('pointerdown', closeOnOutsideClick);
    document.addEventListener('keydown', closeOnEscape);
    return () => {
      document.removeEventListener('pointerdown', closeOnOutsideClick);
      document.removeEventListener('keydown', closeOnEscape);
    };
  }, [open]);

  return (
    <header className={`auth-topbar${variant === 'auth' ? '' : ' account-nav-topbar'}`}>
      <GlassSurface className="auth-nav-glass" width="100%" height="auto" borderRadius={22} backgroundOpacity={0.1}>
        <nav className={`auth-card-nav${open ? ' is-open' : ''}`} aria-label={copy.navigation}>
          <div className="auth-nav-bar">
            <button
              className="auth-nav-menu"
              type="button"
              aria-expanded={open}
              aria-controls={menuId}
              aria-label={open ? copy.closeMenu : copy.openMenu}
              onClick={() => setOpen((current) => !current)}
            >
              <span className="auth-nav-burger" aria-hidden="true"><i /><i /></span>
              <span>{open ? copy.closeMenu : copy.openMenu}</span>
            </button>
            <a className="auth-brand" href="/" aria-label="Leopold">
              <img src="/brand/leopold-mark.png" alt="" />
              <span>Leopold</span>
            </a>
            <div className="auth-language" role="group" aria-label={copy.language}>
              {(Object.keys(authCopy) as AuthLanguage[]).map((code) => {
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
                    onClick={() => onLanguageChange(code)}
                  >
                    <span aria-hidden="true">{flag}</span>
                  </button>
                );
              })}
            </div>
          </div>
          <div className="auth-nav-disclosure" id={menuId} aria-hidden={!open}>
            <div className="auth-nav-cards">
              {variant === 'auth' ? (
                <>
                  <section className="auth-nav-card" aria-label={copy.access}>
                    <div className="auth-nav-card-heading">{copy.access}</div>
                    <p>{copy.accountDescription}</p>
                    <div className="auth-nav-actions">
                      <button type="button" tabIndex={open ? 0 : -1} onClick={() => { onModeChange?.('login'); setOpen(false); }}>{copy.login}</button>
                      <button type="button" tabIndex={open ? 0 : -1} onClick={() => { onModeChange?.('register'); setOpen(false); }}>{copy.register}</button>
                    </div>
                  </section>
                  <section className="auth-nav-card auth-nav-card-brand" aria-label="Leopold">
                    <img src="/brand/leopold-wordmark.png" alt="Leopold" />
                    <p>{copy.navDescription}</p>
                  </section>
                </>
              ) : (
                <>
                  <section className="auth-nav-card" aria-label={copy.navigation}>
                    <div className="auth-nav-card-heading">{copy.explore}</div>
                    <p>{copy.accountDescription}</p>
                    <div className="auth-nav-actions">
                      <a href={variant === 'account' ? '#devices' : '/#devices'} tabIndex={open ? 0 : -1} onClick={() => setOpen(false)}>{copy.devices}</a>
                      <a href={variant === 'account' ? '#services' : '/#services'} tabIndex={open ? 0 : -1} onClick={() => setOpen(false)}>{copy.services}</a>
                      <a className="primary" href="/simulator" tabIndex={open ? 0 : -1} onClick={() => setOpen(false)}>{copy.simulator}</a>
                    </div>
                    {variant === 'account' && onLogout && (
                      <button className="auth-nav-logout" type="button" tabIndex={open ? 0 : -1} onClick={() => { setOpen(false); onLogout(); }}>
                        {copy.logout}
                      </button>
                    )}
                  </section>
                  <section className="auth-nav-card auth-nav-card-brand" aria-label="Leopold">
                    <img src="/brand/leopold-wordmark.png" alt="Leopold" />
                    <p>{copy.navDescription}</p>
                  </section>
                </>
              )}
            </div>
          </div>
        </nav>
      </GlassSurface>
    </header>
  );
}

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
  language,
  onLanguageChange,
  onAuthenticated,
}: {
  context: 'account' | 'setup';
  language: AuthLanguage;
  onLanguageChange: (language: AuthLanguage) => void;
  onAuthenticated: (user: User) => void;
}) {
  const copy = authCopy[language];

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
      <GlyphRainBackground />
      <LeopoldCardNav language={language} onLanguageChange={onLanguageChange} variant="auth" onModeChange={setMode} />
      <GlassSurface className="auth-card-glass" width="min(100%, 480px)" height="auto" borderRadius={22} backgroundOpacity={0.1}>
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

function SetupFlow({
  token,
  language,
  onLanguageChange,
}: {
  token: string;
  language: AuthLanguage;
  onLanguageChange: (language: AuthLanguage) => void;
}) {
  const copy = pageCopy[language].setup;
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
        if (!stopped) setMessage(pageCopy[language].requestError);
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
  }, [token, language]);

  useEffect(() => {
    if (!user || !claim?.valid || claim.claimed || autoClaimStarted.current) return;
    autoClaimStarted.current = true;
    void api<ClaimStatus>(`/v1/onboarding/${token}/claim`, { method: 'POST' })
      .then(setClaim)
      .catch(() => {
        autoClaimStarted.current = false;
        setMessage(pageCopy[language].requestError);
      });
  }, [claim, language, token, user]);

  const ssid = new URLSearchParams(window.location.search).get('ssid') ?? 'Leopold-XXXX';

  if (!claim) {
    return (
      <StatusPanel title={copy.preparing} copy={copy.recovering} loading />
    );
  }

  if (!user) {
    return <AuthForm context="setup" language={language} onLanguageChange={onLanguageChange} onAuthenticated={setUser} />;
  }

  if (claim.pending) {
    return <WifiInstructions ssid={ssid} language={language} />;
  }

  if (claim.claimed) {
    return (
      <StatusPanel
        title={copy.ready}
        copy={copy.readyDescription}
        action={<a className="button primary" href="/">{copy.goToDevices}</a>}
      />
    );
  }

  if (!claim.valid) {
    return (
      <StatusPanel
        title={copy.expired}
        copy={copy.expiredDescription}
        action={<a className="button secondary" href="/">{copy.backToAccount}</a>}
      />
    );
  }

  return (
    <StatusPanel
      title={copy.linking(user.display_name)}
      copy={copy.linkingDescription}
      loading
      message={message}
    />
  );
}

function WifiInstructions({ ssid, language }: { ssid: string; language: AuthLanguage }) {
  const copy = pageCopy[language].setup;
  const [copied, setCopied] = useState(false);

  async function copyNetwork() {
    await navigator.clipboard.writeText(ssid);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1800);
  }

  return (
    <section className="panel wifi-panel">
      <div className="wordmark">GOODMORNING</div>
      <p className="kicker">{copy.step}</p>
      <h1>{copy.connectHeading}</h1>
      <p className="muted">{copy.sessionSaved}</p>
      <div className="network-card">
        <span>{copy.wifiNetwork}</span>
        <strong>{ssid}</strong>
        <button className="text-button" onClick={() => void copyNetwork()}>
          {copied ? copy.copied : copy.copyName}
        </button>
      </div>
      <ol className="setup-steps">
        <li><span>1</span><div><strong>{copy.wifiStepOneTitle}</strong><small>{copy.wifiStepOneCopy}</small></div></li>
        <li><span>2</span><div><strong>{copy.wifiStepTwoTitle(ssid)}</strong><small>{copy.wifiStepTwoCopy}</small></div></li>
        <li><span>3</span><div><strong>{copy.wifiStepThreeTitle}</strong><small>{copy.wifiStepThreeCopy}</small></div></li>
      </ol>
      <div className="waiting-line"><div className="spinner small" /> {copy.waitingForDevice}</div>
    </section>
  );
}

function PairingScanner({ onClose, language }: { onClose: () => void; language: AuthLanguage }) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [message, setMessage] = useState<ScannerNotice | null>(null);
  const copy = pageCopy[language].scanner;

  function openPairing(rawValue: string) {
    try {
      const url = new URL(rawValue);
      const apiHost = new URL(apiUrl).host;
      const validHost = url.host === window.location.host || url.host === apiHost;
      const validPath =
        /^\/setup\/[A-Fa-f0-9]{32,64}$/.test(url.pathname) ||
        /^\/v1\/onboarding\/[A-Fa-f0-9]{32,64}\/continue$/.test(url.pathname);
      if (!validHost || !validPath) {
        setMessage('invalidCode');
        return;
      }
      window.location.assign(url.toString());
    } catch {
      setMessage('invalidCode');
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
      setMessage('cameraPermission');
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
      setMessage('noCode');
    }
  }

  return (
    <div className="modal-backdrop" role="dialog" aria-modal="true" aria-label={copy.dialog}>
      <section className="scanner-modal">
        <div className="modal-heading">
          <div>
            <p className="kicker">{copy.step}</p>
            <h2>{copy.title}</h2>
          </div>
          <button className="close-button" onClick={onClose} aria-label={copy.close}>×</button>
        </div>
        <p className="muted">{copy.instructions}</p>
        <div className="camera-frame"><video ref={videoRef} muted playsInline /></div>
        <label className="button secondary file-button">
          {copy.choosePhoto}
          <input type="file" accept="image/*" capture="environment" onChange={(event) => void scanFile(event)} />
        </label>
        {message && <p className="notice error">{copy[message]}</p>}
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

function Profile({
  user,
  onLogout,
  language,
  onLanguageChange,
}: {
  user: User;
  onLogout: () => void;
  language: AuthLanguage;
  onLanguageChange: (language: AuthLanguage) => void;
}) {
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState(true);
  const [showScanner, setShowScanner] = useState(false);
  const [message, setMessage] = useState<AccountNotice | null>(null);
  const [spotify, setSpotify] = useState<SpotifyStatus | null>(null);
  const [youtube, setYoutube] = useState<YouTubeStatus | null>(null);
  const copy = pageCopy[language];

  async function loadDevices() {
    setLoading(true);
    try {
      setDevices(await api<Device[]>('/v1/me/devices'));
    } catch {
      setMessage('devicesLoadError');
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadDevices();
    void api<SpotifyStatus>('/v1/integrations/spotify/status')
      .then(setSpotify)
      .catch(() => setMessage('servicesLoadError'));
    void api<YouTubeStatus>('/v1/integrations/youtube/status')
      .then(setYoutube)
      .catch(() => setMessage('servicesLoadError'));
    const result = new URLSearchParams(window.location.search).get('spotify');
    if (result === 'connected') {
      setMessage('spotifyConnected');
      window.history.replaceState({}, '', '/');
    } else if (result === 'denied') {
      setMessage('spotifyDenied');
      window.history.replaceState({}, '', '/');
    }
    const youtubeResult = new URLSearchParams(window.location.search).get('youtube');
    if (youtubeResult === 'connected') {
      setMessage('youtubeConnected');
      window.history.replaceState({}, '', '/');
    } else if (youtubeResult === 'denied') {
      setMessage('youtubeDenied');
      window.history.replaceState({}, '', '/');
    }
  }, []);

  function connectSpotify() {
    window.location.assign(`${apiUrl}/v1/integrations/spotify/connect`);
  }

  async function disconnectSpotify() {
    if (!window.confirm(copy.confirmDisconnectSpotify)) return;
    try {
      await api<void>('/v1/integrations/spotify', { method: 'DELETE' });
      setSpotify({ configured: true, connected: false, display_name: null });
      setMessage('spotifyDisconnected');
    } catch {
      setMessage('actionError');
    }
  }

  function connectYoutube() {
    window.location.assign(`${apiUrl}/v1/integrations/youtube/connect`);
  }

  async function disconnectYoutube() {
    if (!window.confirm(copy.confirmDisconnectYoutube)) return;
    try {
      await api<void>('/v1/integrations/youtube', { method: 'DELETE' });
      setYoutube({ configured: true, connected: false, display_name: null });
      setMessage('youtubeDisconnected');
    } catch {
      setMessage('actionError');
    }
  }

  async function removeDevice(device: Device) {
    const name = device.label || device.device_id;
    if (!window.confirm(copy.confirmRemoveDevice(name))) return;
    setMessage(null);
    try {
      await api<void>(`/v1/me/devices/${device.device_id}`, { method: 'DELETE' });
      setDevices((current) => current.filter((item) => item.device_id !== device.device_id));
    } catch {
      setMessage('deviceRemoveError');
    }
  }

  return (
    <main className="account-shell" lang={language}>
      <GlyphRainBackground />
      <LeopoldCardNav language={language} onLanguageChange={onLanguageChange} variant="account" onLogout={onLogout} />
      <div className="account-content">
      <section className="account-heading" id="profile">
        <p className="kicker">{copy.profile}</p>
        <h1>{copy.greeting} {user.display_name}</h1>
        <p className="muted">{user.email}</p>
      </section>

      <section className="devices-section" id="devices">
        <div className="section-heading">
          <div>
            <p className="kicker">{copy.myDevices}</p>
            <h2>{copy.devicesTitle}</h2>
          </div>
          <button className="button primary compact" onClick={() => setShowScanner(true)}>
            {copy.addDevice}
          </button>
        </div>

        {showScanner && <PairingScanner onClose={() => setShowScanner(false)} language={language} />}

        {loading ? (
          <div className="empty"><div className="spinner small" /> {copy.loadingDevices}</div>
        ) : devices.length === 0 ? (
          <div className="empty">
            <strong>{copy.noDevices}</strong>
            <span>{copy.addFirstDevice}</span>
          </div>
        ) : (
          <div className="device-list">
            {devices.map((device) => (
              <article className="device-row" key={device.device_id}>
                <div className="device-icon"><img src="/brand/leopold-mark.png" alt="" /></div>
                <div className="device-copy">
                  <strong>{device.label || device.device_id}</strong>
                  <span>{device.device_id}</span>
                  <small>
                    <i className={device.status === 'online' ? 'online' : ''} />
                    {device.status === 'online' ? copy.online : copy.offline}
                    {device.firmware_version ? copy.firmwareVersion(device.firmware_version) : ''}
                  </small>
                </div>
                <button className="remove-button" onClick={() => void removeDevice(device)}>
                  {copy.removeDevice}
                </button>
              </article>
            ))}
          </div>
        )}
        {message && <p className="notice error" role="alert">{copy.notices[message]}</p>}
      </section>

      <section className="services-section" id="services">
        <div className="section-heading">
          <div>
            <p className="kicker">{copy.services}</p>
            <h2>{copy.experiences}</h2>
          </div>
        </div>
        <article className="service-row spotify-service">
          <img className="spotify-mark" src="/brand/spotify-icon.svg" alt="Spotify" />
          <div className="device-copy">
            <strong>Spotify</strong>
            <span>
              {spotify?.connected
                ? copy.spotifyConnected(spotify.display_name)
                : copy.spotifyDescription}
            </span>
            <small>
              {spotify?.connected
                ? copy.spotifySynced
                : spotify?.configured === false
                  ? copy.spotifyNotConfigured
                  : copy.spotifyPremium}
            </small>
          </div>
          {spotify?.connected ? (
            <button className="remove-button" onClick={() => void disconnectSpotify()}>
              {copy.spotifyDisconnect}
            </button>
          ) : (
            <button
              className="button service-connect-button compact"
              disabled={!spotify?.configured}
              onClick={connectSpotify}
            >
              {copy.spotifyConnect}
            </button>
          )}
        </article>
        <article className="service-row youtube-service">
          <div className="youtube-mark" aria-hidden="true">▶</div>
          <div className="device-copy">
            <strong>YouTube</strong>
            <span>
              {youtube?.connected
                ? copy.youtubeConnected(youtube.display_name)
                : copy.youtubeDescription}
            </span>
            <small>
              {youtube?.connected
                ? copy.youtubeSynced
                : youtube?.configured === false
                  ? copy.youtubeNotConfigured
                  : copy.youtubeReadOnly}
            </small>
          </div>
          {youtube?.connected ? (
            <button className="remove-button" onClick={() => void disconnectYoutube()}>
              {copy.youtubeDisconnect}
            </button>
          ) : (
            <button
              className="button service-connect-button compact"
              disabled={!youtube?.configured}
              onClick={connectYoutube}
            >
              {copy.youtubeConnect}
            </button>
          )}
        </article>
      </section>
      </div>
    </main>
  );
}

function SimulatorPage({
  language,
  onLanguageChange,
}: {
  language: AuthLanguage;
  onLanguageChange: (language: AuthLanguage) => void;
}) {
  const copy = pageCopy[language];
  return (
    <div className="account-shell simulator-account-shell" lang={language}>
      <GlyphRainBackground />
      <LeopoldCardNav language={language} onLanguageChange={onLanguageChange} variant="simulator" />
      <Suspense fallback={<div className="simulator-loading" role="status">{copy.loadingSimulator}</div>}>
        <Simulator />
      </Suspense>
    </div>
  );
}

function App() {
  const isSimulator = window.location.pathname === '/simulator';
  const token = useMemo(currentSetupToken, []);
  const [language, setLanguage] = useState<AuthLanguage>('es');
  const copy = authCopy[language];
  const page = pageCopy[language];
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(!token && !isSimulator);

  useEffect(() => {
    document.documentElement.lang = language;
    document.title = copy.title;
  }, [language, copy.title]);

  useEffect(() => {
    if (token || isSimulator) return;
    void api<User>('/v1/auth/me')
      .then(setUser)
      .catch(() => null)
      .finally(() => setLoading(false));
  }, [token, isSimulator]);

  async function logout() {
    await api<void>('/v1/auth/logout', { method: 'POST' });
    setUser(null);
  }

  if (isSimulator) return <SimulatorPage language={language} onLanguageChange={setLanguage} />;
  if (token) return <main className="center-shell"><SetupFlow token={token} language={language} onLanguageChange={setLanguage} /></main>;
  if (loading) return <main className="center-shell"><StatusPanel title={page.loadingProfile} copy="" loading /></main>;
  if (!user) return <main className="center-shell"><AuthForm context="account" language={language} onLanguageChange={setLanguage} onAuthenticated={setUser} /></main>;
  return <Profile user={user} onLogout={() => void logout()} language={language} onLanguageChange={setLanguage} />;
}

export default App;
