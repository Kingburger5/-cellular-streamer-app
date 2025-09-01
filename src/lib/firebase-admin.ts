
import { initializeApp, getApps, App, cert, AppOptions } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";

let adminApp: App;
let adminStorage: Storage;

// The principal email of the service account App Hosting runs as.
// This is found on the IAM page in the Google Cloud console.
const SERVICE_ACCOUNT_EMAIL = "firebase-app-hosting-compute@cellular-data-streamer.iam.gserviceaccount.com";

try {
    if (!getApps().length) {
        // When running in a Google Cloud environment like App Hosting, the SDK
        // should automatically find the service account credentials. However,
        // we are explicitly providing the service account email to ensure
        // the correct identity is used for signing URLs.
        const appOptions: AppOptions = {
            // By specifying only the serviceAccountId, the SDK will use it
            // with the other credentials it finds automatically in the environment.
            serviceAccountId: SERVICE_ACCOUNT_EMAIL,
        };
        adminApp = initializeApp(appOptions);
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
