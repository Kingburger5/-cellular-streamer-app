
import { initializeApp, getApps, App, cert, AppOptions } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // This is the standard initialization that should be used in App Hosting.
        // It relies on Application Default Credentials to be automatically discovered.
        adminApp = initializeApp();
    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("Firebase Admin SDK initialization error:", error.message);
    // This error is critical and indicates a problem with the environment setup.
    // It will cause the backend to fail to start, and the error will be in the logs.
    throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Error: ${error.message}`);
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
