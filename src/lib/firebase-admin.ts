
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
    
    // In App Hosting, this secret WILL be present. Locally, it will be empty.
    if (!process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON || process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON.trim() === '') {
        // LOCAL DEVELOPMENT: Use Application Default Credentials.
        console.log("[INFO] No GOOGLE_APPLICATION_CREDENTIALS_JSON found. Using Application Default Credentials for local dev.");
        adminApp = initializeApp({
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }

    // PRODUCTION: Parse the secret from the environment variable.
    console.log("[INFO] Initializing Firebase Admin SDK with service account from secret.");
    
    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        const serviceAccount: ServiceAccount = JSON.parse(serviceAccountString);

        // The private_key needs to have its escaped newlines replaced with actual newlines.
        const privateKey = serviceAccount.private_key?.replace(/\\n/g, '\n');

        adminApp = initializeApp({
            credential: cert({
                projectId: serviceAccount.project_id,
                clientEmail: serviceAccount.client_email,
                privateKey: privateKey,
            }),
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        
        adminStorage = getStorage(adminApp);
        console.log("[INFO] Firebase Admin SDK initialized successfully in production.");
        return { adminApp, adminStorage };

    } catch (e: any) {
        console.error("[CRITICAL] Failed to parse Firebase service account JSON from secret.", e);
        const detail = e instanceof Error 
            ? `The service account credentials from the secret are not valid JSON. Details: ${e.message}`
            : "An unknown parsing error occurred.";
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs. Detail: ${detail}`);
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
