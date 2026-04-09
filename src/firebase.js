import { initializeApp, getApps, getApp } from 'firebase/app'
import { getAuth } from 'firebase/auth'
import { getDatabase } from 'firebase/database'

const env = import.meta.env
const useMockData = String(env.VITE_USE_MOCK_DATA).toLowerCase() === 'true'

function readEnvValue(key) {
    const value = env[key]
    return typeof value === 'string' ? value.trim() : value
}

const firebaseConfig = {
    apiKey: readEnvValue('VITE_FIREBASE_API_KEY'),
    authDomain: readEnvValue('VITE_FIREBASE_AUTH_DOMAIN'),
    databaseURL: readEnvValue('VITE_FIREBASE_DATABASE_URL'),
    projectId: readEnvValue('VITE_FIREBASE_PROJECT_ID'),
    storageBucket: readEnvValue('VITE_FIREBASE_STORAGE_BUCKET'),
    messagingSenderId: readEnvValue('VITE_FIREBASE_MESSAGING_SENDER_ID'),
    appId: readEnvValue('VITE_FIREBASE_APP_ID'),
}

const requiredKeys = ['apiKey', 'authDomain', 'databaseURL', 'projectId', 'appId']
const isFirebaseConfigured = requiredKeys.every((key) => Boolean(firebaseConfig[key]))

const app = isFirebaseConfigured
    ? getApps().length
        ? getApp()
        : initializeApp(firebaseConfig)
    : null

const auth = app ? getAuth(app) : null
const db = app ? getDatabase(app) : null

export { auth, db, isFirebaseConfigured, useMockData }
