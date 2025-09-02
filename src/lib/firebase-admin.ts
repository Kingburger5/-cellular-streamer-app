
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
        const existingApp = getApps()[0];
        // Ensure that if the existing app is used, our adminApp and adminStorage variables are set.
        if (existingApp) {
            adminApp = existingApp;
            adminStorage = getStorage(adminApp);
            return { adminApp, adminStorage: adminStorage! };
        }
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        if (!serviceAccountString) {
            throw new Error("The GOOGLE_APPLICATION_CREDENTIALS_JSON environment variable is not set. This is required for server-side operations.");
        }
        
        const serviceAccount = JSON.parse(serviceAccountString) as ServiceAccount;

        // **CRITICAL FIX for "Invalid PEM formatted message" error**
        // The `private_key` from the environment variable often has its newlines
        // escaped (e.g., "\\n"). We must replace them with actual newlines ("\n")
        // for the PEM key to be valid.
        if (serviceAccount.private_key) {
            serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
        }

        adminApp = initializeApp({
            credential: cert(serviceAccount),
            storageBucket: "cellular-data-streamer.firebasestorage.app"
        }, 'firebase-admin-app'); // Assign a unique name to avoid conflicts
        
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error);
        // Include the original error message for better debugging.
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Original Error: ${error.message}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

function getAdminStorage(): Storage {
    // Ensure initialization is attempted on every call if not already set.
    // This makes the function resilient to module-level caching issues.
    if (!adminStorage || !adminApp) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}

export { getAdminStorage };
