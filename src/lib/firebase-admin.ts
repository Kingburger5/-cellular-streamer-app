
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { SecretManagerServiceClient } from "@google-cloud/secret-manager";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

async function initializeFirebaseAdminImpl(): Promise<{ adminApp: App; adminStorage: Storage }> {
    if (adminApp && adminStorage) {
        return { adminApp, adminStorage };
    }

    const appName = 'firebase-admin-app-cellular-streamer';
    const existingApp = getApps().find(app => app.name === appName);

    if (existingApp) {
        adminApp = existingApp;
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }
    
    // Check if the secret env var for the *secret name* is missing. 
    // This is our condition for local development vs. production.
    if (!process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON || process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON.trim() === '') {
        // Local development: Use Application Default Credentials from `gcloud auth application-default login`
        console.log("[INFO] No GOOGLE_APPLICATION_CREDENTIALS_JSON secret found. Using Application Default Credentials for local development.");
        adminApp = initializeApp({
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }

    // Production environment (App Hosting with secret)
    console.log("[INFO] Initializing Firebase Admin SDK with service account from secret.");
    
    let serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
    console.log(
      "[DEBUG] Secret first 100 chars:",
      JSON.stringify(serviceAccountString.slice(0, 100))
    );

    let serviceAccount: any;
    try {
        // The secret is often delivered as a string literal (double-encoded).
        // We need to manually clean it before parsing.
        if (serviceAccountString.startsWith('"') && serviceAccountString.endsWith('"')) {
            serviceAccountString = serviceAccountString.slice(1, -1).replace(/\\"/g, '"');
        }

        serviceAccount = JSON.parse(serviceAccountString);
        
        // The critical fix for the "Invalid PEM" error caused by environment variable escaping.
        if (serviceAccount.private_key) {
            serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
        }
        
    } catch (e) {
        console.error("[CRITICAL] Failed to parse Firebase service account JSON from secret.", e);
        throw new Error("The service account secret is not valid JSON, even after attempting to clean it.");
    }

    adminApp = initializeApp({
        credential: cert(serviceAccount),
        storageBucket: "cellular-data-streamer.firebasestorage.app"
    }, appName);
    
    adminStorage = getStorage(adminApp);
    console.log("[INFO] Firebase Admin SDK initialized successfully in production.");
    return { adminApp, adminStorage };
}


// A wrapper to ensure initialization is only attempted once.
let adminInitPromise: Promise<{ adminApp: App; adminStorage: Storage; }> | null = null;

function getInitializedAdmin() {
    if (!adminInitPromise) {
        adminInitPromise = initializeFirebaseAdminImpl();
    }
    return adminInitPromise;
}


export async function getAdminStorage(): Promise<Storage> {
    try {
        const { adminStorage } = await getInitializedAdmin();
        return adminStorage;
    } catch (error) {
        console.error("Error getting admin storage:", error)
        // Rethrow to ensure the caller knows initialization failed.
        throw new Error("Could not get Firebase Admin Storage instance. Initialization may have failed.");
    }
}
