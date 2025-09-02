
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
    
    // This is the key condition for distinguishing local dev from production.
    // In App Hosting, this secret WILL be present. Locally, it will not.
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
    
    let serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
    console.log("[DEBUG] Raw secret first 100 chars:", serviceAccountString.slice(0, 100));

    let serviceAccount: ServiceAccount;

    try {
        // Step 0: Remove potential outer quotes if the whole thing is a string literal
        if (serviceAccountString.startsWith('"') && serviceAccountString.endsWith('"')) {
            serviceAccountString = serviceAccountString.slice(1, -1);
        }

        // Step 1: Un-escape the inner quotes. This is critical.
        serviceAccountString = serviceAccountString.replace(/\\"/g, '"');
        
        // Step 2 & 3: Parse the now-clean JSON string.
        let finalParsed = JSON.parse(serviceAccountString);

        // It might be double-encoded, so if it's still a string, parse again.
        if (typeof finalParsed === 'string') {
          finalParsed = JSON.parse(finalParsed);
        }
        
        // For debugging: log the parsed credential object, redacting the private key.
        console.log('[DEBUG] Parsed credential object:', JSON.stringify({ ...finalParsed, private_key: '[REDACTED]' }));

        // Step 4: The key is not a valid PEM format until newlines are un-escaped.
        const privateKey = finalParsed.private_key.replace(/\\n/g, '\n');

        serviceAccount = {
            projectId: finalParsed.project_id,
            clientEmail: finalParsed.client_email,
            privateKey: privateKey,
        };
        
    } catch (e: any) {
        console.error("[CRITICAL] Failed to parse Firebase service account JSON from secret.", e);
        const detail = e instanceof Error 
            ? `The service account credentials from the secret are not valid JSON. Details: ${e.message}`
            : "An unknown parsing error occurred.";
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs. Detail: ${detail}`);
    }

    adminApp = initializeApp({
        credential: cert(serviceAccount),
        storageBucket: "cellular-data-streamer.firebasestorage.app"
    }, appName);
    
    adminStorage = getStorage(adminApp);
    console.log("[INFO] Firebase Admin SDK initialized successfully in production.");
    return { adminApp, adminStorage };
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
