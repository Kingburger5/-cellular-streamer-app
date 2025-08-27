
import { initializeApp, getApps, getApp, App } from "firebase/app";
import { getStorage } from "firebase/storage";
import { getAuth, signInAnonymously } from 'firebase/auth';
import { initializeApp as initializeAdminApp, getApps as getAdminApps, getApp as getAdminApp, App as AdminApp } from 'firebase-admin/app';
import { getStorage as getAdminStorage } from 'firebase-admin/storage';
// The Admin SDK, when run in an App Hosting environment, automatically uses the service account credentials
// without needing to explicitly create a credential object.

// --- Client-side Firebase Initialization ---
// This is used by browser components for actions like anonymous sign-in.
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

function getClientAuth() {
    return getAuth(getClientApp());
}

// --- Server-side Firebase Admin Initialization ---
// This is used by Server Actions to access storage with privileged permissions.
// This function ensures the Admin App is initialized only once.
function initializeAdminAppOnce(): AdminApp {
    if (getAdminApps().length) {
        return getAdminApp(); // This uses the imported getAdminApp
    }
    // This will automatically use the App Hosting service account credentials,
    // but it needs to be told which storage bucket to use.
    return initializeAdminApp({
        storageBucket: process.env.GCLOUD_STORAGE_BUCKET,
    });
}

const adminApp = initializeAdminAppOnce();
const adminStorage = getAdminStorage(adminApp);
const clientStorage = getStorage(getClientApp());

export { adminStorage, clientStorage, getClientAuth, signInAnonymously };
