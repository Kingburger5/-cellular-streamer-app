
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        const serviceAccountKey = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;
        const bucketName = "cellular-data-streamer.appspot.com";

        if (serviceAccountKey) {
            const serviceAccount = JSON.parse(serviceAccountKey) as ServiceAccount;
            adminApp = initializeApp({
                credential: cert(serviceAccount),
                storageBucket: bucketName,
            });
        } else {
            // This fallback is for local development or environments without the secret set.
            // It uses Application Default Credentials.
            adminApp = initializeApp({
                storageBucket: bucketName,
            });
        }
    } else {
        adminApp = getApps()[0];
    }
} catch (e: any) {
    console.error("Firebase Admin SDK initialization failed:", e);
    // If the app is already initialized, get the existing instance. This can happen during hot-reloads.
    if (e.code === 'app/duplicate-app' && getApps().length > 0) {
        adminApp = getApps()[0];
    } else {
        throw e; // Re-throw other errors
    }
}


adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
