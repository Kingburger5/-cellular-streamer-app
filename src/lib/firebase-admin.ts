
import { initializeApp, getApps, App, cert, ServiceAccount } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // When deployed to App Hosting, the SDK will automatically use the
        // default service account credentials.
        adminApp = initializeApp({
            storageBucket: "cellular-data-streamer.firebasestorage.app",
        });
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
