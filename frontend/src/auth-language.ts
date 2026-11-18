export type AuthLanguage = 'es' | 'en';

export const authCopy = {
  es: {
    title: 'Configura Leopold', language: 'Idioma', setup: 'Configurar Leopold', account: 'Tu cuenta',
    registerHeading: 'Crea tu perfil.', loginHeading: 'Inicia sesión.',
    setupDescription: 'Usaremos esta cuenta para guardar tus dispositivos y servicios.',
    accountDescription: 'Administra tus dispositivos Leopold desde un solo lugar.',
    access: 'Elegir acceso', login: 'Iniciar sesión', register: 'Crear cuenta', name: 'Nombre',
    email: 'Correo', password: 'Contraseña', busy: 'Continuando…', createProfile: 'Crear perfil', submit: 'Entrar',
    error: 'No pudimos continuar. Inténtalo de nuevo.',
  },
  en: {
    title: 'Set up Leopold', language: 'Language', setup: 'Set up Leopold', account: 'Your account',
    registerHeading: 'Create your profile.', loginHeading: 'Sign in.',
    setupDescription: 'We’ll use this account to save your devices and services.',
    accountDescription: 'Manage your Leopold devices in one place.',
    access: 'Choose access', login: 'Sign in', register: 'Create account', name: 'Name',
    email: 'Email', password: 'Password', busy: 'Continuing…', createProfile: 'Create profile', submit: 'Sign in',
    error: 'We couldn’t continue. Please try again.',
  },
};

const errors: Record<string, Record<AuthLanguage, string>> = {
  'Invalid email or password': { es: 'Correo o contraseña incorrectos.', en: 'Invalid email or password.' },
  'Email is already registered': { es: 'Este correo ya está registrado.', en: 'This email is already registered.' },
  'Enter a valid email address': { es: 'Introduce un correo válido.', en: 'Enter a valid email address.' },
  'Display name is required': { es: 'Introduce tu nombre.', en: 'Enter your name.' },
};

export function authError(message: string, language: AuthLanguage): string {
  return errors[message]?.[language] ?? authCopy[language].error;
}
