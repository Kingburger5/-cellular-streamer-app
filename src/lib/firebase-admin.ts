
import { initializeApp, getApps, App } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// When deployed to App Hosting, the BUCKET_NAME is not needed
// as the SDK will automatically use the project's default bucket.
// const BUCKET_NAME = "cellular-data-streamer.appspot.com";

try {
    if (!getApps().length) {
        // When deployed to App Hosting, the SDK will automatically use the
        // default service account credentials. This is the recommended approach.
        adminApp = initializeApp();
    } else {
        adminApp = getApps()[0];
    }
} catch (error: any) {
    console.error("Firebase Admin SDK initialization error:", error.message);
    // This error is critical and indicates a problem with the environment setup.
    throw new Error("Failed to initialize Firebase Admin SDK. Check server logs for details.");
}


adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
