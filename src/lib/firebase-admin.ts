
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage, Storage } from "firebase-admin/storage";
import { ServiceAccount } from "firebase-admin";

let adminApp: App;
let adminStorage: Storage;

try {
    if (!getApps().length) {
        // This initialization reads credentials from environment variables.
        // It's the most robust method when automatic discovery fails.
        // These variables MUST be set as secrets in the App Hosting backend settings.
        const serviceAccount: ServiceAccount = {
            projectId: process.env.FIREBASE_PROJECT_ID!,
            clientEmail: process.env.FIREBASE_CLIENT_EMAIL!,
            // Replace newlines in the private key with the literal `\n` sequence
            privateKey: process.env.FIREBASE_PRIVATE_KEY!.replace(/\\n/g, '\n'),
        };

        // Validate that the environment variables are set.
        if (!serviceAccount.projectId || !serviceAccount.clientEmail || !serviceAccount.privateKey) {
             throw new Error("Firebase Admin credentials (FIREBASE_PROJECT_ID, FIREBASE_CLIENT_EMAIL, FIREBASE_PRIVATE_KEY) are not set in the environment. Please configure them as secrets in your App Hosting backend.");
        }

        adminApp = initializeApp({
            credential: cert(serviceAccount),
            storageBucket: `${serviceAccount.projectId}.appspot.com`
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
