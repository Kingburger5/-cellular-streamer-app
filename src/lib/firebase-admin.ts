
import { initializeApp, getApps, getApp, App as AdminApp } from 'firebase-admin/app';
import { getStorage as getAdminStorage } from 'firebase-admin/storage';

// This function ensures the Admin App is initialized only once.
function initializeAdminAppOnce(): AdminApp {
    if (getApps().length) {
        return getApp();
    }
    
    // When running in an App Hosting environment, the Admin SDK can automatically
    // find the service account credentials. We must explicitly provide the project ID
    // and storage bucket.
    const projectId = process.env.FIREBASE_PROJECT_ID;
    const storageBucket = process.env.GCLOUD_STORAGE_BUCKET;

    if (!projectId || !storageBucket) {
        throw new Error("Firebase Admin SDK configuration error: Project ID or Storage Bucket is not defined in environment variables.");
    }

    return initializeApp({
        projectId: projectId,
        storageBucket: storageBucket,
    });
}

// Initialize the app and export the storage instance for use in server-side code.
const adminApp = initializeAdminAppOnce();
export const adminStorage = getAdminStorage(adminApp);
