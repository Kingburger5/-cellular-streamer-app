
import { initializeApp, getApps, getApp, App } from "firebase/app";
import { getStorage } from "firebase/storage";
import { getAuth } from 'firebase/auth';

// --- Client-side Firebase Initialization ---
// This is the only configuration this file should contain.
// Server-side initialization is handled in `src/lib/firebase-admin.ts`.

const firebaseConfig = {
  projectId: process.env.NEXT_PUBLIC_FIREBASE_PROJECT_ID,
  appId: process.env.NEXT_PUBLIC_FIREBASE_APP_ID,
  storageBucket: process.env.NEXT_PUBLIC_FIREBASE_STORAGE_BUCKET,
  apiKey: process.env.NEXT_PUBLIC_FIREBASE_API_KEY,
  authDomain: process.env.NEXT_PUBLIC_FIREBASE_AUTH_DOMAIN,
  messagingSenderId: process.env.NEXT_PUBLIC_FIREBASE_MESSAGING_SENDER_ID,
};

function getClientApp(): App {
    if (getApps().length) {
        return getApp();
    }
    return initializeApp(firebaseConfig);
}

// Functions for client-side usage
export const clientStorage = getStorage(getClientApp());
export const clientAuth = getAuth(getClientApp());
