
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// This pattern ensures that we only initialize the app once.
if (!getApps().length) {
    const serviceAccountJson = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;

    if (!serviceAccountJson) {
        // This will cause a loud and clear error if the environment variable is not set.
        throw new Error("FATAL: GOOGLE_SERVICE_ACCOUNT_CREDENTIALS environment variable is not set.");
    }

    try {
        const credentials = JSON.parse(serviceAccountJson);
        adminApp = initializeApp({
            credential: cert(credentials),
            // Explicitly set the project ID from the credentials to avoid any ambiguity.
            projectId: credentials.project_id, 
        });
    } catch (error: any) {
        // This will catch any errors from parsing the JSON credentials.
        throw new Error(`Error initializing Firebase Admin SDK: ${error.message}`);
    }

} else {
    adminApp = getApps()[0];
}

adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
