
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import type { ServiceAccount } from "firebase-admin";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

function initializeFirebaseAdmin() {
    if (adminApp) {
        return { adminApp, adminStorage: adminStorage! };
    }

    const appName = 'firebase-admin-app-cellular-streamer';
    const existingApp = getApps().find(app => app.name === appName);

    if (existingApp) {
        adminApp = existingApp;
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        
        if (serviceAccountString) {
            console.log("[INFO] Initializing Firebase Admin SDK with service account from secret.");
            
            let serviceAccount: ServiceAccount;
            try {
                // First, try to parse the string directly, assuming it's single-encoded.
                serviceAccount = JSON.parse(serviceAccountString);
            } catch (e) {
                // If that fails, it's likely double-encoded. Parse it again.
                console.log("[INFO] Direct parse failed. Attempting to parse double-encoded JSON.");
                serviceAccount = JSON.parse(JSON.parse(serviceAccountString));
            }
            
            // The critical fix for the "Invalid PEM" error caused by environment variable escaping.
            if (serviceAccount.private_key) {
                serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
            }

            adminApp = initializeApp({
                credential: cert(serviceAccount),
                storageBucket: "cellular-data-streamer.firebasestorage.app"
            }, appName);
            
        } else {
            // Local development environment (relies on ADC from `gcloud auth application-default login`)
            console.log("[INFO] No GOOGLE_APPLICATION_CREDENTIALS_JSON secret found. Using Application Default Credentials for local development.");
            adminApp = initializeApp({
                storageBucket: "cellular-data-streamer.firebasestorage.app"
            }, appName);
        }
        
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error);
        // Provide a clearer error message that helps diagnose the issue
        const detail = error.message.includes("Invalid PEM") 
            ? "The private key in the service account credentials is malformed. This often happens due to incorrect newline character escaping when passed as an environment variable."
            : error.message.includes("JSON") 
            ? `The service account credentials from the secret are not valid JSON. Details: ${error.message}`
            : error.message;
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs. Detail: ${detail}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

export function getAdminStorage(): Storage {
    if (!adminStorage) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}
