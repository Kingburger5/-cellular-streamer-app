
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// When deployed to App Hosting, the SDK will automatically use the
// default service account credentials. When running locally or in other
// environments, it will use the service account key from the environment variable.
const serviceAccountCreds = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;

try {
    if (!getApps().length) {
        if (serviceAccountCreds) {
             adminApp = initializeApp({
                credential: cert(JSON.parse(serviceAccountCreds)),
                storageBucket: "cellular-data-streamer.firebasestorage.app",
            });
        } else {
            // This will use the default credentials in App Hosting.
            adminApp = initializeApp({
                storageBucket: "cellular-data-streamer.firebasestorage.app",
            });
        }
    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("Firebase Admin SDK initialization error:", error.message);
    // Throwing an error here can help diagnose issues early.
    throw new Error("Failed to initialize Firebase Admin SDK. Ensure GOOGLE_SERVICE_ACCOUNT_CREDENTIALS is set correctly if running locally.");
}


adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
