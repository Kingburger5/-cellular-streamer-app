
import { initializeApp, getApps, App, cert, AppOptions } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // Attempt to build credentials from environment variables.
        // This is necessary when automatic discovery fails.
        const projectId = process.env.FIREBASE_PROJECT_ID;
        const clientEmail = process.env.FIREBASE_CLIENT_EMAIL;
        // The private key comes with escaped newlines, which need to be un-escaped.
        const privateKey = process.env.FIREBASE_PRIVATE_KEY?.replace(/\\n/g, '\n');

        if (!projectId || !clientEmail || !privateKey) {
            throw new Error("Missing required Firebase Admin credentials from environment variables (FIREBASE_PROJECT_ID, FIREBASE_CLIENT_EMAIL, FIREBASE_PRIVATE_KEY). Please set them in your App Hosting backend settings.");
        }

        const serviceAccount: ServiceAccount = {
            projectId,
            clientEmail,
            privateKey,
        };

        const appOptions: AppOptions = {
            credential: cert(serviceAccount),
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
