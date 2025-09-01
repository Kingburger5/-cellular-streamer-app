
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

// Import the service account key from the new JSON file.
// The `resolveJsonModule` and `esModuleInterop` in tsconfig.json allow this direct import.
import serviceAccountCredentials from './service-account.json';

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // Cast the imported JSON to the ServiceAccount type for type safety.
        const serviceAccount = serviceAccountCredentials as ServiceAccount;

        adminApp = initializeApp({
            credential: cert(serviceAccount),
            // The default bucket name is no longer needed here,
            // as we will specify it explicitly in each action.
        });

    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("[CRITICAL] Firebase Admin SDK initialization error:", error.message);
    // This error is critical and indicates a problem with the environment setup.
    // It will cause the backend to fail to start, and the error will be in the logs.
    throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Error: ${error.message}`);
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
