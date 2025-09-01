
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

let adminApp: App | null = null;
let adminStorage: Storage | null = null;

function initializeFirebaseAdmin() {
    if (getApps().length) {
        if (!adminApp) {
           adminApp = getApps()[0];
           adminStorage = getStorage(adminApp);
        }
        return { adminApp, adminStorage: adminStorage! };
    }

    try {
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        if (!serviceAccountString) {
            throw new Error("The GOOGLE_APPLICATION_CREDENTIALS_JSON environment variable is not set.");
        }
        
        const serviceAccount = JSON.parse(serviceAccountString) as ServiceAccount;

        adminApp = initializeApp({
            credential: cert(serviceAccount),
        });
        adminStorage = getStorage(adminApp);

    } catch (error: any) {
        console.error("[CRITICAL] Firebase Admin SDK initialization error:", error.message);
        throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Error: ${error.message}`);
    }

    return { adminApp, adminStorage: adminStorage! };
}

function getAdminStorage(): Storage {
    if (!adminStorage) {
        initializeFirebaseAdmin();
    }
    return adminStorage!;
}

export { getAdminStorage };
