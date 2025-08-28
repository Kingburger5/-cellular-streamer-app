
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// This is the default bucket name for Firebase projects.
const BUCKET_NAME = "cellular-data-streamer.appspot.com";

try {
    if (!getApps().length) {
        // When running locally, use the service account key from the .env file.
        // When deployed to App Hosting, the SDK will automatically use the
        // default service account credentials.
        const serviceAccountCreds = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;
        if (serviceAccountCreds) {
             adminApp = initializeApp({
                credential: cert(JSON.parse(serviceAccountCreds)),
                storageBucket: BUCKET_NAME,
            });
        } else {
            // This will use the default credentials in the App Hosting environment.
            adminApp = initializeApp({
                storageBucket: BUCKET_NAME,
            });
        }
    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("Firebase Admin SDK initialization error:", error.message);
    // Throwing an error here can help diagnose issues early.
    throw new Error("Failed to initialize Firebase Admin SDK. If running locally, ensure GOOGLE_SERVICE_ACCOUNT_CREDENTIALS is set correctly in your .env file.");
}


adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
