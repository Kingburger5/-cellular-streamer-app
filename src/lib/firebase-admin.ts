
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// The principal email of the service account App Hosting runs as.
// This is found on the IAM page in the Google Cloud console.
const SERVICE_ACCOUNT_EMAIL = "firebase-app-hosting-compute@cellular-data-streamer.iam.gserviceaccount.com";

try {
    if (!getApps().length) {
        // When deployed to App Hosting, the SDK will automatically use the
        // default service account credentials. However, since auto-discovery is failing,
        // we explicitly specify the service account to use.
        adminApp = initializeApp({
             credential: cert({
                projectId: process.env.GCLOUD_PROJECT,
                privateKey: process.env.GOOGLE_PRIVATE_KEY, // This should be available in App Hosting
                clientEmail: SERVICE_ACCOUNT_EMAIL,
            }),
        });
    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("Firebase Admin SDK initialization error:", error.message);
    // This error is critical and indicates a problem with the environment setup.
    throw new Error(`Failed to initialize Firebase Admin SDK. Check server logs for details. Error: ${error.message}`);
}


adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
