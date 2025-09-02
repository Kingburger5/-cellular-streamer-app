
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
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
    
    // Check for the service account JSON in the environment variable.
    // This is the recommended approach for App Hosting to provide full admin privileges.
    const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
    let credential;

    if (serviceAccountString) {
        console.log("[INFO] Initializing Firebase Admin SDK with service account from environment variable.");
        try {
            const serviceAccount: ServiceAccount = JSON.parse(serviceAccountString);
            
            // The private key from environment variables often has escaped newlines.
            // We need to replace them with actual newlines for the PEM format to be valid.
            if (serviceAccount.private_key) {
                serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
            }

            credential = cert(serviceAccount);
        } catch (error) {
            console.error("[CRITICAL] Failed to parse service account JSON from environment variable.", error);
            throw new Error("Could not initialize Firebase Admin SDK. The service account JSON is malformed.");
        }
    } else {
        // Fallback to Application Default Credentials for local development or if the env var is not set.
        // This may have limited permissions (e.g., cannot sign URLs).
        console.log("[INFO] Initializing Firebase Admin SDK with Application Default Credentials.");
        credential = undefined; // initializeApp will use ADC by default
    }
    
    try {
        adminApp = initializeApp({
            credential,
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        
        adminStorage = getStorage(adminApp);
        console.log("[INFO] Firebase Admin SDK initialized successfully.");
        return { adminApp, adminStorage };

    } catch (e: any) {
        console.error("[CRITICAL] Failed to initialize Firebase Admin SDK.", e);
        const detail = e instanceof Error ? `Details: ${e.message}` : "An unknown error occurred.";
        throw new Error(`Failed to initialize Firebase Admin SDK. ${detail}`);
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
