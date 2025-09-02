
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

// A wrapper to ensure initialization is only attempted once.
let adminInitPromise: Promise<{ adminApp: App; adminStorage: Storage; }> | null = null;

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
    
    // When deployed on App Hosting, the SDK automatically uses Application Default Credentials.
    // No service account JSON is needed. For local development, ensure you've run
    // `gcloud auth application-default login`.
    console.log("[INFO] Initializing Firebase Admin SDK with Application Default Credentials.");
    
    try {
        adminApp = initializeApp({
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        
        adminStorage = getStorage(adminApp);
        console.log("[INFO] Firebase Admin SDK initialized successfully.");
        return { adminApp, adminStorage };

    } catch (e: any) {
        console.error("[CRITICAL] Failed to initialize Firebase Admin SDK.", e);
        const detail = e instanceof Error 
            ? `Details: ${e.message}`
            : "An unknown error occurred.";
        throw new Error(`Failed to initialize Firebase Admin SDK. This can happen if Application Default Credentials are not configured correctly. ${detail}`);
    }
}

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
