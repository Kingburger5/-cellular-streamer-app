
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // Attempt to parse the service account from the environment variable.
        // This is a more secure and build-friendly approach than importing a JSON file.
        const serviceAccountString = process.env.GOOGLE_APPLICATION_CREDENTIALS_JSON;
        if (!serviceAccountString) {
            throw new Error("The GOOGLE_APPLICATION_CREDENTIALS_JSON environment variable is not set. Please add it to your .env file.");
        }
        
        const serviceAccount = JSON.parse(serviceAccountString) as ServiceAccount;

        adminApp = initializeApp({
            credential: cert(serviceAccount),
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
