
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import type { ServiceAccount } from "firebase-admin";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

function initializeFirebaseAdmin() {
    if (adminApp) {
        return { adminApp, adminStorage: adminStorage! };
    }

    const appName = 'firebase-admin-app';
    const existingApp = getApps().find(app => app.name === appName);
    if (existingApp) {
        adminApp = existingApp;
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage };
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        
        let credential;

        if (serviceAccountString) {
            // Production environment (App Hosting with secret)
            const serviceAccount = JSON.parse(serviceAccountString) as ServiceAccount;
            if (serviceAccount.private_key) {
                serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
            }
            credential = cert(serviceAccount);
        } else {
            // Local development environment (relies on ADC or GOOGLE_APPLICATION_CREDENTIALS file path)
            console.log("[INFO] GOOGLE_APPLICATION_CREDENTIALS_JSON not found. Using Application Default Credentials for local development.");
            // The SDK will automatically find the credentials
            credential = undefined; 
        }

        adminApp = initializeApp({
            credential: credential ? cert(credential) : undefined,
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, appName);
        
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error);
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs. Original Error: ${error.message}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

export function getAdminStorage(): Storage {
    if (!adminStorage) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}
