
import { initializeApp, getApps, App, cert } from "firebase-admin/app";
import { getStorage } from "firebase-admin/storage";

let adminApp: App;

if (!getApps().length) {
    const projectId = process.env.FIREBASE_PROJECT_ID;
    if (!projectId) {
        throw new Error("FIREBASE_PROJECT_ID environment variable is not set.");
    }
    
    // In a deployed App Hosting environment, GOOGLE_SERVICE_ACCOUNT_CREDENTIALS
    // may not be set if it's using the default compute service account.
    // initializeApp() will automatically use the available service account.
    const serviceAccountEnv = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;

    if (serviceAccountEnv) {
        try {
            const serviceAccount = JSON.parse(serviceAccountEnv);
            adminApp = initializeApp({
                credential: cert(serviceAccount),
                projectId: projectId,
                storageBucket: `${projectId}.appspot.com`
            });
        } catch (e: any) {
            throw new Error(`Failed to parse service account credentials: ${e.message}`);
        }
    } else {
        // This is the standard way to initialize in a Google Cloud environment
        // like App Hosting, where credentials are automatically discovered.
        adminApp = initializeApp({
             projectId: projectId,
             storageBucket: `${projectId}.appspot.com`
        });
    }

} else {
    adminApp = getApps()[0];
}

const adminStorage = getStorage(adminApp);

export { adminApp, adminStorage };
