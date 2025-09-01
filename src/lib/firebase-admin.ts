
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import type { ServiceAccount } from "firebase-admin";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

function initializeFirebaseAdmin() {
    // If already initialized, return the existing instances
    if (adminApp && adminStorage) {
        return { adminApp, adminStorage };
    }

    // Use the first initialized app if it exists (common in serverless environments)
    if (getApps().length > 0) {
        adminApp = getApps()[0];
        adminStorage = getStorage(adminApp);
        return { adminApp, adminStorage: adminStorage! };
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        if (!serviceAccountString) {
            throw new Error("The GOOGLE_APPLICATION_CREDENTIALS_JSON environment variable is not set. This is required for server-side operations.");
        }
        
        // Let the SDK handle the parsing. It can take a string path or an object.
        // By passing the raw string to cert(), we avoid our own parsing issues.
        const serviceAccount = JSON.parse(serviceAccountString) as ServiceAccount;

        adminApp = initializeApp({
            credential: cert(serviceAccount),
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        });
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error);
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Error: ${error.message}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

function getAdminStorage(): Storage {
    // Ensure initialization is attempted on every call if not already set.
    if (!adminStorage) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}

export { getAdminStorage };
