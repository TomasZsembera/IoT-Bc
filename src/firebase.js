import { initializeApp, getApps, getApp } from 'firebase/app'
import { getAuth } from 'firebase/auth'
import { getDatabase } from 'firebase/database'

const useMockData = import.meta.env.VITE_USE_MOCK_DATA === 'true'

const firebaseConfig = {
    apiKey: import.meta.env.VITE_FIREBASE_API_KEY,
    authDomain: import.meta.env.VITE_FIREBASE_AUTH_DOMAIN,
    databaseURL: import.meta.env.VITE_FIREBASE_DATABASE_URL,
    projectId: import.meta.env.VITE_FIREBASE_PROJECT_ID,
    storageBucket: import.meta.env.VITE_FIREBASE_STORAGE_BUCKET,
    messagingSenderId: import.meta.env.VITE_FIREBASE_MESSAGING_SENDER_ID,
    appId: import.meta.env.VITE_FIREBASE_APP_ID,
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
